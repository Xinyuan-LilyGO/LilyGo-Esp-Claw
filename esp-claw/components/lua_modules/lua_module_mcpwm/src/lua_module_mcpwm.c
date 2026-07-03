/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lua_module_mcpwm.h"

#include <stdbool.h>
#include <stdint.h>

#include "cap_lua.h"
#include "driver/mcpwm_prelude.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lauxlib.h"

#define LUA_MODULE_MCPWM_METATABLE "mcpwm"
#define LUA_MODULE_MCPWM_DEFAULT_GROUP_ID 0
#define LUA_MODULE_MCPWM_DEFAULT_RESOLUTION_HZ 1000000U
#define LUA_MODULE_MCPWM_DEFAULT_FREQUENCY_HZ 1000U
#define LUA_MODULE_MCPWM_DEFAULT_DUTY_PERCENT 50.0

static const char *TAG = "lua_module_mcpwm";

typedef struct {
    int gpio;
    int group_id;
    uint32_t resolution_hz;
    uint32_t frequency_hz;
    double duty_percent;
    bool invert;
} lua_module_mcpwm_config_t;

typedef struct {
    mcpwm_timer_handle_t timer;
    mcpwm_oper_handle_t oper;
    mcpwm_cmpr_handle_t comparator;
    mcpwm_gen_handle_t generator;
    lua_module_mcpwm_config_t config;
    uint32_t period_ticks;
    bool timer_enabled;
    bool running;
} lua_module_mcpwm_ud_t;

static lua_module_mcpwm_ud_t *lua_module_mcpwm_get_ud(lua_State *L, int idx)
{
    lua_module_mcpwm_ud_t *ud =
        (lua_module_mcpwm_ud_t *)luaL_checkudata(L, idx, LUA_MODULE_MCPWM_METATABLE);
    if (!ud || !ud->timer) {
        luaL_error(L, "mcpwm: invalid or closed handle");
    }
    return ud;
}

static esp_err_t lua_module_mcpwm_calc_period_ticks(uint32_t resolution_hz,
                                                    uint32_t frequency_hz,
                                                    uint32_t *period_ticks)
{
    ESP_RETURN_ON_FALSE(frequency_hz > 0, ESP_ERR_INVALID_ARG, TAG, "frequency must be > 0");
    ESP_RETURN_ON_FALSE(resolution_hz >= frequency_hz, ESP_ERR_INVALID_ARG, TAG,
                        "resolution must be >= frequency");

    uint32_t ticks = resolution_hz / frequency_hz;
    ESP_RETURN_ON_FALSE(ticks > 0, ESP_ERR_INVALID_ARG, TAG, "period ticks must be > 0");

    *period_ticks = ticks;
    return ESP_OK;
}

static esp_err_t lua_module_mcpwm_apply_generator_actions(lua_module_mcpwm_ud_t *ud)
{
    ESP_RETURN_ON_ERROR(mcpwm_generator_set_action_on_timer_event(
                            ud->generator,
                            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                                         MCPWM_TIMER_EVENT_EMPTY,
                                                         MCPWM_GEN_ACTION_HIGH)),
                        TAG, "set timer action failed");
    ESP_RETURN_ON_ERROR(mcpwm_generator_set_action_on_compare_event(
                            ud->generator,
                            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                                           ud->comparator,
                                                           MCPWM_GEN_ACTION_LOW)),
                        TAG, "set compare action failed");
    return ESP_OK;
}

static esp_err_t lua_module_mcpwm_apply_duty(lua_module_mcpwm_ud_t *ud)
{
    int force_level = -1;
    if (ud->config.duty_percent <= 0.0) {
        force_level = 0;
    } else if (ud->config.duty_percent >= 100.0) {
        force_level = 1;
    }

    if (force_level >= 0) {
        ESP_RETURN_ON_ERROR(mcpwm_generator_set_force_level(ud->generator, force_level, true),
                            TAG, "force level failed");
        return ESP_OK;
    }

    uint32_t compare_ticks =
        (uint32_t)((((double)ud->period_ticks * ud->config.duty_percent) / 100.0) + 0.5);
    if (compare_ticks == 0) {
        compare_ticks = 1;
    }
    if (compare_ticks >= ud->period_ticks) {
        compare_ticks = ud->period_ticks - 1;
    }

    ESP_RETURN_ON_ERROR(mcpwm_generator_set_force_level(ud->generator, -1, false),
                        TAG, "clear force level failed");
    ESP_RETURN_ON_ERROR(mcpwm_comparator_set_compare_value(ud->comparator, compare_ticks),
                        TAG, "set compare failed");
    return ESP_OK;
}

static esp_err_t lua_module_mcpwm_destroy(lua_module_mcpwm_ud_t *ud)
{
    esp_err_t first_err = ESP_OK;
    esp_err_t err;

    if (!ud) {
        return ESP_OK;
    }

    if (ud->running && ud->generator) {
        err = mcpwm_generator_set_force_level(ud->generator, 0, true);
        if (first_err == ESP_OK && err != ESP_OK) {
            first_err = err;
        }
        ud->running = false;
    }

    if (ud->timer_enabled && ud->timer) {
        err = mcpwm_timer_disable(ud->timer);
        if (first_err == ESP_OK && err != ESP_OK) {
            first_err = err;
        }
        ud->timer_enabled = false;
    }

    if (ud->generator) {
        err = mcpwm_del_generator(ud->generator);
        if (first_err == ESP_OK && err != ESP_OK) {
            first_err = err;
        }
        ud->generator = NULL;
    }

    if (ud->comparator) {
        err = mcpwm_del_comparator(ud->comparator);
        if (first_err == ESP_OK && err != ESP_OK) {
            first_err = err;
        }
        ud->comparator = NULL;
    }

    if (ud->oper) {
        err = mcpwm_del_operator(ud->oper);
        if (first_err == ESP_OK && err != ESP_OK) {
            first_err = err;
        }
        ud->oper = NULL;
    }

    if (ud->timer) {
        err = mcpwm_del_timer(ud->timer);
        if (first_err == ESP_OK && err != ESP_OK) {
            first_err = err;
        }
        ud->timer = NULL;
    }

    return first_err;
}

static esp_err_t lua_module_mcpwm_create(lua_module_mcpwm_ud_t *ud)
{
    esp_err_t err;

    err = lua_module_mcpwm_calc_period_ticks(ud->config.resolution_hz,
                                             ud->config.frequency_hz,
                                             &ud->period_ticks);
    if (err != ESP_OK) {
        return err;
    }

    mcpwm_timer_config_t timer_config = {
        .group_id = ud->config.group_id,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = ud->config.resolution_hz,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = ud->period_ticks,
        .intr_priority = 0,
        .flags = {
            .update_period_on_empty = 1,
        },
    };
    err = mcpwm_new_timer(&timer_config, &ud->timer);
    if (err != ESP_OK) {
        goto fail;
    }

    mcpwm_operator_config_t operator_config = {
        .group_id = ud->config.group_id,
        .intr_priority = 0,
    };
    err = mcpwm_new_operator(&operator_config, &ud->oper);
    if (err != ESP_OK) {
        goto fail;
    }

    err = mcpwm_operator_connect_timer(ud->oper, ud->timer);
    if (err != ESP_OK) {
        goto fail;
    }

    mcpwm_comparator_config_t comparator_config = {
        .intr_priority = 0,
        .flags = {
            .update_cmp_on_tez = 1,
        },
    };
    err = mcpwm_new_comparator(ud->oper, &comparator_config, &ud->comparator);
    if (err != ESP_OK) {
        goto fail;
    }

    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = ud->config.gpio,
        .flags = {
            .invert_pwm = ud->config.invert,
        },
    };
    err = mcpwm_new_generator(ud->oper, &generator_config, &ud->generator);
    if (err != ESP_OK) {
        goto fail;
    }

    err = lua_module_mcpwm_apply_generator_actions(ud);
    if (err != ESP_OK) {
        goto fail;
    }

    err = lua_module_mcpwm_apply_duty(ud);
    if (err != ESP_OK) {
        goto fail;
    }

    err = mcpwm_timer_enable(ud->timer);
    if (err != ESP_OK) {
        goto fail;
    }
    ud->timer_enabled = true;

    return ESP_OK;

fail:
    lua_module_mcpwm_destroy(ud);
    return err;
}

static void lua_module_mcpwm_parse_config(lua_State *L, int idx, lua_module_mcpwm_config_t *config)
{
    luaL_checktype(L, idx, LUA_TTABLE);

    config->group_id = LUA_MODULE_MCPWM_DEFAULT_GROUP_ID;
    config->resolution_hz = LUA_MODULE_MCPWM_DEFAULT_RESOLUTION_HZ;
    config->frequency_hz = LUA_MODULE_MCPWM_DEFAULT_FREQUENCY_HZ;
    config->duty_percent = LUA_MODULE_MCPWM_DEFAULT_DUTY_PERCENT;
    config->invert = false;

    lua_getfield(L, idx, "gpio");
    if (lua_isnil(L, -1)) {
        luaL_error(L, "mcpwm.new: gpio is required");
    }
    config->gpio = (int)luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "group_id");
    if (!lua_isnil(L, -1)) {
        config->group_id = (int)luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "resolution_hz");
    if (!lua_isnil(L, -1)) {
        config->resolution_hz = (uint32_t)luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "frequency_hz");
    if (!lua_isnil(L, -1)) {
        config->frequency_hz = (uint32_t)luaL_checkinteger(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "duty_percent");
    if (!lua_isnil(L, -1)) {
        config->duty_percent = luaL_checknumber(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "invert");
    if (!lua_isnil(L, -1)) {
        config->invert = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    if (config->group_id < 0) {
        luaL_error(L, "mcpwm.new: group_id must be >= 0");
    }
    if (config->resolution_hz == 0) {
        luaL_error(L, "mcpwm.new: resolution_hz must be > 0");
    }
    if (config->frequency_hz == 0) {
        luaL_error(L, "mcpwm.new: frequency_hz must be > 0");
    }
    if (config->duty_percent < 0.0 || config->duty_percent > 100.0) {
        luaL_error(L, "mcpwm.new: duty_percent must be between 0 and 100");
    }
    if (config->resolution_hz < config->frequency_hz) {
        luaL_error(L, "mcpwm.new: resolution_hz must be >= frequency_hz");
    }
}

static int lua_module_mcpwm_gc(lua_State *L)
{
    lua_module_mcpwm_ud_t *ud =
        (lua_module_mcpwm_ud_t *)luaL_testudata(L, 1, LUA_MODULE_MCPWM_METATABLE);
    if (ud) {
        lua_module_mcpwm_destroy(ud);
    }
    return 0;
}

static int lua_module_mcpwm_start(lua_State *L)
{
    lua_module_mcpwm_ud_t *ud = lua_module_mcpwm_get_ud(L, 1);
    if (!ud->running) {
        esp_err_t err = mcpwm_timer_start_stop(ud->timer, MCPWM_TIMER_START_NO_STOP);
        if (err != ESP_OK) {
            return luaL_error(L, "mcpwm start failed: %s", esp_err_to_name(err));
        }
        ud->running = true;
    }
    return 0;
}

static int lua_module_mcpwm_stop(lua_State *L)
{
    lua_module_mcpwm_ud_t *ud = lua_module_mcpwm_get_ud(L, 1);
    if (ud->running) {
        esp_err_t err = mcpwm_timer_start_stop(ud->timer, MCPWM_TIMER_STOP_EMPTY);
        if (err != ESP_OK) {
            return luaL_error(L, "mcpwm stop failed: %s", esp_err_to_name(err));
        }
        ud->running = false;
    }
    return 0;
}

static int lua_module_mcpwm_set_enabled(lua_State *L)
{
    lua_module_mcpwm_ud_t *ud = lua_module_mcpwm_get_ud(L, 1);
    if (lua_toboolean(L, 2)) {
        if (!ud->running) {
            esp_err_t err = mcpwm_timer_start_stop(ud->timer, MCPWM_TIMER_START_NO_STOP);
            if (err != ESP_OK) {
                return luaL_error(L, "mcpwm enable failed: %s", esp_err_to_name(err));
            }
            ud->running = true;
        }
    } else if (ud->running) {
        esp_err_t err = mcpwm_timer_start_stop(ud->timer, MCPWM_TIMER_STOP_EMPTY);
        if (err != ESP_OK) {
            return luaL_error(L, "mcpwm disable failed: %s", esp_err_to_name(err));
        }
        ud->running = false;
    }
    return 0;
}

static int lua_module_mcpwm_set_duty(lua_State *L)
{
    lua_module_mcpwm_ud_t *ud = lua_module_mcpwm_get_ud(L, 1);
    double duty_percent = luaL_checknumber(L, 2);
    if (duty_percent < 0.0 || duty_percent > 100.0) {
        return luaL_error(L, "mcpwm duty_percent must be between 0 and 100");
    }

    ud->config.duty_percent = duty_percent;
    esp_err_t err = lua_module_mcpwm_apply_duty(ud);
    if (err != ESP_OK) {
        return luaL_error(L, "mcpwm set_duty failed: %s", esp_err_to_name(err));
    }
    return 0;
}

static int lua_module_mcpwm_set_frequency(lua_State *L)
{
    lua_module_mcpwm_ud_t *ud = lua_module_mcpwm_get_ud(L, 1);
    uint32_t frequency_hz = (uint32_t)luaL_checkinteger(L, 2);
    if (frequency_hz == 0) {
        return luaL_error(L, "mcpwm frequency_hz must be > 0");
    }
    if (ud->config.resolution_hz < frequency_hz) {
        return luaL_error(L, "mcpwm resolution_hz must be >= frequency_hz");
    }

    uint32_t period_ticks = 0;
    esp_err_t err = lua_module_mcpwm_calc_period_ticks(ud->config.resolution_hz,
                                                       frequency_hz,
                                                       &period_ticks);
    if (err != ESP_OK) {
        return luaL_error(L, "mcpwm set_frequency failed: %s", esp_err_to_name(err));
    }

    err = mcpwm_timer_set_period(ud->timer, period_ticks);
    if (err != ESP_OK) {
        return luaL_error(L, "mcpwm set_frequency failed: %s", esp_err_to_name(err));
    }

    ud->config.frequency_hz = frequency_hz;
    ud->period_ticks = period_ticks;
    err = lua_module_mcpwm_apply_duty(ud);
    if (err != ESP_OK) {
        return luaL_error(L, "mcpwm set_frequency failed: %s", esp_err_to_name(err));
    }
    return 0;
}

static int lua_module_mcpwm_close(lua_State *L)
{
    lua_module_mcpwm_ud_t *ud =
        (lua_module_mcpwm_ud_t *)luaL_checkudata(L, 1, LUA_MODULE_MCPWM_METATABLE);
    esp_err_t err = lua_module_mcpwm_destroy(ud);
    if (err != ESP_OK) {
        return luaL_error(L, "mcpwm close failed: %s", esp_err_to_name(err));
    }
    return 0;
}

static int lua_module_mcpwm_new(lua_State *L)
{
    lua_module_mcpwm_config_t config = {0};
    lua_module_mcpwm_parse_config(L, 1, &config);

    lua_module_mcpwm_ud_t *ud =
        (lua_module_mcpwm_ud_t *)lua_newuserdata(L, sizeof(*ud));
    *ud = (lua_module_mcpwm_ud_t) {
        .config = config,
    };

    esp_err_t err = lua_module_mcpwm_create(ud);
    if (err != ESP_OK) {
        lua_module_mcpwm_destroy(ud);
        return luaL_error(L, "mcpwm new failed: %s", esp_err_to_name(err));
    }

    luaL_getmetatable(L, LUA_MODULE_MCPWM_METATABLE);
    lua_setmetatable(L, -2);
    return 1;
}

int luaopen_mcpwm(lua_State *L)
{
    if (luaL_newmetatable(L, LUA_MODULE_MCPWM_METATABLE)) {
        lua_pushcfunction(L, lua_module_mcpwm_gc);
        lua_setfield(L, -2, "__gc");
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lua_module_mcpwm_start);
        lua_setfield(L, -2, "start");
        lua_pushcfunction(L, lua_module_mcpwm_stop);
        lua_setfield(L, -2, "stop");
        lua_pushcfunction(L, lua_module_mcpwm_set_duty);
        lua_setfield(L, -2, "set_duty");
        lua_pushcfunction(L, lua_module_mcpwm_set_frequency);
        lua_setfield(L, -2, "set_frequency");
        lua_pushcfunction(L, lua_module_mcpwm_set_enabled);
        lua_setfield(L, -2, "set_enabled");
        lua_pushcfunction(L, lua_module_mcpwm_close);
        lua_setfield(L, -2, "close");
    }
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushcfunction(L, lua_module_mcpwm_new);
    lua_setfield(L, -2, "new");
    return 1;
}

esp_err_t lua_module_mcpwm_register(void)
{
    return cap_lua_register_module("mcpwm", luaopen_mcpwm);
}
