/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#define BASIC_DEMO_SETTINGS_STR_LEN        320
#define BASIC_DEMO_SETTINGS_TIMEZONE_LEN   32

#define BASIC_DEMO_WIFI_SSID         CONFIG_BASIC_DEMO_WIFI_SSID
#define BASIC_DEMO_WIFI_PASSWORD     CONFIG_BASIC_DEMO_WIFI_PASSWORD
#define BASIC_DEMO_LLM_API_KEY       CONFIG_BASIC_DEMO_LLM_API_KEY
#define BASIC_DEMO_LLM_BACKEND_TYPE  CONFIG_BASIC_DEMO_LLM_BACKEND_TYPE
#define BASIC_DEMO_LLM_PROFILE       CONFIG_BASIC_DEMO_LLM_PROFILE
#define BASIC_DEMO_LLM_MODEL         CONFIG_BASIC_DEMO_LLM_MODEL
#define BASIC_DEMO_LLM_BASE_URL      CONFIG_BASIC_DEMO_LLM_BASE_URL
#define BASIC_DEMO_LLM_AUTH_TYPE     CONFIG_BASIC_DEMO_LLM_AUTH_TYPE
#define BASIC_DEMO_LLM_TIMEOUT_MS    CONFIG_BASIC_DEMO_LLM_TIMEOUT_MS
#define BASIC_DEMO_QQ_APP_ID         CONFIG_BASIC_DEMO_QQ_APP_ID
#define BASIC_DEMO_QQ_APP_SECRET     CONFIG_BASIC_DEMO_QQ_APP_SECRET
#define BASIC_DEMO_FEISHU_APP_ID     CONFIG_BASIC_DEMO_FEISHU_APP_ID
#define BASIC_DEMO_FEISHU_APP_SECRET CONFIG_BASIC_DEMO_FEISHU_APP_SECRET
#define BASIC_DEMO_TG_BOT_TOKEN      CONFIG_BASIC_DEMO_TG_BOT_TOKEN
#define BASIC_DEMO_WECHAT_TOKEN      CONFIG_BASIC_DEMO_WECHAT_TOKEN
#define BASIC_DEMO_WECHAT_BASE_URL   CONFIG_BASIC_DEMO_WECHAT_BASE_URL
#define BASIC_DEMO_WECHAT_CDN_BASE_URL CONFIG_BASIC_DEMO_WECHAT_CDN_BASE_URL
#define BASIC_DEMO_WECHAT_ACCOUNT_ID CONFIG_BASIC_DEMO_WECHAT_ACCOUNT_ID
#define BASIC_DEMO_SEARCH_BRAVE_KEY  CONFIG_BASIC_DEMO_SEARCH_BRAVE_KEY
#define BASIC_DEMO_SEARCH_TAVILY_KEY CONFIG_BASIC_DEMO_SEARCH_TAVILY_KEY
#define BASIC_DEMO_TIME_TIMEZONE     CONFIG_BASIC_DEMO_TIME_TIMEZONE

typedef struct {
    char wifi_ssid[BASIC_DEMO_SETTINGS_STR_LEN];
    char wifi_password[BASIC_DEMO_SETTINGS_STR_LEN];
    char llm_api_key[BASIC_DEMO_SETTINGS_STR_LEN];
    char llm_backend_type[32];
    char llm_profile[32];
    char llm_model[64];
    char llm_base_url[BASIC_DEMO_SETTINGS_STR_LEN];
    char llm_auth_type[32];
    char llm_timeout_ms[16];
    char qq_app_id[32];
    char qq_app_secret[BASIC_DEMO_SETTINGS_STR_LEN];
    char feishu_app_id[64];
    char feishu_app_secret[BASIC_DEMO_SETTINGS_STR_LEN];
    char tg_bot_token[BASIC_DEMO_SETTINGS_STR_LEN];
    char wechat_token[BASIC_DEMO_SETTINGS_STR_LEN];
    char wechat_base_url[BASIC_DEMO_SETTINGS_STR_LEN];
    char wechat_cdn_base_url[BASIC_DEMO_SETTINGS_STR_LEN];
    char wechat_account_id[32];
    char search_brave_key[BASIC_DEMO_SETTINGS_STR_LEN];
    char search_tavily_key[BASIC_DEMO_SETTINGS_STR_LEN];
    char time_timezone[BASIC_DEMO_SETTINGS_TIMEZONE_LEN];
} basic_demo_settings_t;

esp_err_t basic_demo_settings_init(void);
esp_err_t basic_demo_settings_load(basic_demo_settings_t *settings);
esp_err_t basic_demo_settings_save(const basic_demo_settings_t *settings);
