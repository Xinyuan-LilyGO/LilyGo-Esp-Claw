/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "basic_demo_settings.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_claw_start(const basic_demo_settings_t *settings);
extern const char *basic_demo_fatfs_base_path;

#ifdef __cplusplus
}
#endif
