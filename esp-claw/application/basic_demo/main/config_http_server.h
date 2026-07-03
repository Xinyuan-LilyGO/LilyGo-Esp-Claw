/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifndef BASIC_DEMO_HTTP_SERVER_PORT
#define BASIC_DEMO_HTTP_SERVER_PORT 80
#endif

esp_err_t config_http_server_init(const char *storage_base_path);
esp_err_t config_http_server_start(void);
esp_err_t config_http_server_stop(void);
