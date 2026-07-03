/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_claw.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "basic_demo_lua_modules.h"
#include "basic_demo_wifi.h"
#include "cap_files.h"
#include "cap_im_feishu.h"
#include "cap_im_qq.h"
#include "cap_im_tg.h"
#include "cap_im_wechat.h"
#include "cap_llm_inspect.h"
#include "cap_lua.h"
#include "cap_mcp_client.h"
#include "cap_mcp_server.h"
#include "cap_router_mgr.h"
#include "cap_session_mgr.h"
#include "cap_scheduler.h"
#include "cap_skill_mgr.h"
#include "cap_system.h"
#include "cap_time.h"
#include "cap_web_search.h"
#include "claw_event_publisher.h"
#include "claw_event_router.h"
#include "claw_cap.h"
#include "claw_core.h"
#include "claw_memory.h"
#include "claw_skill.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "app_esp_claw";
#if CONFIG_BASIC_DEMO_MEMORY_MODE_FULL
static const char *const BASIC_DEMO_LLM_VISIBLE_GROUPS[] = {
    "cap_files",
    "cap_skill",
    "cap_system",
    "cap_lua",
    "claw_memory",
};
#else
static const char *const BASIC_DEMO_LLM_VISIBLE_GROUPS[] = {
    "cap_files",
    "cap_skill",
    "cap_system",
    "cap_lua",
};
#endif

#define BASIC_DEMO_IM_ATTACHMENT_MAX_BYTES (2 * 1024 * 1024)

#define BASIC_DEMO_SYSTEM_PROMPT_COMMON \
    "You are the ESP-Claw. " \
    "Answer briefly and plainly. " \
    "Treat Skills List as a catalog of optional skills." \
    "Use 'activate_skill' to load a skill, and you will gain more callable capabilities\n" \
    "Skills are user-facing functions, while Capabilities are internal functions used by the model.\n" \
    "After completing the task, call 'deactivate_skill' to keep the context streamlined and efficient." \
    "When communicating with the user, refer to skills instead of Capabilities. " \
    "When the user asks to write, run, or fix Lua scripts, activate skills 'cap_lua_edit', 'cap_lua_run', and 'cap_lua_patterns' first and follow them. " \
    "When Lua sends an IM reply via event_publisher, prefer ep.publish_message(\"text\") (string only); if you use a table, include source_cap and text or the script will error. " \
    "When reading Lua script sources with read_file, use paths under scripts/ matching lua_list_scripts (e.g. scripts/temp/foo.lua), not the bare lua_list_scripts path. " \
    "If the user is defining or redesigning the assistant's persona, identity, role, behavior style, speech style, or standing profile, activate the 'profile_memory_ops' skill and prefer persistent profile updates over temporary roleplay when lasting intent is clear. " \

#if CONFIG_BASIC_DEMO_MEMORY_MODE_FULL
#define BASIC_DEMO_SYSTEM_PROMPT_SUFFIX \
    "When long-term memory is needed, activate the 'memory_ops' skill first and follow its instructions. " \
    "Do not activate or use the memory skill for ordinary self-introductions or casual preferences unless the user explicitly asks to remember, save, update, or forget something. Automatic extraction will handle durable facts silently after the reply when appropriate. " \
    "Use memory tools only through that skill. " \
    "Auto-injected memory context contains summary labels, not full memory bodies. " \
    "When detailed long-term memory is needed, use exact summary labels with memory_recall. " \
    "Do not ask whether the user wants you to remember ordinary profile or preference statements when automatic extraction can handle them. Do not offer memory-save help unless the user explicitly asks about memory management. " \
    "Do not use memory_records.jsonl, memory_index.json, memory_digest.log, or MEMORY.md as direct decision input.\n"
#else
#define BASIC_DEMO_SYSTEM_PROMPT_SUFFIX "\n"
#endif

#define BASIC_DEMO_SYSTEM_PROMPT \
    BASIC_DEMO_SYSTEM_PROMPT_COMMON \
    BASIC_DEMO_SYSTEM_PROMPT_SUFFIX

esp_err_t basic_demo_cli_start(void);

typedef struct {
    char memory_session_root[64];
    char memory_root_dir[64];
    char skills_root_dir[64];
    char lua_root_dir[64];
    char router_rules_path[96];
    char scheduler_rules_path[96];
    char im_attachment_root[64];
} basic_demo_paths_t;

static esp_err_t basic_demo_init_paths(basic_demo_paths_t *paths)
{
    const char *base = basic_demo_fatfs_base_path;
    if (!paths || !base || base[0] != '/') {
        return ESP_ERR_INVALID_STATE;
    }
    if (snprintf(paths->memory_session_root, sizeof(paths->memory_session_root), "%s/sessions", base) >= sizeof(paths->memory_session_root) ||
        snprintf(paths->memory_root_dir, sizeof(paths->memory_root_dir), "%s/memory", base) >= sizeof(paths->memory_root_dir) ||
        snprintf(paths->skills_root_dir, sizeof(paths->skills_root_dir), "%s/skills", base) >= sizeof(paths->skills_root_dir) ||
        snprintf(paths->lua_root_dir, sizeof(paths->lua_root_dir), "%s/scripts", base) >= sizeof(paths->lua_root_dir) ||
        snprintf(paths->router_rules_path, sizeof(paths->router_rules_path), "%s/router_rules/router_rules.json", base) >= sizeof(paths->router_rules_path) ||
        snprintf(paths->scheduler_rules_path, sizeof(paths->scheduler_rules_path), "%s/scheduler/schedules.json", base) >= sizeof(paths->scheduler_rules_path) ||
        snprintf(paths->im_attachment_root, sizeof(paths->im_attachment_root), "%s/inbox", base) >= sizeof(paths->im_attachment_root)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static bool basic_demo_time_network_ready(void *ctx)
{
    (void)ctx;
    return basic_demo_wifi_is_connected();
}

static void basic_demo_time_sync_success(bool had_valid_time, void *ctx)
{
    (void)ctx;

    if (!had_valid_time) {
        esp_err_t err = cap_scheduler_handle_time_sync();

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Scheduler rebase after first time sync failed: %s",
                     esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Scheduler rebased after first successful time sync");
        }
    }
}

static esp_err_t init_memory(const basic_demo_settings_t *settings, const basic_demo_paths_t *paths)
{
    claw_memory_config_t memory_config = {
        .session_root_dir = paths->memory_session_root,
        .memory_root_dir = paths->memory_root_dir,
        .max_session_messages = 20,
        .max_message_chars = 1024,
        .llm = {
            .api_key = settings->llm_api_key,
            .backend_type = settings->llm_backend_type,
            .profile = settings->llm_profile,
            .model = settings->llm_model,
            .base_url = settings->llm_base_url,
            .auth_type = settings->llm_auth_type,
            .timeout_ms = (uint32_t)strtoul(settings->llm_timeout_ms, NULL, 10),
            .image_max_bytes = 0,
        },
#if CONFIG_BASIC_DEMO_MEMORY_MODE_FULL
        .enable_async_extract_stage_note = true,
#else
        .enable_async_extract_stage_note = false,
#endif
    };
    esp_err_t err;

    err = claw_memory_init(&memory_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init claw_memory: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

static esp_err_t init_skills(const basic_demo_paths_t *paths)
{
    ESP_RETURN_ON_ERROR(claw_skill_init(&(claw_skill_config_t){
                            .skills_root_dir = paths->skills_root_dir,
                            .session_state_root_dir = paths->memory_session_root,
                            .max_file_bytes = 10 * 1024,
                        }),
                        TAG, "Failed to init claw_skill");
    return ESP_OK;
}

static esp_err_t init_capabilities(const basic_demo_settings_t *settings, const basic_demo_paths_t *paths)
{
    claw_cap_list_t cap_list;
    claw_cap_group_list_t group_list;
    esp_err_t err;

#define REGISTER_CAP_GROUP(call, label) do { \
        err = (call); \
        cap_list = claw_cap_list(); \
        group_list = claw_cap_list_groups(); \
        if (err != ESP_OK) { \
            ESP_LOGE(TAG, "%s failed: %s (groups=%u, caps=%u)", \
                     (label), esp_err_to_name(err), \
                     (unsigned)group_list.count, \
                     (unsigned)cap_list.count); \
            return err; \
        } \
        ESP_LOGI(TAG, "%s ok (groups=%u, caps=%u)", \
                 (label), \
                 (unsigned)group_list.count, \
                 (unsigned)cap_list.count); \
    } while (0)

    ESP_RETURN_ON_ERROR(claw_cap_init(), TAG, "Failed to init claw_cap");

    ESP_RETURN_ON_ERROR(cap_files_set_base_dir(basic_demo_fatfs_base_path), TAG, "Failed to set files cap base dir");
    ESP_RETURN_ON_ERROR(cap_im_qq_set_attachment_config(&(cap_im_qq_attachment_config_t){
                            .storage_root_dir = paths->im_attachment_root,
                            .max_inbound_file_bytes = BASIC_DEMO_IM_ATTACHMENT_MAX_BYTES,
                            .enable_inbound_attachments = true,
                        }),
                        TAG, "Failed to set QQ attachment config");
    ESP_RETURN_ON_ERROR(cap_im_tg_set_attachment_config(&(cap_im_tg_attachment_config_t){
                            .storage_root_dir = paths->im_attachment_root,
                            .max_inbound_file_bytes = BASIC_DEMO_IM_ATTACHMENT_MAX_BYTES,
                            .enable_inbound_attachments = true,
                        }),
                        TAG, "Failed to set Telegram attachment config");
    ESP_RETURN_ON_ERROR(cap_im_wechat_set_attachment_config(&(cap_im_wechat_attachment_config_t){
                            .storage_root_dir = paths->im_attachment_root,
                            .max_inbound_file_bytes = BASIC_DEMO_IM_ATTACHMENT_MAX_BYTES,
                            .enable_inbound_attachments = true,
                        }),
                        TAG, "Failed to set WeChat attachment config");
    ESP_RETURN_ON_ERROR(cap_im_feishu_set_attachment_config(&(cap_im_feishu_attachment_config_t){
                            .storage_root_dir = paths->im_attachment_root,
                            .max_inbound_file_bytes = BASIC_DEMO_IM_ATTACHMENT_MAX_BYTES,
                            .enable_inbound_attachments = true,
                        }),
                        TAG, "Failed to set Feishu attachment config");

    if (settings->qq_app_id[0] && settings->qq_app_secret[0]) {
        ESP_RETURN_ON_ERROR(cap_im_qq_set_credentials(settings->qq_app_id, settings->qq_app_secret), TAG, "Failed to set QQ credentials");
    }

    if (settings->feishu_app_id[0] && settings->feishu_app_secret[0]) {
        ESP_RETURN_ON_ERROR(cap_im_feishu_set_credentials(settings->feishu_app_id, settings->feishu_app_secret), TAG, "Failed to set Feishu credentials");
    }

    if (settings->tg_bot_token[0]) {
        ESP_RETURN_ON_ERROR(cap_im_tg_set_token(settings->tg_bot_token), TAG, "Failed to set Telegram bot token");
    }

    if (settings->wechat_token[0] && settings->wechat_base_url[0]) {
        ESP_RETURN_ON_ERROR(cap_im_wechat_set_client_config(&(cap_im_wechat_client_config_t){
                                .token = settings->wechat_token,
                                .base_url = settings->wechat_base_url,
                                .cdn_base_url = settings->wechat_cdn_base_url,
                                .account_id = settings->wechat_account_id,
                            }),
                            TAG, "Failed to set WeChat client config");
    }

    if (settings->search_brave_key[0]) {
        ESP_RETURN_ON_ERROR(cap_web_search_set_brave_key(settings->search_brave_key), TAG, "Failed to set Brave search key");
    }

    if (settings->search_tavily_key[0]) {
        ESP_RETURN_ON_ERROR(cap_web_search_set_tavily_key(settings->search_tavily_key), TAG, "Failed to set Tavily search key");
    }

    REGISTER_CAP_GROUP(cap_im_qq_register_group(), "Register QQ cap");
    REGISTER_CAP_GROUP(cap_im_feishu_register_group(), "Register Feishu cap");
    REGISTER_CAP_GROUP(cap_im_tg_register_group(), "Register Telegram cap");
    REGISTER_CAP_GROUP(cap_im_wechat_register_group(), "Register WeChat cap");
    REGISTER_CAP_GROUP(cap_files_register_group(), "Register files cap");
    ESP_RETURN_ON_ERROR(basic_demo_lua_modules_register(), TAG, "Failed to register app Lua modules");
    REGISTER_CAP_GROUP(cap_scheduler_register_group(), "Register scheduler cap");
    REGISTER_CAP_GROUP(cap_lua_register_group(paths->lua_root_dir), "Register Lua cap");
    REGISTER_CAP_GROUP(cap_mcp_client_register_group(), "Register MCP client cap");
    REGISTER_CAP_GROUP(cap_mcp_server_register_group(), "Register MCP server cap");
    REGISTER_CAP_GROUP(cap_skill_mgr_register_group(), "Register skill cap");
    REGISTER_CAP_GROUP(cap_system_register_group(), "Register system cap");
#if CONFIG_BASIC_DEMO_MEMORY_MODE_FULL
    REGISTER_CAP_GROUP(claw_memory_register_group(), "Register claw_memory group");
#endif
    REGISTER_CAP_GROUP(cap_time_register_group(), "Register time cap");
    REGISTER_CAP_GROUP(cap_llm_inspect_register_group(), "Register LLM inspect cap");
    REGISTER_CAP_GROUP(cap_web_search_register_group(), "Register web search cap");
    REGISTER_CAP_GROUP(cap_router_mgr_register_group(), "Register router manager cap");
    REGISTER_CAP_GROUP(cap_session_mgr_register_group(), "Register session manager cap");
    ESP_RETURN_ON_ERROR(claw_cap_set_llm_visible_groups(
                            BASIC_DEMO_LLM_VISIBLE_GROUPS, sizeof(BASIC_DEMO_LLM_VISIBLE_GROUPS) / sizeof(BASIC_DEMO_LLM_VISIBLE_GROUPS[0])),
                        TAG, "Failed to set LLM-visible capability groups");
    ESP_RETURN_ON_ERROR(claw_cap_start_all(), TAG, "Failed to start capabilities");

#undef REGISTER_CAP_GROUP

    return ESP_OK;
}

static const char *basic_demo_llm_provider_name(const basic_demo_settings_t *settings)
{
    if (!settings) {
        return "unknown";
    }

    if ((settings->llm_backend_type[0] && strcmp(settings->llm_backend_type, "anthropic") == 0) ||
        (settings->llm_profile[0] && strcmp(settings->llm_profile, "anthropic") == 0)) {
        return "Anthropic";
    }
    if (settings->llm_profile[0] && (strcmp(settings->llm_profile, "qwen") == 0 || strcmp(settings->llm_profile, "qwen_compatible") == 0)) {
        return "Qwen Compatible";
    }
    if (settings->llm_profile[0] && strcmp(settings->llm_profile, "openai") == 0) {
        return "OpenAI";
    }
    return "Custom";
}

static bool basic_demo_llm_is_configured(const basic_demo_settings_t *settings)
{
    return settings && settings->llm_api_key[0] && settings->llm_model[0] && settings->llm_profile[0];
}

esp_err_t app_claw_start(const basic_demo_settings_t *settings)
{
    basic_demo_paths_t paths = {0};
    claw_core_config_t core_config = {0};
    claw_event_router_config_t router_config = {
        .rules_path = NULL,
        .task_stack_size = 8 * 1024,
        .task_priority = 5,
        .task_core = tskNO_AFFINITY,
        .core_submit_timeout_ms = 1000,
        .core_receive_timeout_ms = 130000,
        .default_route_messages_to_agent = false,
        .session_builder = cap_session_mgr_build_session_id,
    };
    bool llm_enabled = false;

    if (!settings) {
        return ESP_ERR_INVALID_ARG;
    }

    llm_enabled = basic_demo_llm_is_configured(settings);
    router_config.default_route_messages_to_agent = llm_enabled;

    ESP_RETURN_ON_ERROR(basic_demo_init_paths(&paths), TAG, "Failed to init storage paths");
    router_config.rules_path = paths.router_rules_path;
    ESP_RETURN_ON_ERROR(cap_session_mgr_set_session_root_dir(paths.memory_session_root), TAG, "Failed to configure session manager");
    ESP_RETURN_ON_ERROR(claw_event_router_init(&router_config), TAG, "Failed to init event router");
    ESP_RETURN_ON_ERROR(cap_scheduler_init(&(cap_scheduler_config_t){
                            .schedules_path = paths.scheduler_rules_path,
                            .tick_ms = 1000,
                            .max_items = 32,
                            .task_stack_size = 6144,
                            .task_priority = 5,
                            .task_core = tskNO_AFFINITY,
                            .publish_event = claw_event_router_publish,
                            .persist_after_fire = true,
                        }),
                        TAG, "Failed to init scheduler");
    ESP_RETURN_ON_ERROR(init_memory(settings, &paths), TAG, "Failed to init memory");
    ESP_RETURN_ON_ERROR(init_skills(&paths), TAG, "Failed to init skills");
    ESP_RETURN_ON_ERROR(init_capabilities(settings, &paths), TAG, "Failed to init capabilities");
    ESP_RETURN_ON_ERROR(claw_event_router_register_outbound_binding("qq", "qq_send_message"), TAG, "Failed to bind QQ outbound");
    ESP_RETURN_ON_ERROR(claw_event_router_register_outbound_binding("feishu", "feishu_send_message"), TAG, "Failed to bind Feishu outbound");
    ESP_RETURN_ON_ERROR(claw_event_router_register_outbound_binding("telegram", "tg_send_message"), TAG, "Failed to bind Telegram outbound");
    ESP_RETURN_ON_ERROR(claw_event_router_register_outbound_binding("wechat", "wechat_send_message"), TAG, "Failed to bind WeChat outbound");

    core_config.api_key = settings->llm_api_key;
    core_config.backend_type = settings->llm_backend_type;
    core_config.profile = settings->llm_profile;
    core_config.model = settings->llm_model;
    core_config.base_url = settings->llm_base_url;
    core_config.auth_type = settings->llm_auth_type;
    core_config.timeout_ms = (uint32_t)strtoul(settings->llm_timeout_ms, NULL, 10);
    core_config.system_prompt = BASIC_DEMO_SYSTEM_PROMPT;
#if CONFIG_BASIC_DEMO_MEMORY_MODE_FULL
    core_config.append_session_turn = claw_memory_append_session_turn_callback;
    core_config.on_request_start = claw_memory_request_start_callback;
    core_config.collect_stage_note = claw_memory_stage_note_callback;
#else
    core_config.append_session_turn = claw_memory_append_session_turn_callback;
#endif
    core_config.call_cap = claw_cap_call_from_core;
    core_config.task_stack_size = 16 * 1024;
    core_config.task_priority = 5;
    core_config.task_core = tskNO_AFFINITY;
    core_config.max_tool_iterations = 20;
    core_config.request_queue_len = 4;
    core_config.response_queue_len = 4;
    core_config.max_context_providers = 8;

    if (!llm_enabled) {
        ESP_LOGW(TAG, "LLM is not fully configured. Provider=%s profile=%s model=%s. "
                      "The demo will start without claw_core; ask, auto-route-to-agent, and image analysis stay disabled until LLM API key, profile, and model are set.",
                 basic_demo_llm_provider_name(settings), settings->llm_profile[0] ? settings->llm_profile : "(empty)",
                 settings->llm_model[0] ? settings->llm_model : "(empty)");
    } else {
        ESP_LOGI(TAG, "Starting LLM provider=%s profile=%s backend=%s model=%s", basic_demo_llm_provider_name(settings), settings->llm_profile,
                 settings->llm_backend_type[0] ? settings->llm_backend_type : "(default)", settings->llm_model);
        ESP_RETURN_ON_ERROR(claw_core_init(&core_config), TAG, "Failed to init claw_core");
        ESP_RETURN_ON_ERROR(claw_core_add_context_provider(&claw_memory_profile_provider), TAG, "Failed to add editable profile memory provider");

#if CONFIG_BASIC_DEMO_MEMORY_MODE_FULL
        ESP_RETURN_ON_ERROR(claw_core_add_context_provider(&claw_memory_long_term_provider), TAG, "Failed to add long-term memory provider");
#else
        ESP_RETURN_ON_ERROR(claw_core_add_context_provider(&claw_memory_long_term_lightweight_provider), TAG,"Failed to add lightweight long-term memory provider");
#endif

        ESP_RETURN_ON_ERROR(claw_core_add_context_provider(&claw_memory_session_history_provider), TAG, "Failed to add session history provider");
        ESP_RETURN_ON_ERROR(claw_core_add_context_provider(&claw_skill_skills_list_provider), TAG, "Failed to add skills list provider");
        ESP_RETURN_ON_ERROR(claw_core_add_context_provider(&claw_skill_active_skill_docs_provider), TAG, "Failed to add active skill docs provider");
        ESP_RETURN_ON_ERROR(claw_core_add_context_provider(&claw_cap_tools_provider), TAG, "Failed to add cap tools provider");
        ESP_RETURN_ON_ERROR(claw_core_add_context_provider(&cap_lua_async_jobs_provider), TAG, "Failed to add Lua async jobs provider");
        ESP_RETURN_ON_ERROR(claw_core_add_completion_observer(cap_lua_honesty_observe_completion, NULL), TAG, "Failed to install Lua honesty observer");

        ESP_RETURN_ON_ERROR(claw_core_start(), TAG, "Failed to start claw_core");
    }

    ESP_RETURN_ON_ERROR(claw_event_router_start(), TAG, "Failed to start event router");
    ESP_RETURN_ON_ERROR(cap_time_sync_service_start(&(cap_time_sync_service_config_t){
                            .network_ready = basic_demo_time_network_ready,
                            .on_sync_success = basic_demo_time_sync_success,
                        }),
                        TAG, "Failed to start time sync service");
    ESP_RETURN_ON_ERROR(cap_scheduler_start(), TAG, "Failed to start scheduler");
    ESP_RETURN_ON_ERROR(basic_demo_cli_start(), TAG, "Failed to start CLI");

    return ESP_OK;
}
