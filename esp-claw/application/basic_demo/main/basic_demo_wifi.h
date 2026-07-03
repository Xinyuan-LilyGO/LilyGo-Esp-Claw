/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef void (*basic_demo_wifi_state_cb_t)(bool connected, void *user_ctx);

esp_err_t basic_demo_wifi_init(void);
esp_err_t basic_demo_wifi_start(const char *ssid, const char *password);
esp_err_t basic_demo_wifi_wait_connected(uint32_t timeout_ms);
esp_err_t basic_demo_wifi_register_state_callback(basic_demo_wifi_state_cb_t cb, void *user_ctx);
bool basic_demo_wifi_is_connected(void);
const char *basic_demo_wifi_get_ip(void);

const char *basic_demo_wifi_get_ap_ssid(void);
const char *basic_demo_wifi_get_ap_ip(void);
bool basic_demo_wifi_is_ap_active(void);
const char *basic_demo_wifi_get_mode_string(void);
