import type { Lang } from "../site/locales";

export type { Lang };

export interface Strings {
  // Page header
  pageTitle: string;
  pageSubtitle: string;

  tabLockedWhileFlashing: string;

  connectBtn: string;
  disconnectBtn: string;
  connectingMsg: string;
  notConnected: string;
  connectedTo: string;
  webSerialUnsupported: string;

  // Firmware & Config (inside Flash tab)
  step2Title: string;
  noFirmwareMatch: string;
  // Required – Basic
  sectionBasic: string;
  sectionWifi: string;
  wifiSsidLabel: string;
  wifiPasswordLabel: string;
  timezoneLabel: string;
  timezonePlaceholder: string;
  // Required – LLM
  sectionLlm: string;
  llmModeLabel: string;
  llmOptionQwen: string;
  llmOptionOpenai: string;
  llmOptionAnthropic: string;
  llmOptionOpenaiCompat: string;
  llmOptionAnthropicCompat: string;
  llmGetApiKeyBtn: string;
  llmVendorDocsBtn: string;
  llmBaseUrlLabel: string;
  llmBaseUrlLabelAnthropicCompat: string;
  llmModelLabel: string;
  llmApiKeyLabel: string;
  // IM (dynamic channel list)
  sectionIm: string;
  imNote: string;
  imAddChannel: string;
  imRemoveChannel: string;
  imChannelQq: string;
  imChannelFeishu: string;
  imChannelTg: string;
  imChannelWechat: string;
  wechatQrNote: string;
  wechatQrStatusLabel: string;
  wechatQrRefreshBtn: string;
  wechatQrOpenLink: string;
  wechatQrExpiredOverlay: string;
  wechatQrTokenReady: string;
  wechatQrFetchError: string;
  wechatQrPollError: string;
  wechatThirdPartyNotice: string;
  qqAppIdLabel: string;
  qqAppSecretLabel: string;
  feishuAppIdLabel: string;
  feishuAppSecretLabel: string;
  tgBotTokenLabel: string;
  // Advanced
  sectionAdvanced: string;
  sectionSearch: string;
  searchNote: string;
  searchTipTitle: string;
  searchTipBody: string;
  searchTipRecommend: string;
  searchTavilyDashboardLabel: string;
  searchBraveLabel: string;
  searchTavilyLabel: string;

  tabFlash: string;
  tabConsole: string;
  flashBtn: string;
  flashBtnDisabledNoDevice: string;
  flashBtnDisabledNoFirmware: string;
  flashBtnDisabledNoConfig: string;
  flashSuccess: string;
  flashError: string;
  downloadingFirmware: string;
  generatingNvs: string;
  writingFlash: string;
  consolePlaceholder: string;
  consoleClearBtn: string;
  consoleResetBtn: string;
  progressLabel: string;

  // Quick-link cards above the flash tool
  quickLinkNoHardwareTitle: string;
  quickLinkNoHardwareDesc: string;
  quickLinkBoardsTitle: string;
  quickLinkBoardsDesc: string;
}

const en: Strings = {
  pageTitle: "Flash Online",
  pageSubtitle:
    "Connect your supported ESP series development board and flash ESP-Claw firmware directly from the browser.",

  tabLockedWhileFlashing: "Available when flashing finishes",
  connectBtn: "Connect",
  disconnectBtn: "Disconnect",
  connectingMsg: "Connecting…",
  notConnected: "No device connected",
  connectedTo: "Connected",
  webSerialUnsupported:
    "Web Serial API is not supported in this browser. Please use Chrome, Edge, or another Chromium-based browser.",

  step2Title: "Select Firmware",
  noFirmwareMatch:
    "No firmware available for this chip / flash size combination.",
  sectionBasic: "Basic Settings",
  sectionWifi: "Wi-Fi Configuration",
  wifiSsidLabel: "Wi-Fi SSID",
  wifiPasswordLabel: "Wi-Fi Password",
  timezoneLabel: "Timezone (POSIX TZ string)",
  timezonePlaceholder: "e.g. UTC0",
  sectionLlm: "LLM Configuration",
  llmModeLabel: "LLM service",
  llmOptionQwen: "Qwen (Aliyun Bailian)",
  llmOptionOpenai: "OpenAI",
  llmOptionAnthropic: "Claude (Anthropic)",
  llmOptionOpenaiCompat: "OpenAI-compatible API",
  llmOptionAnthropicCompat: "Claude-compatible API",
  llmGetApiKeyBtn: "Get API Key",
  llmVendorDocsBtn: "Vendor Docs",
  llmBaseUrlLabel: "API base URL (will add /v1/chat/completions path automatically)",
  llmBaseUrlLabelAnthropicCompat: "API base URL (will add /v1/messages path automatically)",
  llmModelLabel: "LLM Model",
  llmApiKeyLabel: "API Key",
  sectionIm: "Instant messaging (IM)",
  imNote: "Add zero or more channels. Credentials apply only to added channels.",
  imAddChannel: "Add channel",
  imRemoveChannel: "Remove",
  imChannelQq: "QQ",
  imChannelFeishu: "Feishu",
  imChannelTg: "Telegram",
  imChannelWechat: "WeChat",
  wechatQrNote:
    "After adding WeChat, a QR code is fetched automatically. Scan and confirm in WeChat to get token and base URL.",
  wechatQrStatusLabel: "Status",
  wechatQrRefreshBtn: "Refresh QR Code",
  wechatQrOpenLink: "Open QR Link",
  wechatQrExpiredOverlay: "QR code expired",
  wechatQrTokenReady: "WeChat token is ready and will be written to NVS.",
  wechatQrFetchError: "Failed to fetch QR code. Please check your network and try again.",
  wechatQrPollError: "Failed to check scan status. Please try refreshing the QR code.",
  wechatThirdPartyNotice:
    "Note: Scanning this QR code initiates verification through a third-party relay service. This service does not store your login credentials. If you have concerns, you can use the local verification in Web Config after flashing.",
  qqAppIdLabel: "QQ App ID",
  qqAppSecretLabel: "QQ App Secret",
  feishuAppIdLabel: "Feishu App ID",
  feishuAppSecretLabel: "Feishu App Secret",
  tgBotTokenLabel: "Telegram Bot Token",
  sectionAdvanced: "Advanced Settings",
  sectionSearch: "Search Engine",
  searchNote:
    "Search keys are optional, but at least one key is recommended for up-to-date web results.",
  searchTipTitle: "Tip:",
  searchTipBody:
    "Search engine config enables ESP-Claw to retrieve latest information from the web, including weather queries.",
  searchTipRecommend: "Tavily is recommended and provides a free tier.",
  searchTavilyDashboardLabel: "Open Tavily Dashboard",
  searchBraveLabel: "Brave Search API Key",
  searchTavilyLabel: "Tavily API Key",

  tabFlash: "Flash",
  tabConsole: "Console",
  flashBtn: "Flash Firmware",
  flashBtnDisabledNoDevice: "Connect a device first",
  flashBtnDisabledNoFirmware: "Select firmware first",
  flashBtnDisabledNoConfig: "Complete required fields",
  flashSuccess: "Flash complete! Device is rebooting.",
  flashError: "Flash failed: ",
  downloadingFirmware: "Downloading firmware…",
  generatingNvs: "Generating NVS partition…",
  writingFlash: "Writing to flash…",
  consolePlaceholder: "Send command…",
  consoleClearBtn: "Clear",
  consoleResetBtn: "Reset",
  progressLabel: "Progress",

  quickLinkNoHardwareTitle: "No hardware yet?",
  quickLinkNoHardwareDesc: "Assemble your ESP-Claw board",
  quickLinkBoardsTitle: "Have other ESP boards?",
  quickLinkBoardsDesc: "View supported board list",
};

const zhCn: Strings = {
  pageTitle: "在线烧录",
  pageSubtitle: "连接支持的 ESP 开发版，直接在浏览器中烧录 ESP-Claw 固件。",

  tabLockedWhileFlashing: "烧录结束后可切换到控制台",
  connectBtn: "连接",
  disconnectBtn: "断开",
  connectingMsg: "连接中…",
  notConnected: "未连接设备",
  connectedTo: "已连接",
  webSerialUnsupported:
    "当前浏览器不支持 Web Serial API，请使用 Chrome、Edge 或其他基于 Chromium 的浏览器。",

  step2Title: "选择固件",
  noFirmwareMatch: "当前芯片/Flash 大小没有匹配的固件。",
  sectionBasic: "基本设置",
  sectionWifi: "Wi-Fi 配置",
  wifiSsidLabel: "Wi-Fi 名称 (SSID)",
  wifiPasswordLabel: "Wi-Fi 密码",
  timezoneLabel: "时区（POSIX TZ 字符串）",
  timezonePlaceholder: "例如 UTC0",
  sectionLlm: "LLM 配置",
  llmModeLabel: "LLM 服务",
  llmOptionQwen: "Qwen (阿里云百炼)",
  llmOptionOpenai: "OpenAI",
  llmOptionAnthropic: "Claude (Anthropic)",
  llmOptionOpenaiCompat: "OpenAI 兼容 API",
  llmOptionAnthropicCompat: "Claude 兼容 API",
  llmGetApiKeyBtn: "获取 API Key",
  llmVendorDocsBtn: "厂商文档",
  llmBaseUrlLabel: "API Base URL (会自动添加 /v1/chat/completions 路径)",
  llmBaseUrlLabelAnthropicCompat: "API Base URL (会自动添加 /v1/messages 路径)",
  llmModelLabel: "LLM 模型",
  llmApiKeyLabel: "API 密钥",
  sectionIm: "即时通讯 (IM)",
  imNote: "可添加0个或多个渠道。仅对已添加的渠道填写凭据。",
  imAddChannel: "添加渠道",
  imRemoveChannel: "移除",
  imChannelQq: "QQ",
  imChannelFeishu: "飞书",
  imChannelTg: "Telegram",
  imChannelWechat: "微信",
  wechatQrNote: "添加微信后会自动获取二维码。请扫码并在微信中确认，以获取 token 与 base URL。",
  wechatQrStatusLabel: "状态",
  wechatQrRefreshBtn: "刷新二维码",
  wechatQrOpenLink: "打开二维码链接",
  wechatQrExpiredOverlay: "二维码已过期",
  wechatQrTokenReady: "微信 token 已就绪，烧录时会写入 NVS。",
  wechatQrFetchError: "获取二维码失败，请检查网络后重试。",
  wechatQrPollError: "查询扫码状态失败，请尝试刷新二维码。",
  wechatThirdPartyNotice:
    "提示：扫描此二维码时，验证过程将经由第三方中转服务完成，该服务不会记录您的登录凭据。如有顾虑，可在烧录完成后使用 Web Config 中的本地验证方式。",
  qqAppIdLabel: "QQ App ID",
  qqAppSecretLabel: "QQ App Secret",
  feishuAppIdLabel: "飞书 App ID",
  feishuAppSecretLabel: "飞书 App Secret",
  tgBotTokenLabel: "Telegram Bot Token",
  sectionAdvanced: "高级设置",
  sectionSearch: "搜索引擎配置",
  searchNote: "搜索密钥为可选项，但建议至少配置一个以获得最新联网检索结果。",
  searchTipTitle: "提示：",
  searchTipBody: "配置搜索引擎后，ESP-Claw 才能通过网络检索获取最新信息，查询天气也依赖搜索引擎支持。",
  searchTipRecommend: "推荐配置 Tavily，有一定的免费额度。",
  searchTavilyDashboardLabel: "打开 Tavily Dashboard",
  searchBraveLabel: "Brave Search API Key",
  searchTavilyLabel: "Tavily API Key",

  tabFlash: "烧录",
  tabConsole: "控制台",
  flashBtn: "开始烧录",
  flashBtnDisabledNoDevice: "请先连接设备",
  flashBtnDisabledNoFirmware: "请先选择固件",
  flashBtnDisabledNoConfig: "请完善必填字段",
  flashSuccess: "烧录完成！设备正在重启。",
  flashError: "烧录失败：",
  downloadingFirmware: "正在下载固件…",
  generatingNvs: "正在生成 NVS 分区…",
  writingFlash: "正在写入 Flash…",
  consolePlaceholder: "发送命令…",
  consoleClearBtn: "清空",
  consoleResetBtn: "重启",
  progressLabel: "进度",

  quickLinkNoHardwareTitle: "还没有硬件？",
  quickLinkNoHardwareDesc: "组装 ESP-Claw 开发版",
  quickLinkBoardsTitle: "有其他 ESP 开发版？",
  quickLinkBoardsDesc: "查看支持的开发板列表",
};

const strings: Record<Lang, Strings> = {
  en,
  "zh-cn": zhCn,
};

export function getStrings(lang: Lang): Strings {
  return strings[lang] ?? strings["en"];
}
