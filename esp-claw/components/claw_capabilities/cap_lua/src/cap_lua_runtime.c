/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cap_lua_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lauxlib.h"
#include "lualib.h"

static const char *TAG = "cap_lua_rt";

/** Hook cadence: lower count = more frequent deadline checks and yields (WDT-friendly). */
#define CAP_LUA_HOOK_INSTRUCTION_COUNT 200

typedef struct {
    char *buf;
    size_t size;
    size_t len;
    bool truncated;
    /* deadline_us == 0 means "no wall-clock deadline" (cancel-only mode). */
    int64_t deadline_us;
    volatile bool *stop_requested;
} cap_lua_exec_ctx_t;

static void cap_lua_output_append(cap_lua_exec_ctx_t *ctx,
                                  const char *text,
                                  size_t len)
{
    size_t room;
    size_t copy;

    if (!ctx || !ctx->buf || ctx->size == 0 || !text || len == 0) {
        return;
    }

    if (ctx->len >= ctx->size - 1) {
        ctx->truncated = true;
        return;
    }

    room = ctx->size - 1 - ctx->len;
    copy = len < room ? len : room;
    memcpy(ctx->buf + ctx->len, text, copy);
    ctx->len += copy;
    ctx->buf[ctx->len] = '\0';
    if (copy < len) {
        ctx->truncated = true;
    }
}

static void cap_lua_push_json_value(lua_State *L, const cJSON *item)
{
    cJSON *child = NULL;
    int index = 1;

    if (!item || cJSON_IsNull(item)) {
        lua_pushnil(L);
        return;
    }
    if (cJSON_IsBool(item)) {
        lua_pushboolean(L, cJSON_IsTrue(item));
        return;
    }
    if (cJSON_IsNumber(item)) {
        /*
         * Whole JSON numbers become Lua integers so tool args like pin=0 stay integer
         * in Lua (avoids "0.0" in logs and subtle API mismatches).
         */
        lua_Number d = (lua_Number)item->valuedouble;
        lua_Integer i;
        if (lua_numbertointeger(d, &i) && (lua_Number)i == d) {
            lua_pushinteger(L, i);
        } else {
            lua_pushnumber(L, d);
        }
        return;
    }
    if (cJSON_IsString(item)) {
        lua_pushstring(L, item->valuestring);
        return;
    }
    if (cJSON_IsArray(item)) {
        lua_newtable(L);
        cJSON_ArrayForEach(child, item) {
            cap_lua_push_json_value(L, child);
            lua_rawseti(L, -2, index++);
        }
        return;
    }
    if (cJSON_IsObject(item)) {
        lua_newtable(L);
        cJSON_ArrayForEach(child, item) {
            cap_lua_push_json_value(L, child);
            lua_setfield(L, -2, child->string);
        }
        return;
    }

    lua_pushnil(L);
}

static int cap_lua_print_capture(lua_State *L)
{
    cap_lua_exec_ctx_t *ctx = (cap_lua_exec_ctx_t *)lua_touserdata(
                                  L, lua_upvalueindex(1));
    int top = lua_gettop(L);
    int i;

    for (i = 1; i <= top; i++) {
        size_t len = 0;
        const char *text = luaL_tolstring(L, i, &len);

        if (i > 1) {
            cap_lua_output_append(ctx, "\t", 1);
            fwrite("\t", sizeof(char), 1, stdout);
        }
        cap_lua_output_append(ctx, text, len);
        fwrite(text, sizeof(char), len, stdout);
        lua_pop(L, 1);
    }

    cap_lua_output_append(ctx, "\n", 1);
    fwrite("\n", sizeof(char), 1, stdout);
    fflush(stdout);
    return 0;
}

static void cap_lua_timeout_hook(lua_State *L, lua_Debug *ar)
{
    cap_lua_exec_ctx_t *ctx = NULL;

    (void)ar;

    lua_getglobal(L, "__cap_lua_exec_ctx");
    ctx = (cap_lua_exec_ctx_t *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    if (!ctx) {
        return;
    }

    /* Cooperative cancellation always wins over deadline reporting. */
    if (ctx->stop_requested && *ctx->stop_requested) {
        luaL_error(L, "stopped by user");
    }

    if (ctx->deadline_us != 0 && esp_timer_get_time() > ctx->deadline_us) {
        luaL_error(L, "execution timed out");
    }
    /* Yield so tight Lua loops cannot starve IDLE / trigger the task WDT (see report P2-13). */
    taskYIELD();
}

static void cap_lua_load_registered_modules(lua_State *L)
{
    size_t i;

    for (i = 0; i < cap_lua_get_module_count(); i++) {
        const cap_lua_module_t *module = cap_lua_get_module(i);

        if (!module || !module->name || !module->open_fn) {
            continue;
        }

        luaL_requiref(L, module->name, module->open_fn, 1);
        lua_pop(L, 1);
    }
}

static void cap_lua_set_args_global(lua_State *L, const char *args_json)
{
    cJSON *root = NULL;

    if (args_json && args_json[0]) {
        root = cJSON_Parse(args_json);
    }

    if (root) {
        cap_lua_push_json_value(L, root);
        cJSON_Delete(root);
    } else {
        lua_newtable(L);
    }

    lua_setglobal(L, "args");
}

static void cap_lua_run_runtime_cleanups(void)
{
    size_t i;

    for (i = 0; i < cap_lua_get_runtime_cleanup_count(); i++) {
        cap_lua_runtime_cleanup_fn_t cleanup_fn = cap_lua_get_runtime_cleanup(i);

        if (cleanup_fn) {
            cleanup_fn();
        }
    }
}

esp_err_t cap_lua_runtime_init(void)
{
    ESP_LOGI(TAG,
             "Lua runtime ready: scripts=%s registered_modules=%u",
             cap_lua_get_base_dir(),
             (unsigned int)cap_lua_get_module_count());
    return ESP_OK;
}

esp_err_t cap_lua_runtime_execute_file(const char *path,
                                       const char *args_json,
                                       uint32_t timeout_ms,
                                       volatile bool *stop_requested,
                                       char *output,
                                       size_t output_size)
{
    struct stat st = {0};
    lua_State *L = NULL;
    cap_lua_exec_ctx_t ctx = {
        .buf = output,
        .size = output_size,
        .deadline_us = (timeout_ms == 0)
                       ? 0
                       : esp_timer_get_time() + ((int64_t)timeout_ms * 1000),
        .stop_requested = stop_requested,
    };
    int status;

    if (!output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    output[0] = '\0';

    if (!cap_lua_path_is_valid(path)) {
        snprintf(output, output_size, "Error: Lua path must be a valid .lua script path");
        return ESP_ERR_INVALID_ARG;
    }

    if (stat(path, &st) != 0) {
        snprintf(output, output_size, "Error: Lua script not found: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    if (st.st_size <= 0 || st.st_size > CAP_LUA_MAX_SCRIPT_SIZE) {
        snprintf(output, output_size, "Error: Lua script size invalid: %ld bytes", (long)st.st_size);
        return ESP_ERR_INVALID_SIZE;
    }

    L = luaL_newstate();
    if (!L) {
        snprintf(output, output_size, "Error: failed to create Lua state");
        return ESP_ERR_NO_MEM;
    }

    luaL_openlibs(L);
    cap_lua_load_registered_modules(L);
    lua_pushlightuserdata(L, &ctx);
    lua_setglobal(L, "__cap_lua_exec_ctx");
    cap_lua_set_args_global(L, args_json);
    lua_pushlightuserdata(L, &ctx);
    lua_pushcclosure(L, cap_lua_print_capture, 1);
    lua_setglobal(L, "print");
    lua_sethook(L, cap_lua_timeout_hook, LUA_MASKCOUNT, CAP_LUA_HOOK_INSTRUCTION_COUNT);

    status = luaL_dofile(L, path);
    cap_lua_run_runtime_cleanups();
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        if (ctx.len > 0) {
            cap_lua_output_append(&ctx, "ERROR: ", 7);
        }
        cap_lua_output_append(&ctx,
                              msg ? msg : "unknown Lua error",
                              strlen(msg ? msg : "unknown Lua error"));
        cap_lua_output_append(&ctx, "\n", 1);
        lua_close(L);
        return ESP_FAIL;
    }

    if (ctx.len == 0) {
        cap_lua_output_append(&ctx, "Lua script completed with no output.\n", 36);
    } else if (ctx.truncated) {
        cap_lua_output_append(&ctx, "[output truncated]\n", 19);
    }

    lua_close(L);
    return ESP_OK;
}
