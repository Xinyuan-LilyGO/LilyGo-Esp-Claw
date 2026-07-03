/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lua_module_delay.h"

#include <stdint.h>

#include "cap_lua.h"
#include "lauxlib.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static int lua_module_delay_sleep_ms(lua_State *L)
{
    lua_Integer ms = luaL_checkinteger(L, 1);

    if (ms < 0) {
        ms = 0;
    }

    vTaskDelay(pdMS_TO_TICKS((uint32_t)ms));
    return 0;
}

int luaopen_delay(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, lua_module_delay_sleep_ms);
    lua_setfield(L, -2, "delay_ms");
    return 1;
}

esp_err_t lua_module_delay_register(void)
{
    return cap_lua_register_module("delay", luaopen_delay);
}
