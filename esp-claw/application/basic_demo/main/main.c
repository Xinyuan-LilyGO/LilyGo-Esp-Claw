/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_claw.h"
#if defined(CONFIG_BASIC_DEMO_ENABLE_EMOTE)
#include "app_expression_emote.h"
#endif
#include "basic_demo_settings.h"
#include "basic_demo_wifi.h"
#include "cap_lua.h"
#include "captive_dns.h"
#include "claw_skill.h"
#include "config_http_server.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include "time.h"
#include "esp_vfs_fat.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wear_levelling.h"
#include "esp_board_manager_includes.h"

static const char *TAG = "basic_demo";
static basic_demo_settings_t s_settings = {0};

const char *basic_demo_fatfs_base_path = "/fatfs";
#define BASIC_DEMO_FATFS_PARTITION_LABEL "storage"
#define BASIC_DEMO_ENABLE_MEM_LOG        (0)

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

static esp_err_t cap_lua_run_deactivate_guard(const char *session_id,
                                              const char *skill_id,
                                              char *reason_out,
                                              size_t reason_size)
{
    (void)session_id;
    (void)skill_id;

    size_t active = cap_lua_get_active_async_job_count();
    if (active == 0) {
        return ESP_OK;
    }
    if (reason_out && reason_size > 0) {
        if (active == SIZE_MAX) {
            snprintf(reason_out, reason_size,
                     "Lua async runner is busy (lock contended). "
                     "Call lua_list_async_jobs to confirm, then "
                     "lua_stop_all_async_jobs before retrying deactivate_skill.");
        } else {
            snprintf(reason_out, reason_size,
                     "%u Lua async job(s) still running. "
                     "Call lua_stop_all_async_jobs (or lua_stop_async_job per id/name) "
                     "first, then retry deactivate_skill.",
                     (unsigned)active);
        }
    }
    return ESP_ERR_INVALID_STATE;
}

static void on_wifi_state_changed(bool connected, void *user_ctx)
{
    (void)user_ctx;

    const char *ap_ssid = basic_demo_wifi_is_ap_active()
                          ? basic_demo_wifi_get_ap_ssid()
                          : NULL;

    ESP_LOGI(TAG, "Wi-Fi state: sta_connected=%d ap_active=%d mode=%s ap_ssid=%s",
             connected,
             basic_demo_wifi_is_ap_active(),
             basic_demo_wifi_get_mode_string(),
             ap_ssid ? ap_ssid : "(none)");

#if defined(CONFIG_BASIC_DEMO_ENABLE_EMOTE)
    esp_err_t err = app_expression_emote_set_status(connected, ap_ssid);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to update network emote: %s", esp_err_to_name(err));
    }
#else
    (void)connected;
#endif
}

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

static esp_err_t init_fatfs(void)
{
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 8,
        .allocation_unit_size = 4096,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };
    uint64_t total = 0;
    uint64_t free_bytes = 0;
    esp_err_t err;

    err = esp_vfs_fat_spiflash_mount_rw_wl(basic_demo_fatfs_base_path,
                                           BASIC_DEMO_FATFS_PARTITION_LABEL,
                                           &mount_config,
                                           &s_wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_vfs_fat_info(basic_demo_fatfs_base_path, &total, &free_bytes);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to query FATFS info: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "FATFS mounted total=%u used=%u",
                 (unsigned int)total,
                 (unsigned int)(total - free_bytes));
    }

    return ESP_OK;
}

static esp_err_t init_timezone(const char *timezone)
{
    esp_err_t err = ESP_OK;

    if (!timezone || timezone[0] == '\0') {
        ESP_LOGE(TAG, "Timezone is empty.");
        err = ESP_ERR_INVALID_ARG;
        goto tz_default;
    }

    if (setenv("TZ", timezone, 1) != 0) {
        ESP_LOGE(TAG, "Failed to set TZ env");
        err = ESP_FAIL;
        goto tz_default;
    }
    tzset();
    ESP_LOGI(TAG, "Timezone set to %s", timezone);
    return ESP_OK;

tz_default:
    assert(setenv("TZ", "CST-8", 1) == 0);
    tzset();
    ESP_LOGI(TAG, "Timezone set to default: CST-8");
    return err;
}

#if BASIC_DEMO_ENABLE_MEM_LOG

static void print_task_stack_info(void)
{
#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    static TaskStatus_t s_task_status_snapshot[24];
    UBaseType_t count = uxTaskGetSystemState(s_task_status_snapshot,
                                             sizeof(s_task_status_snapshot) / sizeof(s_task_status_snapshot[0]),
                                             NULL);

    for (UBaseType_t i = 0; i < count; i++) {
        ESP_LOGI(TAG,
                 "Task %s  %u",
                 s_task_status_snapshot[i].pcTaskName,
                 s_task_status_snapshot[i].usStackHighWaterMark);
    }
#endif
}

/* Periodic task: print internal free, minimum free, and PSRAM free every 20s */
static void memory_monitor_task(void *arg)
{
    (void)arg;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        size_t internal_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG, "Memory: internal_free=%u bytes, internal_min_free=%u bytes, psram_free=%u bytes",
                 (unsigned)internal_free, (unsigned)internal_min, (unsigned)psram_free);
        print_task_stack_info();
    }
}

#endif

void app_main(void)
{
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);

    ESP_LOGI(TAG, "Starting basic_demo");
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(basic_demo_settings_init());
    ESP_ERROR_CHECK(basic_demo_settings_load(&s_settings));
    init_timezone(s_settings.time_timezone); // no need to check error
    ESP_ERROR_CHECK(esp_board_manager_init());
#if defined(CONFIG_BASIC_DEMO_ENABLE_EMOTE)
    ESP_ERROR_CHECK(app_expression_emote_start());
#endif
    ESP_ERROR_CHECK(init_fatfs());
    ESP_ERROR_CHECK(basic_demo_wifi_init());
    ESP_ERROR_CHECK(config_http_server_init(basic_demo_fatfs_base_path));
    ESP_ERROR_CHECK(basic_demo_wifi_register_state_callback(on_wifi_state_changed, NULL));

    esp_err_t wifi_err = basic_demo_wifi_start(s_settings.wifi_ssid, s_settings.wifi_password);
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi start failed: %s", esp_err_to_name(wifi_err));
    } else {
        ESP_ERROR_CHECK(config_http_server_start());
        if (captive_dns_start() != ESP_OK) {
            ESP_LOGW(TAG, "Captive DNS could not start, portal pop-up disabled");
        }

        if (s_settings.wifi_ssid[0] != '\0') {
            if (basic_demo_wifi_wait_connected(30000) == ESP_OK) {
                ESP_LOGI(TAG, "Wi-Fi STA ready: %s", basic_demo_wifi_get_ip());
            } else {
                ESP_LOGW(TAG, "STA could not connect, dropped to AP fallback");
            }
        }

        ESP_LOGW(TAG,
                 "*** Provisioning portal: SSID=\"%s\" (open) IP=%s URL=http://%s/ ***",
                 basic_demo_wifi_get_ap_ssid(),
                 basic_demo_wifi_get_ap_ip(),
                 basic_demo_wifi_get_ap_ip());
    }

    ESP_ERROR_CHECK(app_claw_start(&s_settings));

    esp_err_t guard_err = claw_skill_register_deactivate_guard("cap_lua_run",
                                                               cap_lua_run_deactivate_guard);
    if (guard_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register cap_lua_run deactivate guard: %s",
                 esp_err_to_name(guard_err));
    }

#if BASIC_DEMO_ENABLE_MEM_LOG
    /* Start memory monitor: print internal free, min free, PSRAM free every 20s */
    xTaskCreate(memory_monitor_task, "mem_mon", 4096, NULL, 1, NULL);
#endif
}
