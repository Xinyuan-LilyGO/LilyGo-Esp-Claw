/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "basic_demo_wifi.h"

#include <inttypes.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"

static const char *TAG = "basic_demo_wifi";

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_RETRY_BASE_MS  1000
#define WIFI_RETRY_MAX_MS   30000

#ifndef CONFIG_BASIC_DEMO_WIFI_AP_SSID_PREFIX
#define CONFIG_BASIC_DEMO_WIFI_AP_SSID_PREFIX "esp-claw"
#endif
#ifndef CONFIG_BASIC_DEMO_WIFI_AP_CHANNEL
#define CONFIG_BASIC_DEMO_WIFI_AP_CHANNEL 1
#endif
#ifndef CONFIG_BASIC_DEMO_WIFI_AP_MAX_CONN
#define CONFIG_BASIC_DEMO_WIFI_AP_MAX_CONN 4
#endif
#ifndef CONFIG_BASIC_DEMO_WIFI_MAX_RETRY
#define CONFIG_BASIC_DEMO_WIFI_MAX_RETRY 5
#endif

typedef enum {
    WIFI_MODE_OFF = 0,
    WIFI_MODE_PROVISION_AP,   /* AP only, no STA credentials */
    WIFI_MODE_APSTA_TRYING,   /* APSTA, STA still trying */
    WIFI_MODE_APSTA_OK,       /* APSTA, STA connected */
    WIFI_MODE_AP_FALLBACK,    /* STA failed N times, dropped back to pure AP */
} wifi_mode_state_t;

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count;
static bool s_connected;
static bool s_ap_active;
static bool s_sta_configured;
static char s_ip_addr[16] = "0.0.0.0";
static char s_ap_ip[16] = "192.168.4.1";
static char s_ap_ssid[33];
static wifi_mode_state_t s_mode = WIFI_MODE_OFF;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static basic_demo_wifi_state_cb_t s_state_cb;
static void *s_state_cb_user_ctx;
static esp_timer_handle_t s_reconnect_timer;

static void notify_state_changed(bool force);
static esp_err_t fallback_to_ap(void);
static void reconnect_timer_cb(void *arg);

static void compose_ap_ssid(void)
{
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) != ESP_OK) {
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
    }
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "%s-%02X%02X%02X",
             CONFIG_BASIC_DEMO_WIFI_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Provisioning AP SSID: %s", s_ap_ssid);
}

static void apply_ap_config(void)
{
    wifi_config_t ap_cfg = {0};
    strlcpy((char *)ap_cfg.ap.ssid, s_ap_ssid, sizeof(ap_cfg.ap.ssid));
    ap_cfg.ap.ssid_len = strlen(s_ap_ssid);
    ap_cfg.ap.channel = CONFIG_BASIC_DEMO_WIFI_AP_CHANNEL;
    ap_cfg.ap.max_connection = CONFIG_BASIC_DEMO_WIFI_AP_MAX_CONN;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
}

static void refresh_ap_ip_str(void)
{
    if (!s_ap_netif) {
        return;
    }
    esp_netif_ip_info_t ip_info = {0};
    if (esp_netif_get_ip_info(s_ap_netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        snprintf(s_ap_ip, sizeof(s_ap_ip), IPSTR, IP2STR(&ip_info.ip));
    }
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            return;

        case WIFI_EVENT_STA_DISCONNECTED:
            strlcpy(s_ip_addr, "0.0.0.0", sizeof(s_ip_addr));
            if (s_connected) {
                s_connected = false;
                notify_state_changed(false);
            }
            if (!s_sta_configured) {
                return;
            }
            if (s_retry_count < CONFIG_BASIC_DEMO_WIFI_MAX_RETRY) {
                uint32_t delay_ms = WIFI_RETRY_BASE_MS << s_retry_count;
                if (delay_ms > WIFI_RETRY_MAX_MS) {
                    delay_ms = WIFI_RETRY_MAX_MS;
                }
                s_retry_count++;
                s_mode = s_ap_active ? WIFI_MODE_APSTA_TRYING : s_mode;
                ESP_LOGI(TAG, "STA disconnected, retry %d/%d in %" PRIu32 "ms",
                         s_retry_count, CONFIG_BASIC_DEMO_WIFI_MAX_RETRY, delay_ms);
                if (s_reconnect_timer) {
                    esp_timer_stop(s_reconnect_timer);
                    esp_timer_start_once(s_reconnect_timer,
                                         (uint64_t)delay_ms * 1000ULL);
                } else {
                    esp_wifi_connect();
                }
            } else {
                ESP_LOGE(TAG, "STA failed after %d retries, falling back to AP",
                         CONFIG_BASIC_DEMO_WIFI_MAX_RETRY);
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                fallback_to_ap();
            }
            return;

        case WIFI_EVENT_AP_START:
            s_ap_active = true;
            refresh_ap_ip_str();
            ESP_LOGW(TAG, "*** Provisioning AP active: %s @ %s ***",
                     s_ap_ssid, s_ap_ip);
            notify_state_changed(true);
            return;

        case WIFI_EVENT_AP_STOP:
            s_ap_active = false;
            ESP_LOGW(TAG, "Provisioning AP stopped");
            notify_state_changed(true);
            return;

        case WIFI_EVENT_AP_STACONNECTED: {
            wifi_event_ap_staconnected_t *ev = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "AP client connected: " MACSTR " aid=%d",
                     MAC2STR(ev->mac), ev->aid);
            return;
        }

        case WIFI_EVENT_AP_STADISCONNECTED: {
            wifi_event_ap_stadisconnected_t *ev = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "AP client disconnected: " MACSTR " aid=%d",
                     MAC2STR(ev->mac), ev->aid);
            return;
        }

        default:
            return;
        }
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_ip_addr, sizeof(s_ip_addr), IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        s_connected = true;
        s_mode = s_ap_active ? WIFI_MODE_APSTA_OK : s_mode;
        ESP_LOGI(TAG, "STA connected, IP=%s", s_ip_addr);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        notify_state_changed(true);
    }
}

static void notify_state_changed(bool force)
{
    static bool s_last_connected;
    static bool s_last_ap_active;
    static bool s_initialized;

    if (!force && s_initialized && s_last_connected == s_connected &&
            s_last_ap_active == s_ap_active) {
        return;
    }

    s_last_connected = s_connected;
    s_last_ap_active = s_ap_active;
    s_initialized = true;

    if (s_state_cb) {
        s_state_cb(s_connected, s_state_cb_user_ctx);
    }
}

static void reconnect_timer_cb(void *arg)
{
    (void)arg;
    if (!s_sta_configured) {
        return;
    }
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_CONN) {
        ESP_LOGW(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
    }
}

static esp_err_t fallback_to_ap(void)
{
    s_mode = WIFI_MODE_AP_FALLBACK;
    s_sta_configured = false;
    s_retry_count = 0;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallback set_mode failed: %s", esp_err_to_name(err));
        return err;
    }
    apply_ap_config();
    refresh_ap_ip_str();
    ESP_LOGW(TAG, "*** Reprovision required, AP active: %s @ %s ***",
             s_ap_ssid, s_ap_ip);
    notify_state_changed(true);
    return ESP_OK;
}

esp_err_t basic_demo_wifi_init(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    const esp_timer_create_args_t timer_args = {
        .callback = reconnect_timer_cb,
        .name = "wifi_reconnect",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_reconnect_timer));

    compose_ap_ssid();
    return ESP_OK;
}

esp_err_t basic_demo_wifi_start(const char *ssid, const char *password)
{
    s_sta_configured = (ssid && ssid[0] != '\0');
    s_retry_count = 0;
    if (s_reconnect_timer) {
        esp_timer_stop(s_reconnect_timer);
    }
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    if (s_sta_configured) {
        wifi_config_t sta_cfg = {0};
        strlcpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
        strlcpy((char *)sta_cfg.sta.password,
                password ? password : "",
                sizeof(sta_cfg.sta.password));
        sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        sta_cfg.sta.pmf_cfg.capable = true;
        sta_cfg.sta.pmf_cfg.required = false;

        s_mode = WIFI_MODE_APSTA_TRYING;
        ESP_LOGI(TAG, "Starting Wi-Fi APSTA, STA target: %s", ssid);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        apply_ap_config();
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    } else {
        s_mode = WIFI_MODE_PROVISION_AP;
        ESP_LOGW(TAG, "No STA credentials, starting Wi-Fi in pure AP mode");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        apply_ap_config();
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}

esp_err_t basic_demo_wifi_wait_connected(uint32_t timeout_ms)
{
    if (!s_sta_configured) {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits;
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);

    bits = xEventGroupWaitBits(s_wifi_event_group,
                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                               pdFALSE,
                               pdFALSE,
                               ticks);
    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t basic_demo_wifi_register_state_callback(basic_demo_wifi_state_cb_t cb, void *user_ctx)
{
    s_state_cb = cb;
    s_state_cb_user_ctx = user_ctx;

    if (s_state_cb) {
        s_state_cb(s_connected, s_state_cb_user_ctx);
    }

    return ESP_OK;
}

bool basic_demo_wifi_is_connected(void)
{
    return s_connected;
}

const char *basic_demo_wifi_get_ip(void)
{
    return s_ip_addr;
}

const char *basic_demo_wifi_get_ap_ssid(void)
{
    return s_ap_ssid;
}

const char *basic_demo_wifi_get_ap_ip(void)
{
    return s_ap_ip;
}

bool basic_demo_wifi_is_ap_active(void)
{
    return s_ap_active;
}

const char *basic_demo_wifi_get_mode_string(void)
{
    switch (s_mode) {
    case WIFI_MODE_PROVISION_AP: return "provision";
    case WIFI_MODE_APSTA_TRYING: return "apsta";
    case WIFI_MODE_APSTA_OK:     return "sta_ok";
    case WIFI_MODE_AP_FALLBACK:  return "fallback";
    default:                     return "off";
    }
}
