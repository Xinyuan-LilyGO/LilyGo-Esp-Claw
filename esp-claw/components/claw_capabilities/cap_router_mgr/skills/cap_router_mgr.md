# Router Rule Management

Use this skill when the user needs to inspect or change event router automation rules.

## When to use
- The user asks to list, view, add, update, delete, or reload automation rules.
- The user asks to create a rule that reacts to `event_type` or `event_key` and performs actions.
- The user asks to route schedule, trigger, or message events to capabilities, the agent, scripts, outbound messages, or emitted events.

## Available capabilities
- `list_router_rules`: list all rules as a JSON array.
- `get_router_rule`: get one rule by `id`.
- `add_router_rule`: add one rule by passing `rule_json` string.
- `update_router_rule`: replace one rule by passing `rule_json` string.
- `delete_router_rule`: delete one rule by `id`.
- `reload_router_rules`: reload rules from disk.

## Calling rules
- Use the direct router manager capabilities. Do not route through console wrappers unless the user explicitly asks for CLI commands.
- `add_router_rule` and `update_router_rule` input must be:

```json
{
  "rule_json": "<JSON string of one rule object>"
}
```

- `rule_json` is a string, not an object.
- `get_router_rule` and `delete_router_rule` input must be:

```json
{
  "id": "rule_id"
}
```

## Rule object shape
- Required fields inside `rule_json`:
  - `id`: string
  - `match`: object
  - `actions`: non-empty array
  - `match.event_type`: string
- Common optional rule-level fields:
  - `description`
  - `enabled`
  - `consume_on_match`
  - `ack`
  - `vars`
- Common optional match fields:
  - `event_key`
  - `source_cap`
  - `source_channel`
  - `chat_id`
  - `content_type`
  - `text`

## Action types
- Supported action `type` values:
  - `call_cap`
  - `run_agent`
  - `run_script`
  - `send_message`
  - `emit_event`
  - `drop`
- Optional action-level fields supported by the runtime:
  - `caller`: one of `system`, `agent`, `console`
  - `capture_output`: boolean
  - `fail_open`: boolean

## Action requirements
- `call_cap`
  - Requires `cap` and `input` object.
- `run_agent`
  - `input` object is optional. If omitted, the runtime treats it as `{}`.
  - Common useful fields in `input` include `text`, `target_channel`, `target_chat_id`, and `session_policy`.
- `run_script`
  - Requires `input` object.
  - The router converts this action into a Lua script capability call.
  - Common fields are `path`, `args`, and optional `async`.
- `send_message`
  - Requires `input` object.
  - `input.channel`, `input.chat_id`, and `input.message` may be templated.
  - If `channel` is omitted, the router falls back to the event target channel or source channel.
  - If `chat_id` is omitted, the router falls back to the event target endpoint or event chat id.
  - If `message` is omitted, the router falls back to the previous action output in the same rule execution.
- `emit_event`
  - Requires `input` object.
  - Common fields are `event_type`, `source_cap`, `source_channel`, `chat_id`, `message_id`, `content_type`, `text`, `payload_json`, and `session_policy`.
- `drop`
  - No special input is required.

## Template guidance
- Rule action inputs are rendered against the current event context before execution.
- Common templates include:
  - `{{event.source_channel}}`
  - `{{event.chat_id}}`
  - `{{event.text}}`
  - `{{event.payload_json}}`
- Use templates instead of hardcoding chat routing when the action should reply back to the originating conversation.

## Recommended workflow
1. Call `list_router_rules` first to inspect current rules and avoid id conflicts.
2. Decide the minimal match condition needed, starting with `match.event_type`.
3. Add or update exactly one rule.
4. Call `get_router_rule` with that id to verify the final shape.
5. If the user explicitly asks to re-read rules from storage, call `reload_router_rules`.

## Common failure causes
- Putting `event_type` at rule top level instead of `match.event_type`.
- Passing `rule_json` as an object instead of a string.
- Missing `actions`, or using an empty `actions` array.
- Using unsupported action `type`.
- Missing required fields for the chosen action type.
- Using `source_channel` in one place and `channel` in another. For rule matching, use `match.source_channel`.
- Using `add_router_rule` with an existing `id`. Use `update_router_rule` instead.

## Examples

### `send_message` with templates

Reply to the same source channel and chat that produced the event.

```json
{
  "rule_json": "{\"id\":\"schedule_echo_reply\",\"description\":\"Reply to the originating chat when a schedule event arrives\",\"enabled\":true,\"consume_on_match\":true,\"match\":{\"event_type\":\"schedule\",\"event_key\":\"drink_reminder\"},\"actions\":[{\"type\":\"send_message\",\"input\":{\"channel\":\"{{event.source_channel}}\",\"chat_id\":\"{{event.chat_id}}\",\"message\":\"Time to drink water.\"}}]}"
}
```

### `run_agent`

Wake the agent when a scheduled event arrives and deliver the result to a specific chat.

```json
{
  "rule_json": "{\"id\":\"daily_agent_check_agent\",\"description\":\"Wake agent for daily scheduled check\",\"enabled\":true,\"consume_on_match\":true,\"match\":{\"event_type\":\"schedule\",\"event_key\":\"daily_agent_check\"},\"actions\":[{\"type\":\"run_agent\",\"input\":{\"text\":\"{{event.text}}\",\"target_channel\":\"qq\",\"target_chat_id\":\"group:example\",\"session_policy\":\"trigger\"}}]}"
}
```

### `call_cap`

Call another capability when a matching event arrives.

```json
{
  "rule_json": "{\"id\":\"router_call_cap_demo\",\"description\":\"Call a capability on matching trigger\",\"enabled\":true,\"consume_on_match\":true,\"match\":{\"event_type\":\"trigger\",\"event_key\":\"demo\"},\"actions\":[{\"type\":\"call_cap\",\"cap\":\"roll_chat_session\",\"input\":{}}]}"
}
```

### `emit_event`

Emit a follow-up event for downstream handling.

```json
{
  "rule_json": "{\"id\":\"router_emit_event_demo\",\"description\":\"Convert one event into another\",\"enabled\":true,\"consume_on_match\":false,\"match\":{\"event_type\":\"message\",\"event_key\":\"text\"},\"actions\":[{\"type\":\"emit_event\",\"input\":{\"event_type\":\"trigger\",\"source_cap\":\"claw_event_router\",\"source_channel\":\"{{event.source_channel}}\",\"chat_id\":\"{{event.chat_id}}\",\"content_type\":\"trigger\",\"text\":\"follow-up\",\"payload_json\":\"{}\",\"session_policy\":\"trigger\"}}]}"
}
```

### Get one rule

```json
{
  "id": "schedule_echo_reply"
}
```

### Delete one rule

```json
{
  "id": "schedule_echo_reply"
}
```
