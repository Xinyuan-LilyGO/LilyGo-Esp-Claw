<div align="center" markdown="1">
  <img src="images/LilyGo_logo.png" alt="LilyGo logo" width="100"/>
</div>
<h1 align = "center">🌟LilyGo-Esp-Claw🌟</h1>

​								**[English](./README.md) | 中文**

<img src="images\esp-claw.png" alt="esp-claw" style="zoom:25%;" /> 

适配设备：

| T-Connect-Pro | [![alt text](https://camo.githubusercontent.com/aba00c21f632b6ee99f63dba2e64de19d88bea4ac4f4c1f1512ab13d0a72fbc2/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/aba00c21f632b6ee99f63dba2e64de19d88bea4ac4f4c1f1512ab13d0a72fbc2/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) |
| ------------- | ------------------------------------------------------------ |
|               |                                                              |
|               |                                                              |

# `1` Esp-Claw👋

* **ESP-Claw** 是面向物联网设备的 **Chat Coding（聊天造物）** 式 AI 智能体框架，以对话定义设备行为，在乐鑫芯片上本地完成感知、推理、决策与执行的完整闭环。

# `2` 快速开始

## 使用 Flash download tool 烧录固件

下载 [Flash_download_tool](https://dl.espressif.com/public/flash_download_tool.zip)

![web_flasher](C:\Users\Xinyuan\Desktop\esp-claw\images\esp_downloader.gif)

* 请注意，烧录完成后，您需要按下“RST”键进行重置。

## 使用在线 Web Flasher烧录固件

- [ESP Web Flasher Online](https://espressif.github.io/esptool-js/)

![web_flasher](C:\Users\Xinyuan\Desktop\esp-claw\images\web_flasher.gif)

- 请注意，烧录完成后，您需要按下“RST”键进行重置。

## 使用命令行烧录固件


如果系统提示要安装开发工具，请进行安装。

```bash
python3 -m pip install --upgrade pip
python3 -m pip install esptool
```

为了启动 esptool.py，直接使用以下命令执行即可：

```bash
python3 -m esptool
```

 ESP32-S3 请使用以下命令进行写入操作。

```bash
esptool --chip esp32s3  --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m 0x0 firmware.bin
```

# `3` Web 配置

1. 连接到已开启的设备的热点 Wi-Fi（设备名称：**LilyGo-EspClaw-xxxxxx**）

2. 在浏览器中打开：[后台](https://192.168.4.1) (192.168.4.1).

   > \[!提示]]
   >
   > 页面加载后会自动加载当前配置。页面支持多语言切换，以及显示 Wi-Fi 连接状态、设备 IP 等信息。

3. 设置 Wi-Fi 的网络名称（SSID）和密码。

   > \[!提示]
   >
   > 您的电脑或手机需与设备处于同一局域网内。设置好WiFi连接以后可以使用设备IP地址登录配置页。

4. 配置 LLM：可选 GPT (OpenAI), Qwen (阿里云百炼), Claude (Anthropic), DeepSeek (DeepSeek API)，请按提示生成 API Key 并填入。

   > \[!提示]
   >
   > 🤖 请选择推理能力较强的模型
   >
   > ESP-Claw 的自编程、复杂工具组合等功能依赖强推理模型的能力。 推荐选用 GPT-5.4、Qwen3.6-plus、Claude4.6-sonnet、DeepSeek v4 Pro 或类似性能的模型以取得最佳体验。

5. 配置需要连接到的聊天软件：目前支持 Telegram、QQ Bot（OpenClaw）、飞书与微信 ClawBot 等，可同时填写多个。

   | 聊天软件    | 配置项           | 帮助文档                                                     |
   | ----------- | ---------------- | ------------------------------------------------------------ |
   | Telegram    | Bot token        | [Telegram Bot 文档](https://core.telegram.org/bots#6-create-a-token) |
   | QQ Bot      | ID 和 secret     | [QQ Bot 网站](https://q.qq.com/qqbot/openclaw/)              |
   | 飞书        | App ID 和 secret | [飞书文档](https://www.feishu.cn/content/article/7602519239445974205) |
   | 微信ClawBot | 微信扫码自动配置 | N/A                                                          |

6. 配置搜索引擎：

   > \[!提示]
   >
   > 配置搜索引擎后，ESP-Claw 才能通过网络检索获取最新信息。查询天气也依赖搜索引擎支持。
   >
   > 推荐配置 Tavily，有一定的免费额度。

   打开 [Tavily Dashboard](https://app.tavily.com/), 注册账号并登录，复制一个 API Key 并填入网页烧录工具的「Tavily」输入框。

7. 保存修改后的内容后，按下复位按钮以重新启动，然后您就可以开始体验了。

**日常问答**

- 你好，介绍一下你当前有哪些能力？
- 最近有什么 AI 新闻？
- 帮我计算 12% 年化利率，3年后10000元会变成多少钱？

**控制硬件**

- 使连接到 GPIO8 的继电器模块每 2 秒自动开启和关闭一次，每次开启和关闭之间间隔 0.5 秒，并无限循环。
- 在显示屏上显示 "Hello， ESP-Claw!"，显示时间为 5 秒。

**询问记忆**

- 记住我喜欢简洁的回答。
- 你还记得我叫什么名字吗？



# `4` 页面布局

配置页分为三个 Tab：

- **Configuration**：系统配置（Wi-Fi、LLM、IM、搜索、时区等）
- **Memory**：记忆文件管理（长期记忆与画像记忆）
- **File Manager**：文件系统管理

### 系统配置

#### LLM 供应商选择

配置页提供 **LLM Provider** 下拉菜单，选择后会自动填写对应的 `Profile`、`Base URL`、`Backend Type` 和 `Auth Type`：

| Provider        | Profile           | Base URL                                            |
| --------------- | ----------------- | --------------------------------------------------- |
| OpenAI          | `openai`          | `https://api.openai.com/v1`                         |
| Qwen Compatible | `qwen_compatible` | `https://dashscope.aliyuncs.com/compatible-mode/v1` |
| Anthropic       | `anthropic`       | `https://api.anthropic.com/v1`                      |
| Custom          | 自定义            | 自定义                                              |

选择供应商后，只需填写 `API Key` 和 `Model` 即可。

1. 启用微信 ClawBot 接口

   目前，微信以插件的形式提供 ClawBot 功能，需先按照下述步骤在微信中启用，并在 Web 配置页面登录：

   1. **启用微信 ClawBot 插件**

      微信「我」→「设置」→「插件」→「ClawBot」，启用此插件。

      提示

      若找不到 ClawBot 插件，请尝试更新微信到最新版本。

   2. **在 Web 配置页面登录**

      打开 Web 配置页面，可通过以下两种方式登录：

      - **扫码登录**：点击「Generate QR」生成二维码，使用微信「我」→「设置」→「插件」→「ClawBot」→「微信扫一扫」扫描二维码。
      - **链接登录**：点击「Generate QR」生成二维码后，点击「Open login link」在新窗口打开登录链接，按提示完成登录。

   3. **登录成功**

      登录成功后，Web 配置页面将有相应提示。

   4. **保存配置**

      点击「Save Changes」按钮保存全部配置。

   > \[!提示]
   >
   > 虽然 Web 配置界面可以修改 WeChat Base URL & WeChat CDN Base URL，但现阶段微信仅可使用默认的 Base URL，请勿修改。

### 记忆管理

记忆管理页面用于查看记忆系统中的长期记忆、管理「灵魂」、「身份」与「用户信息」。

- **长期记忆**是系统根据结构化记忆生成的人类可读文件。修改该文件无法直接改变结构化记忆，因此仅提供只读查看功能。
- **灵魂**、**身份**与**用户信息**是可编辑的文件，用于记录设备与用户的个性化信息。

在与 ESP-Claw 对话后，你可以点击「Refresh」按钮或「Refresh All」按钮，刷新记忆文件，查看最新的记忆内容。

### 文件管理

Web 配置页面提供了简单的文件管理功能，可以在不通过 LLM 的情况下直接查阅和读写文件系统中的文件。

默认情况下，文件管理为只读模式，仅当启用「管理员模式」（Dev Mode）后，才能进行写操作。 启用「管理员模式」后，可用于手动添加 Skill、修改自动化规则、修改 Lua 脚本等。

> \[!注意]
>
> 启用「管理员模式」后，请谨慎操作。误删重要文件、JSON 不合法等操作可能导致系统无法正常运行，甚至无限重启。

# `5 `FAQ

### 启动后没有连上 Wi-Fi

**常见原因：**

- SSID/密码错误：两者均区分大小写。
- 路由器频段不兼容：除 ESP32-C5 外，其他芯片暂仅支持 2.4GHz Wi-Fi。
- Wi-Fi 信号过差。

**排查建议：**

1. 通过 SoftAP 下的 Web 配置页，修改 Wi-Fi SSID 与密码，连接到 2.4G Wi-Fi。

### ESP-Claw 无法收发 IM 消息

**常见原因：**

- IM 配置参数不正确，或者 Secret/Token 过期。
- ESP-Claw 无法访问 IM 平台 API：使用 Telegram Bot API 通常需要较好的网络环境。
- 飞书等平台 Bot 权限配置不正确。

**排查建议：**

1. 观察日志中的 HTTP 状态与错误消息。
2. 重新配置 IM 相关参数，配置完成后重启设备。
3. 检查飞书 Bot 权限是否配置正确。

