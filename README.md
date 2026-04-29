<div align="center" markdown="1">
  <img src="images/LilyGo_logo.png" alt="LilyGo logo" width="100"/>
</div>
<h1 align = "center">🌟LilyGo-Esp-Claw🌟</h1>

​								**English | [中文](./README_CN.md)**

<img src="images\esp-claw.png" alt="esp-claw" style="zoom:25%;" /> Compatible device：

| T-Connect-Pro | [![alt text](https://camo.githubusercontent.com/aba00c21f632b6ee99f63dba2e64de19d88bea4ac4f4c1f1512ab13d0a72fbc2/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/aba00c21f632b6ee99f63dba2e64de19d88bea4ac4f4c1f1512ab13d0a72fbc2/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) |
| ------------- | ------------------------------------------------------------ |
|               |                                                              |
|               |                                                              |

# `1` Esp-Claw👋

* ESP-Claw is a Chat Coding AI agent framework for IoT devices. It defines device behavior through conversation and completes the full loop of sensing, reasoning, decision-making, and execution locally on Espressif chips.

# `2` Quick Start

## Use ESP Download Tool

Download [Flash_download_tool](https://dl.espressif.com/public/flash_download_tool.zip)

![web_flasher](images/esp_downloader.gif)

* Note that after writing is completed, you need to press RST to reset.

## Use Web Flasher

- [ESP Web Flasher Online](https://espressif.github.io/esptool-js/)

![web_flasher](images/web_flasher.gif)

- Note that after writing is completed, you need to press RST to reset.

## Use command line


If system asks about install Developer Tools, do it.

```bash
python3 -m pip install --upgrade pip
python3 -m pip install esptool
```

In order to launch esptool.py, exec directly with this:

```bash
python3 -m esptool
```

For ESP32-S3 use the following command to write

```bash
esptool --chip esp32s3  --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m 0x0 firmware.bin
```

# `3` Web configuration

1. Connect to the hotspot Wi-Fi of the device that has been turned on (name: **LilyGo-EspClaw-xxxxxx**)

2. In the browser open: [link](https://192.168.4.1) (192.168.4.1).

   > \[!TIP]
   >
   > The page loads current settings automatically. The UI supports multi-language switching and displays Wi-Fi status, device IP, and other information.

3. Configure Wi-Fi SSID and password.

   > \[!TIP]
   >
   > Your computer or phone must be on the same LAN as the board.After setting up the WiFi connection, you can log in to the configuration page using the device's IP address.

4. Configure LLM: Supports GPT (OpenAI), Qwen (Alibaba Bailian), Claude (Anthropic), and DeepSeek (DeepSeek API). Generate an API Key as prompted and enter it.

   > \[!TIP]
   >
   > 🤖 Please select a model with strong reasoning ability
   >
   > ESP-Claw’s self-programming, complex tool combination, and other advanced features rely on the ability of strong reasoning models. We recommend using GPT-5.4, Qwen3.6-plus, Claude4.6-sonnet, DeepSeek v4 Pro, or similar performance models to get the best experience.

5. Configure chat apps to connect: currently supports Telegram, QQ Bot (OpenClaw), Feishu, and WeChat ClawBot. You can fill several at once.

   | Chat app       | Setting                                    | Help                                                         |
   | -------------- | ------------------------------------------ | ------------------------------------------------------------ |
   | Telegram       | Bot token                                  | [Telegram Bot docs](https://core.telegram.org/bots#6-create-a-token) |
   | QQ Bot         | ID and secret                              | [QQ Bot site](https://q.qq.com/qqbot/openclaw/)              |
   | Feishu         | App ID and secret                          |
   | WeChat ClawBot | Automatic configuration via WeChat QR scan | N/A                                                          |

6. Configure search engine:

   > \[!TIP]
   >
   > After a search engine is configured, ESP-Claw can retrieve up-to-date information from the web. Weather queries also depend on search engine support.
   >
   > Tavily is recommended and provides a free quota.

   Open [Tavily Dashboard](https://app.tavily.com/), sign up and sign in, copy an API Key, then paste it into the Tavily field in the web flasher.

7. After saving the modified content, press the reset button to restart, and then you can start experiencing it.

**Everyday Q&A**

- Hi, what capabilities do you currently have?
- Any AI news lately?
- At 12% annual interest, what will 10,000 become after 3 years?

**Hardware control**

- Make a relay module connected to GPIO8 automatically switch on and off every 2 seconds, with an interval of 0.5 seconds between each on and off, and keep looping indefinitely.
- Draw Hello, ESP-Claw! on the display，Display for 5 seconds.

**Memory**

- Remember that I prefer concise answers
- Do you still remember my name?



# `4` Page layout

The configuration page is divided into three tabs:

- **Configuration**: System settings (Wi-Fi, LLM, IM, search, time zone, etc.)
- **Memory**: Memory file management (long-term memory and profile memory)
- **File Manager**: Filesystem management

### System settings

#### LLM provider selection

The page provides an **LLM Provider** dropdown menu. Selecting a provider automatically fills in the corresponding `Profile`, `Base URL`, `Backend Type`, and `Auth Type`:

| Provider        | Profile           | Base URL                                            |
| --------------- | ----------------- | --------------------------------------------------- |
| OpenAI          | `openai`          | `https://api.openai.com/v1`                         |
| Qwen Compatible | `qwen_compatible` | `https://dashscope.aliyuncs.com/compatible-mode/v1` |
| Anthropic       | `anthropic`       | `https://api.anthropic.com/v1`                      |
| Custom          | Custom            | Custom                                              |

After choosing a provider, you only need to fill in your `API Key` and `Model`.

#### Enable WeChat ClawBot

WeChat ships ClawBot as a plugin—enable it in WeChat first, then log in via the Web config page:

1. **Enable the WeChat ClawBot plugin**

   WeChat Me → Settings → Plugins → ClawBot, and turn it on.

   > \[!TIP]
   >
   > If you cannot find the plugin, update WeChat to the latest version.

2. **Log in on the Web config page**

   Open the Web config page. You can log in via two ways:

   - **Scan QR login**: Click 「Generate QR」 to create a QR code, then in WeChat Me → Settings → Plugins → ClawBot, use Scan to read the code.
   - **Link login**: Click 「Generate QR」 to create a QR code first, then click 「Open login link」 to open the login page in a new window and follow the prompts.

3. **Signed in**

   After login, the Web page shows a confirmation.

4. **Save**

   Click Save Changes to persist all settings.

   > \[!TIP]
   >
   > The UI can edit WeChat Base URL & WeChat CDN Base URL, but WeChat currently only works with the default base URL—do not change it.

### Memory manager

The Memory manager page is used to view and manage the long-term memory, soul, identity, and user information in the memory system.

- **Long-term memory** is a human-readable file generated by the system based on structured memory. Modifying this file does not directly change the structured memory, so it only provides a read-only view function.
- **Soul**, **Identity**, and **User Information** are editable files used to record the personalized information of the device and the user.

After chatting with ESP-Claw, you can click the “Refresh” button or “Refresh All” button to refresh the memory files and view the latest memory content.

### File manager

The Web UI includes a simple file manager so you can browse and read/write files directly, without going through the LLM.

By default, the file manager is read-only. Write operations are available only after enabling Admin mode (Dev Mode). After enabling Admin mode, you can manually add Skills, edit automation rules, modify Lua scripts, and more.

> \[!Warning]
>
> After enabling Admin mode, operate carefully. Deleting critical files or writing invalid JSON may cause system malfunction or even reboot loops.

# `5 `FAQ

### Wi‑Fi does not connect after boot

**Typical causes:**

- Wrong SSID/password (case-sensitive).
- Router band mismatch: except ESP32-C5, only 2.4 GHz Wi‑Fi is supported today.
- Weak signal.

**What to try:**

1. Open the Web config page under SoftAP, update Wi-Fi SSID and password, and connect to 2.4 GHz Wi-Fi.

### ESP-Claw cannot send or receive IM

**Typical causes:**

- Wrong IM credentials or expired secret/token.
- Device cannot reach IM platform APIs (Telegram often needs solid connectivity).
- Feishu bot permissions misconfigured.

**What to try:**

1. Inspect HTTP status/errors in logs.
2. Re-enter IM settings and reboot.
3. Verify Feishu bot permissions.









