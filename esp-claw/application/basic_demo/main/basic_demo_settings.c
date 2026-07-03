/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "basic_demo_settings.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "basic_demo_settings";
static const char *BASIC_DEMO_SETTINGS_NAMESPACE = "basic_demo";

typedef struct {
    const char *key;
    const char *default_value;
    char *buffer;
    size_t buffer_size;
} basic_demo_settings_field_t;

static void settings_safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    strlcpy(dst, src, dst_size);
}

static void settings_load_defaults(basic_demo_settings_t *settings)
{
    settings_safe_copy(settings->wifi_ssid, sizeof(settings->wifi_ssid), BASIC_DEMO_WIFI_SSID);
    settings_safe_copy(settings->wifi_password, sizeof(settings->wifi_password), BASIC_DEMO_WIFI_PASSWORD);
    settings_safe_copy(settings->llm_api_key, sizeof(settings->llm_api_key), BASIC_DEMO_LLM_API_KEY);
    settings_safe_copy(settings->llm_backend_type, sizeof(settings->llm_backend_type), BASIC_DEMO_LLM_BACKEND_TYPE);
    settings_safe_copy(settings->llm_profile, sizeof(settings->llm_profile), BASIC_DEMO_LLM_PROFILE);
    settings_safe_copy(settings->llm_model, sizeof(settings->llm_model), BASIC_DEMO_LLM_MODEL);
    settings_safe_copy(settings->llm_base_url, sizeof(settings->llm_base_url), BASIC_DEMO_LLM_BASE_URL);
    settings_safe_copy(settings->llm_auth_type, sizeof(settings->llm_auth_type), BASIC_DEMO_LLM_AUTH_TYPE);
    settings_safe_copy(settings->llm_timeout_ms, sizeof(settings->llm_timeout_ms), BASIC_DEMO_LLM_TIMEOUT_MS);
    settings_safe_copy(settings->qq_app_id, sizeof(settings->qq_app_id), BASIC_DEMO_QQ_APP_ID);
    settings_safe_copy(settings->qq_app_secret, sizeof(settings->qq_app_secret), BASIC_DEMO_QQ_APP_SECRET);
    settings_safe_copy(settings->feishu_app_id, sizeof(settings->feishu_app_id), BASIC_DEMO_FEISHU_APP_ID);
    settings_safe_copy(settings->feishu_app_secret, sizeof(settings->feishu_app_secret), BASIC_DEMO_FEISHU_APP_SECRET);
    settings_safe_copy(settings->tg_bot_token, sizeof(settings->tg_bot_token), BASIC_DEMO_TG_BOT_TOKEN);
    settings_safe_copy(settings->wechat_token, sizeof(settings->wechat_token), BASIC_DEMO_WECHAT_TOKEN);
    settings_safe_copy(settings->wechat_base_url, sizeof(settings->wechat_base_url), BASIC_DEMO_WECHAT_BASE_URL);
    settings_safe_copy(settings->wechat_cdn_base_url, sizeof(settings->wechat_cdn_base_url), BASIC_DEMO_WECHAT_CDN_BASE_URL);
    settings_safe_copy(settings->wechat_account_id, sizeof(settings->wechat_account_id), BASIC_DEMO_WECHAT_ACCOUNT_ID);
    settings_safe_copy(settings->search_brave_key, sizeof(settings->search_brave_key), BASIC_DEMO_SEARCH_BRAVE_KEY);
    settings_safe_copy(settings->search_tavily_key, sizeof(settings->search_tavily_key), BASIC_DEMO_SEARCH_TAVILY_KEY);
    settings_safe_copy(settings->time_timezone, sizeof(settings->time_timezone), BASIC_DEMO_TIME_TIMEZONE);
}

static esp_err_t settings_open(nvs_open_mode_t mode, nvs_handle_t *handle)
{
    return nvs_open(BASIC_DEMO_SETTINGS_NAMESPACE, mode, handle);
}

esp_err_t basic_demo_settings_init(void)
{
    nvs_handle_t handle;
    esp_err_t err = settings_open(NVS_READONLY, &handle);
    if (err == ESP_OK) {
        nvs_close(handle);
        return ESP_OK;
    }

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = settings_open(NVS_READWRITE, &handle);
        if (err == ESP_OK) {
            nvs_close(handle);
        }
    }

    return err;
}

esp_err_t basic_demo_settings_load(basic_demo_settings_t *settings)
{
    char legacy_provider[32] = {0};

    if (!settings) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(settings, 0, sizeof(*settings));
    settings_load_defaults(settings);

    nvs_handle_t handle;
    esp_err_t err = settings_open(NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    basic_demo_settings_field_t fields[] = {
        { "wifi_ssid", settings->wifi_ssid, settings->wifi_ssid, sizeof(settings->wifi_ssid) },
        { "wifi_password", settings->wifi_password, settings->wifi_password, sizeof(settings->wifi_password) },
        { "llm_api_key", settings->llm_api_key, settings->llm_api_key, sizeof(settings->llm_api_key) },
        { "llm_backend", settings->llm_backend_type, settings->llm_backend_type, sizeof(settings->llm_backend_type) },
        { "llm_profile", settings->llm_profile, settings->llm_profile, sizeof(settings->llm_profile) },
        { "llm_model", settings->llm_model, settings->llm_model, sizeof(settings->llm_model) },
        { "llm_base_url", settings->llm_base_url, settings->llm_base_url, sizeof(settings->llm_base_url) },
        { "llm_auth_type", settings->llm_auth_type, settings->llm_auth_type, sizeof(settings->llm_auth_type) },
        { "llm_timeout_ms", settings->llm_timeout_ms, settings->llm_timeout_ms, sizeof(settings->llm_timeout_ms) },
        { "qq_app_id", settings->qq_app_id, settings->qq_app_id, sizeof(settings->qq_app_id) },
        { "qq_app_secret", settings->qq_app_secret, settings->qq_app_secret, sizeof(settings->qq_app_secret) },
        { "feishu_app_id", settings->feishu_app_id, settings->feishu_app_id, sizeof(settings->feishu_app_id) },
        { "feishu_secret", settings->feishu_app_secret, settings->feishu_app_secret, sizeof(settings->feishu_app_secret) },
        { "tg_bot_token", settings->tg_bot_token, settings->tg_bot_token, sizeof(settings->tg_bot_token) },
        { "wechat_token", settings->wechat_token, settings->wechat_token, sizeof(settings->wechat_token) },
        { "wechat_base_url", settings->wechat_base_url, settings->wechat_base_url, sizeof(settings->wechat_base_url) },
        { "wechat_cdn_url", settings->wechat_cdn_base_url, settings->wechat_cdn_base_url, sizeof(settings->wechat_cdn_base_url) },
        { "wechat_acct_id", settings->wechat_account_id, settings->wechat_account_id, sizeof(settings->wechat_account_id) },
        { "brave_key", settings->search_brave_key, settings->search_brave_key, sizeof(settings->search_brave_key) },
        { "tavily_key", settings->search_tavily_key, settings->search_tavily_key, sizeof(settings->search_tavily_key) },
        { "time_timezone", settings->time_timezone, settings->time_timezone, sizeof(settings->time_timezone) },
    };

    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        size_t required_size = fields[i].buffer_size;
        if (nvs_get_str(handle, fields[i].key, fields[i].buffer, &required_size) != ESP_OK) {
            settings_safe_copy(fields[i].buffer, fields[i].buffer_size, fields[i].default_value);
        }
    }

    {
        size_t required_size = sizeof(legacy_provider);
        if (nvs_get_str(handle, "llm_provider", legacy_provider, &required_size) == ESP_OK &&
                settings->llm_profile[0] == '\0') {
            if (strcmp(legacy_provider, "qwen") == 0) {
                settings_safe_copy(settings->llm_profile, sizeof(settings->llm_profile), "qwen_compatible");
            } else if (strcmp(legacy_provider, "openai") == 0) {
                settings_safe_copy(settings->llm_profile, sizeof(settings->llm_profile), "openai");
            } else {
                settings_safe_copy(settings->llm_profile, sizeof(settings->llm_profile), legacy_provider);
            }
        }
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t basic_demo_settings_save(const basic_demo_settings_t *settings)
{
    if (!settings) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = settings_open(NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    basic_demo_settings_field_t fields[] = {
        { "wifi_ssid", NULL, (char *)settings->wifi_ssid, sizeof(settings->wifi_ssid) },
        { "wifi_password", NULL, (char *)settings->wifi_password, sizeof(settings->wifi_password) },
        { "llm_api_key", NULL, (char *)settings->llm_api_key, sizeof(settings->llm_api_key) },
        { "llm_backend", NULL, (char *)settings->llm_backend_type, sizeof(settings->llm_backend_type) },
        { "llm_profile", NULL, (char *)settings->llm_profile, sizeof(settings->llm_profile) },
        { "llm_model", NULL, (char *)settings->llm_model, sizeof(settings->llm_model) },
        { "llm_base_url", NULL, (char *)settings->llm_base_url, sizeof(settings->llm_base_url) },
        { "llm_auth_type", NULL, (char *)settings->llm_auth_type, sizeof(settings->llm_auth_type) },
        { "llm_timeout_ms", NULL, (char *)settings->llm_timeout_ms, sizeof(settings->llm_timeout_ms) },
        { "qq_app_id", NULL, (char *)settings->qq_app_id, sizeof(settings->qq_app_id) },
        { "qq_app_secret", NULL, (char *)settings->qq_app_secret, sizeof(settings->qq_app_secret) },
        { "feishu_app_id", NULL, (char *)settings->feishu_app_id, sizeof(settings->feishu_app_id) },
        { "feishu_secret", NULL, (char *)settings->feishu_app_secret, sizeof(settings->feishu_app_secret) },
        { "tg_bot_token", NULL, (char *)settings->tg_bot_token, sizeof(settings->tg_bot_token) },
        { "wechat_token", NULL, (char *)settings->wechat_token, sizeof(settings->wechat_token) },
        { "wechat_base_url", NULL, (char *)settings->wechat_base_url, sizeof(settings->wechat_base_url) },
        { "wechat_cdn_url", NULL, (char *)settings->wechat_cdn_base_url, sizeof(settings->wechat_cdn_base_url) },
        { "wechat_acct_id", NULL, (char *)settings->wechat_account_id, sizeof(settings->wechat_account_id) },
        { "brave_key", NULL, (char *)settings->search_brave_key, sizeof(settings->search_brave_key) },
        { "tavily_key", NULL, (char *)settings->search_tavily_key, sizeof(settings->search_tavily_key) },
        { "time_timezone", NULL, (char *)settings->time_timezone, sizeof(settings->time_timezone) },
    };

    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        err = nvs_set_str(handle, fields[i].key, fields[i].buffer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "nvs_set_str(%s) failed: %s", fields[i].key, esp_err_to_name(err));
            nvs_close(handle);
            return err;
        }
    }

    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
    }
    nvs_close(handle);
    return err;
}
