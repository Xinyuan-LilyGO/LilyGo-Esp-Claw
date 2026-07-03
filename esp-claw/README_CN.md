# ESP-Claw：物联网设备 AI 智能体框架

<p style="text-align: center;">
  <a href="./README.md">English</a> |
  <a href="./README_CN.md">中文</a>
</p>

![LOGO](./docs/static/ESP-CLAW-LOGO.jpg)

<p style="text-align: center;">
  <a href="https://espressif.github.io/esp-claw/zh-cn/">首页</a> |
  <a href="https://espressif.github.io/esp-claw/zh-cn/tutorial/">文档</a>
</p>

**ESP-Claw** 是面向物联网设备的 **Chat Coding（聊天造物）** 式 AI 智能体框架，以对话定义设备行为，在乐鑫芯片上本地完成感知、推理、决策与执行的完整闭环。在 OpenClaw 理念的基础上，ESP-Claw 增加了如下特性：

- **Chat Coding：** 通过 IM 聊天 + Lua 动态加载，普通用户即可定义设备行为，无需编程
- **事件驱动：** 可由任意事件触发 Agent Loop 和其他动作，而不只是用户消息
- **结构化记忆管理：** 有条理地沉淀记忆内容，隐私不上云
- **MCP 通讯：** 支持标准 MCP 设备与传统 IoT 设备接入，设备具备 Server/Client 双重身份
- **开箱即用：** 基于 Board Manager 快速配置，并提供一键烧录
- **组件化：** 所有模块均可按需裁剪

![breadboard-photo](docs/src/assets/images/claw-breadboard-photo.jpg)

## 为什么 ESP-Claw：从云中心化到边缘 AI

传统 IoT 只停留在连接层——设备能联网，却不能思考；能执行，却不能决策；能记录，却不能学习。ESP-Claw 将 Agent Runtime 从 PC 环境下沉至乐鑫芯片，让芯片从被动的"执行端"转变为主动的"决策中心"。

- **去中心化：** 从"指令接收者"变为"边缘决策者"
- **协议标准化：** 以 MCP 消除"协议孤岛"
- **数据本地化：** 构建隐私的"物理屏障"
- **逻辑自主化：** 从"硬编码"转向"动态画布"

| **维度** | **传统 IoT（云中心化）** | **ESP-Claw（边缘 AI）** |
| --- | --- | --- |
| **核心场景** | 设备联网与远程控制 | 物理世界感知、决策与控制 |
| **处理逻辑** | 预设静态规则（IF-This-Then-That） | LLM 动态决策 + Lua 确定性规则 |
| **执行引擎** | 规则引擎 | LLM + Lua + Router（三级事件处理） |
| **控制中心** | 云端服务器 | 边缘节点（ESP 芯片） |
| **设备协议** | MQTT / Matter / 私有 SDK | MCP 统一语言 + 多协议桥接 |
| **设备间通讯** | 强依赖云中转 | 本地直连 + MCP 抽象 |
| **记忆管理** | 云端数据存储 | 本地结构化记忆（JSONL + 标签） |
| **交互方式** | App / 控制面板 | IM（Telegram / 微信 / 飞书） |
| **扩展性** | 生态封闭，开发门槛高 | MCP Tool 即插即用 |
| **智能能力** | 预设自动化 | LLM + 本地规则（物理闭环） |

### 聊天即造物：LLM 动态决策 + Lua 确定性规则

用户通过飞书、微信或 Telegram 发送一句话，ESP-Claw 即可生成并运行对应的 Lua 脚本——驱动灯带、屏幕、摄像头等外设，或实现自定义游戏、控制算法。测试满意后，生成的逻辑可一键固化为本地 Lua 规则，确保在 LLM 服务中断或更换模型时仍能稳定运行。

尝试以下玩法：

- 接入一条灯带，让它帮你生成炫酷的灯效或天气灯
- 让 Agent 生成一个像素风的小游戏
- 让你的平衡车自主迭代算法，让它跑得又快又稳
- 做一个调试器，让它替你采集日志、控制设备

### 毫秒级响应：事件驱动，主动感知

不同于 OpenClaw 主要围绕用户消息进行响应，ESP-Claw 采用**事件驱动架构**：设备主动上报事件，由本地事件总线触发处理逻辑。

**Capability** 可以同时是事件的发出者和行为的执行者，例如：

- IM 可以传达用户的命令，调用 Agent 处理并将结果通知给用户
- MCP 接收传感器数据，调用 Agent 对数据进行分析后，再调用 MCP 控制执行器
- Lua 脚本调用 Agent 进行分析后，将分析结果存到本地

对于高实时性需求，通过**本地规则直接执行**，实现毫秒级响应，断网亦可独立运行。当本地无匹配规则时，Agent 自动调用 LLM 进行分析；对超出本地算力的任务（如图像识别），自动上传云端处理后返回结果，实现**云边协同**。

![event router](docs/src/assets/images/router_block.svg)

### 本地记忆系统：越用越懂你

传统 AI Agent 的记忆通常局限于对话窗口，会话结束后就容易遗忘。ESP-Claw 在设备本地实现了完整的 **结构化长期记忆系统**：

**五类记忆：** 用户资料（`profile`）· 用户偏好（`preference`）· 事实知识（`fact`）· 设备事件（`event`）· 行为规则（`rule`）

**轻量级检索：** 不依赖向量数据库，而采用 **摘要标签** 机制。每条记忆附带 1–3 个关键词标签，请求时系统注入标签池，供 LLM 按需召回正文，从而在 MCU 有限资源下实现高效检索。

**自动进化：** 系统通过对话抽取、事件归档、行为规则沉淀三条链路持续积累记忆。更关键的是，LLM 能从中发现规律，并 **主动建议自动化**。

同时支持对 Agent 核心角色（如 soul、identity 等）的可编辑与持久化，使设备具备可定制的人格与行为风格。

### MCP 统一协议：让设备成为 AI 原生对象

ESP-Claw 设备同时具备 **MCP Server** 和 **MCP Client** 双重身份：

- **作为 MCP Server：** 将传感器读取、执行器控制等硬件能力封装为标准 MCP Tool，任何支持 MCP 的 Agent（如 OpenClaw、Claude、Codex）均可直接调用
- **作为 MCP Client：** 主动调用网络上任何 MCP Server 暴露的服务，包括其他 IoT 设备、PC 端及云端软件能力（如高德地图查询路况、调用飞书发送提醒）

**AI 原生语义接口：** 工具命名采用动词-名词结构（`turn_on`、`get_temperature`），返回值携带单位与新鲜度元信息，AI 无需外部文档即可理解和调用。

### 丰富外设接入：信息处理专家

ESP-Claw 支持接入摄像头、麦克风以及其他各类传感器，大部分驱动均可通过 ESP-IDF Component Registry 获取。内置的 Lua 模块包括：

- **显示与交互：** `lua_module_display`、`lua_module_lcd_touch`、`lua_module_button`
- **多媒体：** `lua_module_camera`、`lua_module_audio`
- **执行器：** `lua_module_led_strip`、`lua_module_gpio`、`lua_module_mcpwm`
- **系统工具：** `lua_module_storage`、`lua_module_delay`、`lua_module_event_publisher`


## 如何部署使用

### 在线烧录

通过网页即可完成配置与固件获取，无需额外编译固件或安装任何软件，即可开始烧录与使用。
[体验在线烧录](https://espressif.github.io/esp-claw/zh-cn/flash)

### 通过源码编译

[`basic_demo`](./application/basic_demo) 提供了基础示例。关于编译与烧录的详细说明，请参考[文档](https://espressif.github.io/esp-claw/zh-cn/tutorial/)

## 代码架构

项目采用 **"应用示例 + 通用组件"** 的组织方式：`application/basic_demo` 是可直接编译运行的 ESP-IDF 示例工程；`components` 则沉淀可复用的运行时核心、能力插件和硬件/脚本扩展模块。

当前代码分为四层：

- **应用装配层：** `application/basic_demo/main`，负责启动入口、网络连接、参数配置、HTTP 配网页面以及 Demo 级模块注册
- **能力层：** `components/claw_capabilities`，包括 IM 通讯、MCP Client/Server、Lua 运行时、调度、文件、时间、Web 搜索等能力
- **运行时核心层：** `components/claw_modules`，包括核心上下文、能力注册、事件路由、记忆管理与技能管理等
- **设备与脚本扩展层：** `components/lua_modules`，把显示屏、摄像头、音频、按键、GPIO、存储等外设能力暴露给 Lua 和上层 Agent

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

### 注意事项

- 当前项目仍处于开发阶段，如果遇到问题，欢迎随时提交 issue。
- 自编程等功能依赖高推理模型的能力，推荐选用 GPT-5.4, Qwen3.5-plus 或类似性能模型以取得最佳体验。

## 关注我们

如果这个项目对您有所启发和帮助，欢迎点亮一颗星！⭐⭐⭐⭐⭐

## 致谢

灵感来自 [OpenClaw](https://github.com/openclaw/openclaw)。

Agent Loop 和 IM 通讯等功能在 ESP32 上的实现参考了 [MimiClaw](https://github.com/memovai/mimiclaw)。
