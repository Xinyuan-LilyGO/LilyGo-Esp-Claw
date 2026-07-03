/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cap_time.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "claw_cap.h"
#include "claw_task.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "cap_time";

#define CAP_TIME_SOURCE_URL     "https://api.telegram.org/"
#define CAP_TIME_HTTP_TIMEOUT_MS 10000
#define CAP_TIME_MIN_VALID_EPOCH 1704067200
#define CAP_TIME_UTC_TIMEZONE "UTC0"
#define CAP_TIME_DEFAULT_DISCONNECTED_RETRY_MS 5000
#define CAP_TIME_DEFAULT_SYNC_RETRY_MS        30000

static const char *s_month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

typedef struct {
    char date_header[64];
} cap_time_state_t;

static cap_time_state_t s_time = {0};
static SemaphoreHandle_t s_time_mutex = NULL;
static struct {
    TaskHandle_t task_handle;
    cap_time_sync_service_config_t config;
    bool running;
} s_time_service = {0};

static esp_err_t cap_time_ensure_mutex(void)
{
    if (!s_time_mutex) {
        s_time_mutex = xSemaphoreCreateMutex();
    }
    return s_time_mutex ? ESP_OK : ESP_ERR_NO_MEM;
}

static int cap_time_month_to_index(const char *month)
{
    size_t i;

    if (!month) {
        return -1;
    }

    for (i = 0; i < sizeof(s_month_names) / sizeof(s_month_names[0]); i++) {
        if (strcmp(month, s_month_names[i]) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static esp_err_t cap_time_read_timezone(char *timezone, size_t timezone_size)
{
    const char *configured_tz = NULL;

    if (!timezone || timezone_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    configured_tz = getenv("TZ");
    if (configured_tz && configured_tz[0]) {
        strlcpy(timezone, configured_tz, timezone_size);
        return ESP_OK;
    }

    return ESP_ERR_INVALID_STATE;
}

static bool cap_time_parse_and_set_clock(const char *date_header,
                                         char *output,
                                         size_t output_size)
{
    int day = 0;
    int year = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int month = -1;
    char month_name[4] = {0};
    struct tm utc_tm = {0};
    struct timeval tv = {0};
    struct tm local_tm = {0};
    char local_tz[64] = {0};
    bool ok = false;
    time_t epoch;

    if (!date_header || !output || output_size == 0) {
        return false;
    }

    if (sscanf(date_header,
               "%*[^,], %d %3s %d %d:%d:%d",
               &day,
               month_name,
               &year,
               &hour,
               &minute,
               &second) != 6) {
        return false;
    }

    month = cap_time_month_to_index(month_name);
    if (month < 0) {
        return false;
    }

    utc_tm.tm_mday = day;
    utc_tm.tm_mon = month;
    utc_tm.tm_year = year - 1900;
    utc_tm.tm_hour = hour;
    utc_tm.tm_min = minute;
    utc_tm.tm_sec = second;

    if (cap_time_read_timezone(local_tz, sizeof(local_tz)) != ESP_OK) {
        return false;
    }

    if (setenv("TZ", CAP_TIME_UTC_TIMEZONE, 1) != 0) {
        return false;
    }
    tzset();
    epoch = mktime(&utc_tm);
    if (epoch < 0) {
        goto done;
    }

    tv.tv_sec = epoch;
    if (settimeofday(&tv, NULL) != 0) {
        goto done;
    }

    setenv("TZ", local_tz, 1);
    tzset();
    localtime_r(&epoch, &local_tm);
    if (strftime(output, output_size, "%Y-%m-%d %H:%M:%S %Z (%A)", &local_tm) == 0) {
        goto done;
    }

    ok = true;

done:
    setenv("TZ", local_tz, 1);
    tzset();
    return ok;
}

static esp_err_t cap_time_http_event_handler(esp_http_client_event_t *event)
{
    if (!event || !event->user_data) {
        return ESP_OK;
    }

    if (event->event_id == HTTP_EVENT_ON_HEADER &&
            event->header_key &&
            event->header_value &&
            strcasecmp(event->header_key, "Date") == 0) {
        cap_time_state_t *state = (cap_time_state_t *)event->user_data;

        strlcpy(state->date_header, event->header_value, sizeof(state->date_header));
    }

    return ESP_OK;
}

static esp_err_t cap_time_fetch_current(char *output, size_t output_size)
{
    esp_http_client_config_t config = {
        .url = CAP_TIME_SOURCE_URL,
        .method = HTTP_METHOD_HEAD,
        .timeout_ms = CAP_TIME_HTTP_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = cap_time_http_event_handler,
        .user_data = &s_time,
    };
    esp_http_client_handle_t client = NULL;
    esp_err_t err;

    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_time.date_header[0] = '\0';
    client = esp_http_client_init(&config);
    if (!client) {
        return ESP_FAIL;
    }

    err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) {
        return err;
    }

    if (s_time.date_header[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    if (!cap_time_parse_and_set_clock(s_time.date_header, output, output_size)) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t cap_time_sync_now(char *output, size_t output_size)
{
    esp_err_t err;

    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_RETURN_ON_ERROR(cap_time_ensure_mutex(), TAG, "Failed to create time mutex");
    xSemaphoreTake(s_time_mutex, portMAX_DELAY);
    err = cap_time_fetch_current(output, output_size);
    xSemaphoreGive(s_time_mutex);
    return err;
}

esp_err_t cap_time_get_timezone(char *timezone, size_t timezone_size)
{
    esp_err_t err;

    if (!timezone || timezone_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(cap_time_ensure_mutex(), TAG, "Failed to create time mutex");
    xSemaphoreTake(s_time_mutex, portMAX_DELAY);
    err = cap_time_read_timezone(timezone, timezone_size);
    xSemaphoreGive(s_time_mutex);
    return err;
}

bool cap_time_is_valid(void)
{
    time_t now = time(NULL);

    return now >= CAP_TIME_MIN_VALID_EPOCH;
}

static bool cap_time_network_ready(void)
{
    if (!s_time_service.config.network_ready) {
        return true;
    }

    return s_time_service.config.network_ready(s_time_service.config.network_ready_ctx);
}

static void cap_time_notify_sync_success(bool had_valid_time)
{
    if (s_time_service.config.on_sync_success) {
        s_time_service.config.on_sync_success(had_valid_time,
                                              s_time_service.config.on_sync_success_ctx);
    }
}

static void cap_time_sync_service_task(void *arg)
{
    char output[256];

    (void)arg;

    while (s_time_service.running) {
        bool time_valid;
        uint32_t delay_ms;

        if (!cap_time_network_ready()) {
            delay_ms = s_time_service.config.disconnected_retry_ms;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            continue;
        }

        time_valid = cap_time_is_valid();
        if (!time_valid) {
            bool had_valid_time = false;
            esp_err_t err;

            output[0] = '\0';
            err = cap_time_sync_now(output, sizeof(output));
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Time sync succeeded: %s", output);
                cap_time_notify_sync_success(had_valid_time);
                break;
            }

            ESP_LOGW(TAG, "Time sync failed: %s", esp_err_to_name(err));
            delay_ms = s_time_service.config.sync_retry_ms;
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            continue;
        }

        cap_time_notify_sync_success(false);
        break;
    }

    s_time_service.running = false;
    s_time_service.task_handle = NULL;
    claw_task_delete(NULL);
}

static esp_err_t cap_time_execute(const char *input_json,
                                  const claw_cap_call_context_t *ctx,
                                  char *output,
                                  size_t output_size)
{
    esp_err_t err;

    (void)input_json;
    (void)ctx;

    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    err = cap_time_sync_now(output, output_size);
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: failed to fetch time (%s)", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", output);
        return err;
    }

    ESP_LOGI(TAG, "Time synced: %s", output);
    return ESP_OK;
}

static const claw_cap_descriptor_t s_time_descriptors[] = {
    {
        .id = "get_current_time",
        .name = "get_current_time",
        .family = "system",
        .description = "Fetch current network time, update the local clock, and return formatted local time.",
        .kind = CLAW_CAP_KIND_CALLABLE,
        .cap_flags = CLAW_CAP_FLAG_CALLABLE_BY_LLM,
        .input_schema_json = "{\"type\":\"object\",\"properties\":{}}",
        .execute = cap_time_execute,
    },
};

static const claw_cap_group_t s_time_group = {
    .group_id = "cap_time",
    .descriptors = s_time_descriptors,
    .descriptor_count = sizeof(s_time_descriptors) / sizeof(s_time_descriptors[0]),
};

esp_err_t cap_time_register_group(void)
{
    if (claw_cap_group_exists(s_time_group.group_id)) {
        return ESP_OK;
    }

    return claw_cap_register_group(&s_time_group);
}

esp_err_t cap_time_sync_service_start(const cap_time_sync_service_config_t *config)
{
    BaseType_t ok;

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_time_service.task_handle || s_time_service.running) {
        return ESP_OK;
    }

    memset(&s_time_service.config, 0, sizeof(s_time_service.config));
    s_time_service.config = *config;
    if (s_time_service.config.disconnected_retry_ms == 0) {
        s_time_service.config.disconnected_retry_ms = CAP_TIME_DEFAULT_DISCONNECTED_RETRY_MS;
    }
    if (s_time_service.config.sync_retry_ms == 0) {
        s_time_service.config.sync_retry_ms = CAP_TIME_DEFAULT_SYNC_RETRY_MS;
    }

    if (cap_time_is_valid()) {
        cap_time_notify_sync_success(false);
        return ESP_OK;
    }

    s_time_service.running = true;

    ok = claw_task_create(&(claw_task_config_t){
                              .name = "cap_time_sync",
                              .stack_size = 4096,
                              .priority = 5,
                              .core_id = tskNO_AFFINITY,
                              .stack_policy = CLAW_TASK_STACK_PREFER_PSRAM,
                          },
                          cap_time_sync_service_task,
                          NULL,
                          &s_time_service.task_handle);
    if (ok != pdPASS || !s_time_service.task_handle) {
        s_time_service.running = false;
        s_time_service.task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t cap_time_sync_service_stop(void)
{
    TaskHandle_t task = s_time_service.task_handle;

    s_time_service.running = false;
    if (!task) {
        return ESP_OK;
    }

    while (s_time_service.task_handle == task) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return ESP_OK;
}
