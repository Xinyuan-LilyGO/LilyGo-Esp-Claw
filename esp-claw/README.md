# ESP-Claw: An AI Agent Framework for IoT Devices

<p style="text-align: center;">
  <a href="./README.md">English</a> |
  <a href="./README_CN.md">中文</a>
</p>

![LOGO](./docs/static/ESP-CLAW-LOGO.jpg)

<p style="text-align: center;">
  <a href="https://espressif.github.io/esp-claw/en/">Home</a> |
  <a href="https://espressif.github.io/esp-claw/en/tutorial/">Docs</a>
</p>

**ESP-Claw** is a **Chat Coding** AI agent framework for IoT devices. It defines device behavior through conversation and completes the full loop of sensing, reasoning, decision-making, and execution locally on Espressif chips. Built on the OpenClaw concept, ESP-Claw adds the following features:

- **Chat Coding:** Ordinary users can define device behavior through IM chat + dynamic Lua loading, without traditional programming
- **Event-driven:** Any event can trigger the Agent Loop and other actions, not just user messages
- **Structured memory management:** Organizes memories so they can accumulate and stay useful over time, with privacy kept local
- **MCP communication:** Supports both standard MCP devices and traditional IoT hardware, with dual Server/Client identities
- **Ready out of the box:** Fast configuration through Board Manager with one-click flashing
- **Modularized:** Every module can be included or trimmed on demand

![breadboard-photo](docs/src/assets/images/claw-breadboard-photo.jpg)

## Why ESP-Claw: From Cloud-Centric to Edge AI

Traditional IoT stays at the connectivity layer: devices can connect, but cannot think; can execute, but cannot decide; can log, but cannot learn. ESP-Claw brings the Agent Runtime down from PC environments to Espressif chips, turning chips from passive executors into active decision centers.

- **Decentralized:** from an instruction receiver to an edge-side decision maker
- **Standardized protocols:** eliminate protocol silos through MCP
- **Localized data:** build a physical barrier for privacy
- **Autonomous logic:** move from hard-coded behavior to a dynamic canvas

| **Dimension** | **Traditional IoT (Cloud-Centric)** | **ESP-Claw (Edge AI)** |
| --- | --- | --- |
| **Core scenario** | Device connectivity and remote control | Physical-world sensing, decision-making, and control |
| **Processing logic** | Preset static rules (If-This-Then-That) | LLM dynamic decision-making + Lua deterministic rules |
| **Execution engine** | Rule engine | LLM + Lua + Router (three-tier event handling) |
| **Control center** | Cloud server | Edge node (ESP chip) |
| **Device protocol** | MQTT / Matter / proprietary SDK | MCP as a unified language + multi-protocol bridging |
| **Inter-device communication** | Strong dependence on cloud relay | Direct local links + MCP abstraction |
| **Memory management** | Cloud data storage | Local structured memory (JSONL + tags) |
| **Interaction model** | App / control panel | IM chat (Telegram / WeChat / Feishu) |
| **Extensibility** | Closed ecosystem, high development barrier | Plug-and-play MCP Tools |
| **Intelligence** | Preset automation | LLM + local rules for physical closed loops |

### Chat to Build: LLM Dynamic Decisions + Lua Deterministic Rules

With a single sentence sent through Feishu, WeChat, or Telegram, ESP-Claw can generate and run Lua scripts to drive peripherals like LED strips, screens, and cameras, or implement custom mini games and control algorithms. Once verified, generated logic can be persisted as local Lua rules, ensuring stable execution even when LLM services are unavailable or models change.

Try ideas like these:

- Connect an LED strip and let it generate dynamic lighting effects or weather lights
- Ask the Agent to generate a pixel-art mini game
- Let your balancing robot iteratively improve its own algorithm so it runs faster and more steadily
- Build a debugger that collects logs and controls devices for you

### Millisecond-Level Response: Event-Driven and Proactive

Unlike OpenClaw, which primarily reacts to user messages, ESP-Claw uses an **event-driven architecture**: devices actively report events, and local event buses trigger processing logic.

**Capabilities** can both emit events and execute actions. For example:

- IM can deliver user commands, call the Agent, and notify users with results
- MCP can receive sensor data, call the Agent for analysis, and then call MCP again to control actuators
- Lua scripts can call the Agent for analysis and store results locally

For high real-time requirements, **local rules execute directly** for millisecond-level response and can run independently even offline. If no local rule matches, the Agent automatically calls an LLM. For tasks beyond local compute (such as image recognition), it automatically uploads to the cloud and returns results, achieving **cloud-edge collaboration**.

![event router](docs/src/assets/images/router_block.svg)

## Local Memory System: Learns More About You Over Time

Traditional AI agents usually keep memory only inside the conversation window, which means context is easily lost once the session ends. ESP-Claw implements a complete **structured long-term memory system** directly on the device:

**Five memory types:** user profile (`profile`), user preference (`preference`), factual knowledge (`fact`), device event (`event`), and behavior rule (`rule`)

**Lightweight retrieval:** instead of relying on a vector database, ESP-Claw uses a **summary tag** mechanism. Each memory entry carries 1 to 3 keyword tags. At request time, the system injects the tag pool so the LLM can selectively recall full memory content, enabling efficient retrieval under MCU resource constraints.

**Automatic evolution:** memories keep accumulating through three paths: dialogue extraction, event archiving, and behavior rule consolidation. More importantly, the LLM can discover patterns from those memories and **proactively suggest automations**.

It also supports editable and persistent Agent core roles (such as soul and identity), giving devices customizable personality and behavior style.

### MCP as a Unified Protocol: Make Devices AI-Native Objects

ESP-Claw devices have dual identities as both **MCP Server** and **MCP Client**:

- **As an MCP Server:** hardware capabilities such as sensor reads and actuator control are packaged as standard MCP Tools that any MCP-compatible Agent (such as OpenClaw, Claude, or Codex) can call directly
- **As an MCP Client:** actively call any service exposed by MCP Servers on the network, including other IoT devices, PC-side services, and cloud software capabilities (such as querying Gaode Maps traffic or sending Feishu reminders)

**AI-native semantic interface:** tool naming follows a verb-noun pattern such as `turn_on` and `get_temperature`, while return values include units and freshness metadata. This lets AI understand and invoke tools without external documentation.

### Rich Peripheral Access: Built for Information Processing

ESP-Claw supports cameras, microphones, and many other sensors. Most drivers can be obtained through the ESP-IDF Component Registry. Built-in Lua modules include:

- **Display and interaction:** `lua_module_display`, `lua_module_lcd_touch`, `lua_module_button`
- **Multimedia:** `lua_module_camera`, `lua_module_audio`
- **Actuators:** `lua_module_led_strip`, `lua_module_gpio`, `lua_module_mcpwm`
- **System tools:** `lua_module_storage`, `lua_module_delay`, `lua_module_event_publisher`


## How to Deploy and Use

### Online Flashing

Configuration and firmware retrieval can be completed directly in the browser. No extra firmware compilation or software installation is required before flashing and use.
[Try Online Flashing](https://espressif.github.io/esp-claw/en/flash)

### Build from Source

[`basic_demo`](./application/basic_demo) provides a foundational example. For detailed build and flashing instructions, please refer to the [docs](https://espressif.github.io/esp-claw/en/tutorial/)

## Code Architecture

The project follows an **"application example + reusable components"** structure. `application/basic_demo` is a ready-to-build ESP-IDF sample project. `components` contains reusable runtime cores, capability plugins, and hardware/script extension modules.

The current codebase is divided into four layers:

- **Application assembly layer:** `application/basic_demo/main`, responsible for startup entry, network connection, parameter configuration, HTTP config pages, and demo-level module registration
- **Capability layer:** `components/claw_capabilities`, including IM communication, MCP Client/Server, Lua runtime, scheduling, files, time, web search, and more
- **Runtime core layer:** `components/claw_modules`, including core context, capability registration, event routing, memory management, and skill management
- **Device and script extension layer:** `components/lua_modules`, exposing displays, cameras, audio, buttons, GPIO, storage, and other peripherals to Lua and upper-layer Agents

```text
esp-claw/
├── application/
│   └── basic_demo/
│       ├── main/
│       │   ├── main.c                    # Firmware entry
│       │   ├── app_claw.c                # App bootstrap and assembly
│       │   ├── basic_demo_wifi.*         # Wi-Fi connection and network setup
│       │   ├── basic_demo_settings.*     # Local settings persistence
│       │   ├── config_http_server.*      # Web configuration service
│       │   ├── basic_demo_lua_modules.*  # Lua module registration
│       │   └── web/                      # Frontend assets for device config
│       └── README.md
├── components/
│   ├── claw_modules/
│   │   ├── claw_core/          # Core runtime context
│   │   ├── claw_cap/           # Capability abstraction and registration
│   │   ├── claw_event_router/  # Deterministic event routing
│   │   ├── claw_memory/        # Structured memory management
│   │   └── claw_skill/         # Skill metadata and loading
│   ├── claw_capabilities/
│   │   ├── cap_im_feishu / cap_im_qq / cap_im_tg / cap_im_wechat
│   │   ├── cap_mcp_client / cap_mcp_server
│   │   ├── cap_lua / cap_skill_mgr / cap_scheduler / cap_router_mgr
│   │   ├── cap_files / cap_time / cap_web_search / cap_cli
│   │   └── ...
│   └── lua_modules/
│       ├── lua_module_display / lua_module_camera / lua_module_audio
│       ├── lua_module_button / lua_module_gpio / lua_module_led_strip
│       ├── lua_module_storage / lua_module_delay / lua_module_event_publisher
│       └── esp_painter
├── docs/
├── README.md
└── README_CN.md
```

### Notes

- The project is still under active development. If you run into issues, feel free to open an issue at any time.
- Features such as self-programming depend on strong reasoning models. GPT-5.4, Qwen3.5-plus, or a model with similar capability is recommended for the best experience.

## Follow Us

If this project inspires or helps you, please consider giving it a star. ⭐⭐⭐⭐⭐

## Acknowledgements

ESP-Claw is inspired by [OpenClaw](https://github.com/openclaw/openclaw).

Its implementation of Agent Loop and IM communication on ESP32 was also informed by [MimiClaw](https://github.com/memovai/mimiclaw).
