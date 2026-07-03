/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "basic_demo_lua_modules.h"

#include "lua_module_delay.h"
#include "lua_module_event_publisher.h"
#include "lua_module_gpio.h"
#include "lua_module_led_strip.h"
#include "lua_module_storage.h"
#include "lua_module_button.h"
#include "lua_module_esp_heap.h"
#include "lua_module_system.h"
#include "lua_module_board_manager.h"
#include "lua_module_mcpwm.h"

#if defined(CONFIG_ESP_BOARD_DEV_AUDIO_CODEC_SUPPORT)
#include "lua_module_audio.h"
#endif
#include "lua_module_display.h"
#if defined(CONFIG_ESP_BOARD_DEV_LCD_TOUCH_I2C_SUPPORT)
#include "lua_module_lcd_touch.h"
#endif
#if defined(CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT)
#include "lua_module_camera.h"
#endif

extern const char *basic_demo_fatfs_base_path;

esp_err_t basic_demo_lua_modules_register(void)
{
    esp_err_t err;

    err = lua_module_delay_register();
    if (err != ESP_OK) {
        return err;
    }

    err = lua_module_storage_register(basic_demo_fatfs_base_path);
    if (err != ESP_OK) {
        return err;
    }

    err = lua_module_gpio_register();
    if (err != ESP_OK) {
        return err;
    }

    err = lua_module_led_strip_register();
    if (err != ESP_OK) {
        return err;
    }

#if defined(CONFIG_ESP_BOARD_DEV_AUDIO_CODEC_SUPPORT)
    err = lua_module_audio_register();
    if (err != ESP_OK) {
        return err;
    }
#endif

    err = lua_module_button_register();
    if (err != ESP_OK) {
        return err;
    }

    err = lua_module_display_register();
    if (err != ESP_OK) {
        return err;
    }

    err = lua_module_board_manager_register();
    if (err != ESP_OK) {
        return err;
    }

#if defined(CONFIG_ESP_BOARD_DEV_LCD_TOUCH_I2C_SUPPORT)
    err = lua_module_lcd_touch_register();
    if (err != ESP_OK) {
        return err;
    }
#endif

#if defined(CONFIG_ESP_BOARD_DEV_CAMERA_SUPPORT)
    err = lua_module_camera_register();
    if (err != ESP_OK) {
        return err;
    }
#endif

    err = lua_module_esp_heap_register();
    if (err != ESP_OK) {
        return err;
    }

    err = lua_module_system_register();
    if (err != ESP_OK) {
        return err;
    }

    err = lua_module_mcpwm_register();
    if (err != ESP_OK) {
        return err;
    }

    return lua_module_event_publisher_register();
}
