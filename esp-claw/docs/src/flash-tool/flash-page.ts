/// <reference types="w3c-web-serial" />
import { generate } from "lean-qr";
import { ESPLoader, Transport } from "esptool-js";
import type { Lang, Strings } from "../flash-tool/i18n";
import type { FirmwareDb, FirmwareEntry } from "../flash-tool/firmware-db";
import { getFirmwareList, parseAddr } from "../flash-tool/firmware-db";
import { generateNvsPartition, type NvsConfig } from "../flash-tool/nvs-gen";

interface FlashBoot {
  lang: Lang;
  firmwareDb: FirmwareDb;
  strings: Strings;
}

function readBoot(): FlashBoot {
  const raw = document.getElementById("flash-boot")?.textContent ?? "{}";
  return JSON.parse(raw) as FlashBoot;
}

const { firmwareDb, strings: s } = readBoot();

function sleep(ms: number) {
  return new Promise<void>((r) => setTimeout(r, ms));
}

/** ESPLoader keeps a background readLoop() with an exclusive reader; raw console read must stop it first.
 * Transport declares `reader` as private, so we narrow via `unknown` instead of intersecting (which becomes `never`). */
async function stopEsptoolReadLoop(t: InstanceType<typeof Transport> | null) {
  if (!t) return;
  const reader = (t as unknown as { reader?: ReadableStreamDefaultReader<Uint8Array> }).reader;
  try {
    await reader?.cancel();
  } catch {}
  for (let i = 0; i < 30; i++) {
    await t.waitForUnlock(50);
    const { readable } = t.device;
    if (!readable || !readable.locked) break;
  }
}

let chipInfo: { chipName: string; flashSizeMB: number } | null = null;
let selectedFirmware: FirmwareEntry | null = null;
/** True while ESPLoader's readLoop is expected to be running (connect / flash tab). */
let esptoolReadLoopRunning = false;
let consoleActive = false;
let isFlashing = false;
let transport: InstanceType<typeof Transport> | null = null;
let loader: InstanceType<typeof ESPLoader> | null = null;

let consoleReader: ReadableStreamDefaultReader<Uint8Array> | null = null;
let consoleReadPromise: Promise<void> | null = null;

const connectBtn = document.getElementById("connect-btn") as HTMLButtonElement;
const connectBtnOriginalHTML = connectBtn.innerHTML;
const disconnectBtn = document.getElementById("disconnect-btn") as HTMLButtonElement;
const connectBar = document.getElementById("connect-bar") as HTMLDivElement;
const connectBarIdle = document.getElementById("connect-bar-idle") as HTMLDivElement;
const connectBarActive = document.getElementById("connect-bar-active") as HTMLDivElement;
const mainContent = document.getElementById("main-content") as HTMLDivElement;
const statusBadge = document.getElementById("status-badge") as HTMLSpanElement;
const statusText = document.getElementById("status-text") as HTMLSpanElement;
const infoChip = document.getElementById("info-chip") as HTMLSpanElement;
const infoFlash = document.getElementById("info-flash") as HTMLSpanElement;
const firmwareGrid = document.getElementById("firmware-grid") as HTMLDivElement;
const firmwareDesc = document.getElementById("firmware-desc") as HTMLDivElement;
const noFirmwareMsg = document.getElementById("no-firmware-msg") as HTMLParagraphElement;
const configForm = document.getElementById("config-form") as HTMLDivElement;
const flashBtn = document.getElementById("flash-btn") as HTMLButtonElement;
const flashHint = document.getElementById("flash-hint") as HTMLSpanElement;
const progressWrap = document.getElementById("progress-wrap") as HTMLDivElement;
const progressBar = document.getElementById("progress-bar") as HTMLDivElement;
const progressStage = document.getElementById("progress-stage") as HTMLSpanElement;
const progressPct = document.getElementById("progress-pct") as HTMLSpanElement;
const progressLog = document.getElementById("progress-log") as HTMLDivElement;
const flashResult = document.getElementById("flash-result") as HTMLDivElement;
const tabFlashBtn = document.getElementById("tab-flash-btn") as HTMLButtonElement;
const tabConsoleBtn = document.getElementById("tab-console-btn") as HTMLButtonElement;
const tabFlash = document.getElementById("tab-flash") as HTMLDivElement;
const tabConsole = document.getElementById("tab-console") as HTMLDivElement;
const consoleOutput = document.getElementById("console-output") as HTMLDivElement;
const consoleInput = document.getElementById("console-input") as HTMLInputElement;
const consoleSendBtn = document.getElementById("console-send-btn") as HTMLButtonElement;
const consoleClearBtn = document.getElementById("console-clear-btn") as HTMLButtonElement;
const consoleResetBtn = document.getElementById("console-reset-btn") as HTMLButtonElement;
const unsupportedBanner = document.getElementById("unsupported-banner") as HTMLDivElement;
const wifiPasswordInput = document.getElementById("wifi_password") as HTMLInputElement | null;
const timezoneInput = document.getElementById("time_timezone") as HTMLInputElement | null;
const llmModeSelect = document.getElementById("llm_mode") as HTMLSelectElement | null;
const llmVendorActions = document.getElementById("llm_vendor_actions") as HTMLDivElement | null;
const llmApiKeyLink = document.getElementById("llm_api_key_link") as HTMLAnchorElement | null;
const llmDocsLink = document.getElementById("llm_docs_link") as HTMLAnchorElement | null;
const imChannelList = document.getElementById("im-channel-list") as HTMLDivElement | null;
const imAddBtn = document.getElementById("im-add-btn") as HTMLButtonElement | null;
const imAddMenu = document.getElementById("im-add-menu") as HTMLDivElement | null;
const WECHAT_ILINK_BASE_URL =
  (import.meta.env.PUBLIC_WECHAT_ILINK_BASE_URL as string | undefined)?.trim().replace(/\/+$/, "") ||
  "https://ilinkai.weixin.qq.com";
const WECHAT_BOT_TYPE = 3;
const WECHAT_POLL_INTERVAL_MS = 1500;
const WECHAT_QR_SIZE = 180;

// ─── IM channel state ─────────────────────────────────────────────────────────

/** Channels currently added by the user (in insertion order). */
const activeChannels: string[] = [];

interface ImChannelDef {
  fields: Array<{ id: string; labelKey: keyof typeof s; type: string; full?: boolean }>;
}

const IM_CHANNEL_DEFS: Record<string, ImChannelDef> = {
  qq: {
    fields: [
      { id: "qq_app_id",     labelKey: "qqAppIdLabel",     type: "text" },
      { id: "qq_app_secret", labelKey: "qqAppSecretLabel", type: "password" },
    ],
  },
  feishu: {
    fields: [
      { id: "feishu_app_id",     labelKey: "feishuAppIdLabel",     type: "text" },
      { id: "feishu_app_secret", labelKey: "feishuAppSecretLabel", type: "password" },
    ],
  },
  tg: {
    fields: [
      { id: "tg_bot_token", labelKey: "tgBotTokenLabel", type: "password", full: true },
    ],
  },
  wechat: {
    fields: [],
  },
};

type WechatQrPhase =
  | "idle"
  | "fetching_qr"
  | "waiting_scan"
  | "scanned"
  | "redirected"
  | "expired"
  | "confirmed"
  | "error";

interface WechatLoginState {
  qrcode: string;
  qrDataUrl: string;
  currentApiBaseUrl: string;
  token: string;
  baseUrl: string;
  phase: WechatQrPhase;
  message: string;
  expired: boolean;
  polling: boolean;
  fetchInFlight: boolean;
  pollTimer: number | null;
}

const wechatLoginState: WechatLoginState = {
  qrcode: "",
  qrDataUrl: "",
  currentApiBaseUrl: WECHAT_ILINK_BASE_URL,
  token: "",
  baseUrl: "",
  phase: "idle",
  message: "",
  expired: false,
  polling: false,
  fetchInFlight: false,
  pollTimer: null,
};

function clearWechatPollTimer() {
  if (wechatLoginState.pollTimer !== null) {
    window.clearTimeout(wechatLoginState.pollTimer);
    wechatLoginState.pollTimer = null;
  }
}

function stopWechatPolling() {
  clearWechatPollTimer();
  wechatLoginState.polling = false;
}

function resetWechatLoginState(): void {
  stopWechatPolling();
  wechatLoginState.qrcode = "";
  wechatLoginState.qrDataUrl = "";
  wechatLoginState.currentApiBaseUrl = WECHAT_ILINK_BASE_URL;
  wechatLoginState.token = "";
  wechatLoginState.baseUrl = "";
  wechatLoginState.phase = "idle";
  wechatLoginState.message = "";
  wechatLoginState.expired = false;
  wechatLoginState.fetchInFlight = false;
}

function getWechatPhaseLabel(phase: WechatQrPhase): string {
  switch (phase) {
    case "fetching_qr":
      return "fetching_qr";
    case "waiting_scan":
      return "waiting_scan";
    case "scanned":
      return "scanned";
    case "redirected":
      return "redirected";
    case "expired":
      return "expired";
    case "confirmed":
      return "confirmed";
    case "error":
      return "error";
    default:
      return "idle";
  }
}

function drawWechatQr(canvas: HTMLCanvasElement, content: string): void {
  if (!canvas || !content) return;
  const pixelRatio = Math.max(2, Math.ceil(window.devicePixelRatio || 1));
  const renderSize = WECHAT_QR_SIZE * pixelRatio;
  canvas.width = renderSize;
  canvas.height = renderSize;
  canvas.style.width = `${WECHAT_QR_SIZE}px`;
  canvas.style.height = `${WECHAT_QR_SIZE}px`;
  const code = generate(content);
  const sourceCanvas = document.createElement("canvas");
  code.toCanvas(sourceCanvas);
  const ctx = canvas.getContext("2d");
  if (!ctx) return;
  ctx.clearRect(0, 0, renderSize, renderSize);
  // Keep hard QR edges after upscaling; smoothing makes scanners harder to read.
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(sourceCanvas, 0, 0, sourceCanvas.width, sourceCanvas.height, 0, 0, renderSize, renderSize);
}

function renderWechatLoginPanel(container: HTMLDivElement): void {
  const notice = document.createElement("div");
  notice.className = "wechat-notice";
  const noticeIcon = document.createElementNS("http://www.w3.org/2000/svg", "svg");
  noticeIcon.setAttribute("viewBox", "0 0 24 24");
  noticeIcon.setAttribute("aria-hidden", "true");
  noticeIcon.setAttribute("focusable", "false");
  noticeIcon.classList.add("wechat-notice-icon");
  noticeIcon.innerHTML = `
    <path d="M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0Z"></path>
    <line x1="12" y1="9" x2="12" y2="13"></line>
    <line x1="12" y1="17" x2="12.01" y2="17"></line>
  `;
  const noticeText = document.createElement("span");
  noticeText.textContent = s.wechatThirdPartyNotice;
  notice.appendChild(noticeIcon);
  notice.appendChild(noticeText);
  container.appendChild(notice);

  const wrap = document.createElement("div");
  wrap.className = "wechat-qr-wrap";

  const qrBox = document.createElement("div");
  qrBox.className = "wechat-qr-box";
  qrBox.classList.toggle("expired", wechatLoginState.expired);

  const canvas = document.createElement("canvas");
  canvas.className = "wechat-qr-canvas";
  canvas.style.width = `${WECHAT_QR_SIZE}px`;
  canvas.style.height = `${WECHAT_QR_SIZE}px`;
  qrBox.appendChild(canvas);

  const overlay = document.createElement("button");
  overlay.className = "wechat-qr-overlay";
  overlay.type = "button";
  overlay.textContent = s.wechatQrRefreshBtn;
  qrBox.appendChild(overlay);

  if (wechatLoginState.qrDataUrl) {
    try {
      drawWechatQr(canvas, wechatLoginState.qrDataUrl);
    } catch (error) {
      console.error("[flash-tool] Failed to render WeChat QR code", error);
    }
  } else {
    const ctx = canvas.getContext("2d");
    ctx?.clearRect(0, 0, canvas.width, canvas.height);
  }

  overlay.disabled = wechatLoginState.fetchInFlight;
  overlay.addEventListener("click", () => {
    void fetchWechatQrCode(true);
  });

  if (wechatLoginState.phase === "error" && wechatLoginState.message) {
    const errBox = document.createElement("div");
    errBox.className = "wechat-error-box";
    const errText = document.createElement("span");
    errText.textContent = wechatLoginState.message;
    errBox.appendChild(errText);
    wrap.appendChild(errBox);
  }

  wrap.appendChild(qrBox);

  const meta = document.createElement("div");
  meta.className = "wechat-qr-meta";

  const status = document.createElement("div");
  status.className = "wechat-qr-status";
  const phaseLabel = getWechatPhaseLabel(wechatLoginState.phase);
  status.textContent = `${s.wechatQrStatusLabel}: ${phaseLabel}`;
  meta.appendChild(status);

  if (wechatLoginState.token) {
    const ready = document.createElement("div");
    ready.className = "wechat-qr-ready";
    ready.textContent = s.wechatQrTokenReady;
    meta.appendChild(ready);
  }

  const actions = document.createElement("div");
  actions.className = "wechat-qr-actions";
  if (wechatLoginState.qrDataUrl) {
    const openLink = document.createElement("a");
    openLink.className = "wechat-qr-link";
    openLink.href = wechatLoginState.qrDataUrl;
    openLink.target = "_blank";
    openLink.rel = "noreferrer";
    openLink.textContent = s.wechatQrOpenLink;
    actions.appendChild(openLink);
  }
  meta.appendChild(actions);

  wrap.appendChild(meta);
  container.appendChild(wrap);
}

function scheduleWechatStatusPoll(delayMs = WECHAT_POLL_INTERVAL_MS) {
  clearWechatPollTimer();
  if (!activeChannels.includes("wechat") || !wechatLoginState.qrcode || wechatLoginState.expired) {
    wechatLoginState.polling = false;
    return;
  }
  wechatLoginState.polling = true;
  wechatLoginState.pollTimer = window.setTimeout(() => {
    void pollWechatQrStatus();
  }, delayMs);
}

async function fetchWechatQrCode(force = false): Promise<void> {
  if (!activeChannels.includes("wechat")) return;
  if (wechatLoginState.fetchInFlight) return;
  if (!force && wechatLoginState.qrcode && !wechatLoginState.expired) return;

  wechatLoginState.fetchInFlight = true;
  wechatLoginState.phase = "fetching_qr";
  wechatLoginState.message = "";
  wechatLoginState.expired = false;
  wechatLoginState.token = "";
  wechatLoginState.baseUrl = "";
  wechatLoginState.currentApiBaseUrl = WECHAT_ILINK_BASE_URL;
  stopWechatPolling();
  renderImChannels();
  updateFlashBtn();

  try {
    const endpoint = `${WECHAT_ILINK_BASE_URL}/ilink/bot/get_bot_qrcode?bot_type=${WECHAT_BOT_TYPE}`;
    const resp = await fetch(endpoint, { cache: "no-store" });
    const data = await resp.json();
    if (!resp.ok || data?.ret !== 0 || !data?.qrcode || !data?.qrcode_img_content) {
      throw new Error(`HTTP ${resp.status} — ret=${data?.ret ?? "?"}`);
    }

    wechatLoginState.qrcode = String(data.qrcode);
    wechatLoginState.qrDataUrl = String(data.qrcode_img_content);
    wechatLoginState.phase = "waiting_scan";
    scheduleWechatStatusPoll(1000);
  } catch (error) {
    wechatLoginState.phase = "error";
    const detail = error instanceof Error ? error.message : String(error);
    wechatLoginState.message = `${s.wechatQrFetchError} (${detail})`;
    stopWechatPolling();
  } finally {
    wechatLoginState.fetchInFlight = false;
    renderImChannels();
    updateFlashBtn();
  }
}

async function pollWechatQrStatus(): Promise<void> {
  if (!activeChannels.includes("wechat")) {
    stopWechatPolling();
    return;
  }
  if (!wechatLoginState.qrcode) {
    stopWechatPolling();
    return;
  }

  const apiBase = wechatLoginState.currentApiBaseUrl || WECHAT_ILINK_BASE_URL;
  const endpoint = `${apiBase}/ilink/bot/get_qrcode_status?qrcode=${encodeURIComponent(wechatLoginState.qrcode)}`;

  try {
    const resp = await fetch(endpoint, { cache: "no-store" });
    const data = await resp.json();
    if (!resp.ok) {
      throw new Error(`failed to get WeChat QR status: ${resp.status}`);
    }

    const status = String(data?.status || "");
    if (status === "wait") {
      wechatLoginState.phase = "waiting_scan";
    } else if (status === "scanned") {
      wechatLoginState.phase = "scanned";
    } else if (status === "scaned_but_redirect") {
      const redirectHost = String(data?.redirect_host || "").trim();
      if (redirectHost) {
        wechatLoginState.currentApiBaseUrl = `https://${redirectHost}`;
      }
      wechatLoginState.phase = "redirected";
    } else if (status === "expired") {
      wechatLoginState.phase = "expired";
      wechatLoginState.expired = true;
      stopWechatPolling();
    } else if (status === "confirmed") {
      const token = String(data?.bot_token || "").trim();
      if (token) {
        wechatLoginState.token = token;
      }
      const baseurl = String(data?.baseurl || "").trim();
      wechatLoginState.baseUrl = baseurl || wechatLoginState.currentApiBaseUrl || WECHAT_ILINK_BASE_URL;
      wechatLoginState.phase = "confirmed";
      wechatLoginState.expired = false;
      stopWechatPolling();
    } else {
      wechatLoginState.phase = "error";
      wechatLoginState.message = `unexpected status: ${status || "unknown"}`;
      stopWechatPolling();
    }
  } catch (error) {
    wechatLoginState.phase = "error";
    const detail = error instanceof Error ? error.message : String(error);
    wechatLoginState.message = `${s.wechatQrPollError} (${detail})`;
    stopWechatPolling();
  } finally {
    renderImChannels();
    updateFlashBtn();
    if (wechatLoginState.polling) {
      scheduleWechatStatusPoll();
    }
  }
}

function renderImChannels(): void {
  if (!imChannelList) return;
  imChannelList.innerHTML = "";

  for (const channelId of activeChannels) {
    const def = IM_CHANNEL_DEFS[channelId];
    if (!def) continue;

    // Resolve channel display name from the menu item data-channel attribute
    const menuItem = imAddMenu?.querySelector<HTMLButtonElement>(`[data-channel="${channelId}"]`);
    const channelName = menuItem?.textContent?.trim() ?? channelId;

    const card = document.createElement("div");
    card.className = "im-channel-card";
    card.dataset.channel = channelId;

    const header = document.createElement("div");
    header.className = "im-channel-card-header";
    const nameSpan = document.createElement("span");
    nameSpan.className = "im-channel-name";
    nameSpan.textContent = channelName;
    const removeBtn = document.createElement("button");
    removeBtn.type = "button";
    removeBtn.className = "im-channel-remove";
    removeBtn.textContent = s.imRemoveChannel;
    removeBtn.dataset.remove = channelId;
    removeBtn.addEventListener("click", () => {
      const idx = activeChannels.indexOf(channelId);
      if (idx !== -1) activeChannels.splice(idx, 1);
      if (channelId === "wechat") {
        resetWechatLoginState();
      }
      renderImChannels();
      updateFlashBtn();
    });
    header.appendChild(nameSpan);
    header.appendChild(removeBtn);
    card.appendChild(header);

    const body = document.createElement("div");
    body.className = "im-channel-card-body";

    if (channelId === "wechat") {
      renderWechatLoginPanel(body);
    } else if (def.fields.length > 0) {
      const grid = document.createElement("div");
      grid.className = "form-grid";
      for (const field of def.fields) {
        const fg = document.createElement("div");
        fg.className = field.full || def.fields.length === 1 ? "form-group full" : "form-group";
        const lbl = document.createElement("label");
        lbl.htmlFor = field.id;
        lbl.innerHTML = `${s[field.labelKey] as string} <span class="required">*</span>`;
        const inp = document.createElement("input");
        inp.type = field.type;
        inp.id = field.id;
        inp.name = field.id;
        inp.autocomplete = "off";
        inp.addEventListener("input", updateFlashBtn);
        fg.appendChild(lbl);
        fg.appendChild(inp);
        grid.appendChild(fg);
      }
      body.appendChild(grid);
    }

    card.appendChild(body);
    imChannelList.appendChild(card);
  }

  // Update menu: disable already-added channels
  imAddMenu?.querySelectorAll<HTMLButtonElement>(".im-add-menu-item").forEach((btn) => {
    btn.disabled = activeChannels.includes(btn.dataset.channel ?? "");
  });
}

// Toggle add-channel dropdown
imAddBtn?.addEventListener("click", (e) => {
  e.stopPropagation();
  if (!imAddMenu) return;
  const open = imAddMenu.style.display !== "none";
  imAddMenu.style.display = open ? "none" : "block";
});

// Close dropdown when clicking outside
document.addEventListener("click", () => {
  if (imAddMenu) imAddMenu.style.display = "none";
});

// Handle menu item clicks
imAddMenu?.addEventListener("click", (e) => {
  const btn = (e.target as HTMLElement).closest<HTMLButtonElement>(".im-add-menu-item");
  if (!btn || btn.disabled) return;
  const channelId = btn.dataset.channel;
  if (!channelId || activeChannels.includes(channelId)) return;
  activeChannels.push(channelId);
  if (imAddMenu) imAddMenu.style.display = "none";
  renderImChannels();
  if (channelId === "wechat") {
    void fetchWechatQrCode();
  }
  updateFlashBtn();
});

const llmModelInput = document.getElementById("llm_model") as HTMLInputElement | null;
let llmModelUserEdited = false;
llmModelInput?.addEventListener("input", () => {
  const current = llmModelInput!.value.trim();
  const allDefaults = Object.values(LLM_DEFAULT_MODELS);
  llmModelUserEdited = current !== "" && !allDefaults.includes(current);
});

const LLM_DEFAULT_MODELS: Record<string, string> = {
  qwen: "qwen3.6-plus",
  openai: "gpt-5.4",
  anthropic: "claude-sonnet-4-6",
};

const LLM_VENDOR_LINKS = {
  qwen: {
    apiKey: "https://bailian.console.aliyun.com/?tab=model#/api-key",
    docs: "https://bailian.console.aliyun.com/cn-beijing?tab=doc#/doc/?type=model&url=2840915",
  },
  openai: {
    apiKey: "https://platform.openai.com/api-keys",
    docs: "https://developers.openai.com/api/reference/overview",
  },
  anthropic: {
    apiKey: "https://console.anthropic.com/settings/keys",
    docs: "https://docs.anthropic.com/en/api/messages",
  },
} as const;

function appendConsole(text: string) {
  consoleOutput.textContent += text;
  consoleOutput.scrollTop = consoleOutput.scrollHeight;
}

function formatPosixOffset(offsetMinutes: number): string {
  if (offsetMinutes === 0) return "0";
  const sign = offsetMinutes < 0 ? "-" : "";
  const absMinutes = Math.abs(offsetMinutes);
  const hours = Math.floor(absMinutes / 60);
  const minutes = absMinutes % 60;
  if (minutes === 0) return `${sign}${hours}`;
  return `${sign}${hours}:${String(minutes).padStart(2, "0")}`;
}

function getShortTimeZoneName(date: Date): string | null {
  try {
    const formatter = new Intl.DateTimeFormat(undefined, { timeZoneName: "short" });
    const name = formatter.formatToParts(date).find((part) => part.type === "timeZoneName")?.value?.trim();
    return name || null;
  } catch {
    return null;
  }
}

function sanitizeTzAbbreviation(name: string | null, fallback: string): string {
  if (!name) return fallback;
  if (/^[A-Za-z]{3,}$/.test(name)) return name;
  return fallback;
}

function getTransitionRule(date: Date): string {
  const month = date.getMonth() + 1;
  const weekday = date.getDay();
  const dayOfMonth = date.getDate();
  const lastDay = new Date(date.getFullYear(), month, 0).getDate();
  const week = dayOfMonth + 7 > lastDay ? 5 : Math.floor((dayOfMonth - 1) / 7) + 1;
  const hours = date.getHours();
  const minutes = date.getMinutes();
  const seconds = date.getSeconds();
  let timePart = "";
  if (hours !== 2 || minutes !== 0 || seconds !== 0) {
    timePart = `/${hours}`;
    if (minutes !== 0 || seconds !== 0) {
      timePart += `:${String(minutes).padStart(2, "0")}`;
    }
    if (seconds !== 0) {
      timePart += `:${String(seconds).padStart(2, "0")}`;
    }
  }
  return `M${month}.${week}.${weekday}${timePart}`;
}

function findTransitionInstant(year: number, startMonth: number, endMonth: number): Date | null {
  let previousOffset = new Date(year, startMonth, 1, 12).getTimezoneOffset();
  for (let month = startMonth; month <= endMonth; month++) {
    const daysInMonth = new Date(year, month + 1, 0).getDate();
    for (let day = month === startMonth ? 2 : 1; day <= daysInMonth; day++) {
      const probe = new Date(year, month, day, 12);
      const offset = probe.getTimezoneOffset();
      if (offset === previousOffset) {
        continue;
      }

      for (let hour = 0; hour < 24; hour++) {
        const hourProbe = new Date(year, month, day, hour);
        if (hourProbe.getTimezoneOffset() !== previousOffset) {
          for (let minute = 0; minute < 60; minute++) {
            const minuteProbe = new Date(year, month, day, hour, minute);
            if (minuteProbe.getTimezoneOffset() !== previousOffset) {
              return minuteProbe;
            }
          }
          return hourProbe;
        }
      }
      return probe;
    }
    previousOffset = new Date(year, month, new Date(year, month + 1, 0).getDate(), 12).getTimezoneOffset();
  }
  return null;
}

function guessLocalPosixTimezone(): string {
  const now = new Date();
  const year = now.getFullYear();
  const monthlyOffsets = Array.from({ length: 12 }, (_, month) => new Date(year, month, 1, 12).getTimezoneOffset());
  const standardOffset = Math.max(...monthlyOffsets);
  const daylightOffset = Math.min(...monthlyOffsets);

  const january = new Date(year, 0, 1, 12);
  const july = new Date(year, 6, 1, 12);
  const standardSample = january.getTimezoneOffset() === standardOffset ? january : july;
  const daylightSample = january.getTimezoneOffset() === daylightOffset ? january : july;

  const standardName = sanitizeTzAbbreviation(getShortTimeZoneName(standardSample), "UTC");
  if (standardOffset === daylightOffset) {
    return `${standardName}${formatPosixOffset(standardOffset)}`;
  }

  const daylightName = sanitizeTzAbbreviation(getShortTimeZoneName(daylightSample), "DST");
  const firstHalfTransition = findTransitionInstant(year, 0, 5);
  const secondHalfTransition = findTransitionInstant(year, 6, 11);
  if (!firstHalfTransition || !secondHalfTransition) {
    return `${standardName}${formatPosixOffset(standardOffset)}${daylightName}`;
  }

  const startTransition =
    firstHalfTransition.getTimezoneOffset() === daylightOffset ? firstHalfTransition : secondHalfTransition;
  const endTransition = startTransition === firstHalfTransition ? secondHalfTransition : firstHalfTransition;

  return `${standardName}${formatPosixOffset(standardOffset)}${daylightName},${getTransitionRule(startTransition)},${getTransitionRule(endTransition)}`;
}

function syncDerivedDefaults() {
  if (timezoneInput && !timezoneInput.value.trim()) {
    timezoneInput.value = guessLocalPosixTimezone();
  }
}

function getValidationFailureReason(): string | null {
  const requiredText = ["wifi_ssid", "time_timezone", "llm_model", "llm_api_key"];
  for (const id of requiredText) {
    const el = document.getElementById(id) as HTMLInputElement | null;
    if (!el) return `missing element: ${id}`;
    if (!el.value.trim()) return `required field empty: ${id}`;
  }

  const wifiPassword = wifiPasswordInput?.value?.trim() ?? "";
  if (wifiPassword && wifiPassword.length < 8) {
    return "wifi password too short";
  }

  const mode = (document.getElementById("llm_mode") as HTMLSelectElement | null)?.value;
  if (mode === "openai_compat" || mode === "anthropic_compat") {
    const url = (document.getElementById("llm_base_url") as HTMLInputElement | null)?.value?.trim();
    if (!url) return "required field empty: llm_base_url";
  }

  // Validate required fields for each added IM channel
  for (const channelId of activeChannels) {
    if (channelId === "wechat" && !wechatLoginState.token.trim()) {
      return "required field empty: wechat_token";
    }
    const def = IM_CHANNEL_DEFS[channelId];
    if (!def) continue;
    for (const field of def.fields) {
      const val = (document.getElementById(field.id) as HTMLInputElement | null)?.value?.trim();
      if (!val) return `required field empty: ${field.id}`;
    }
  }

  return null;
}

function setFlashTabUi(active: "flash" | "console") {
  const flash = active === "flash";
  tabFlashBtn.classList.toggle("active", flash);
  tabConsoleBtn.classList.toggle("active", !flash);
  tabFlash.classList.toggle("active", flash);
  tabConsole.classList.toggle("active", !flash);
}

function updateConsoleTabEnabled() {
  const canUseConsole = !!transport && !isFlashing;
  tabConsoleBtn.disabled = !canUseConsole;
  tabConsoleBtn.title = isFlashing && transport ? s.tabLockedWhileFlashing : "";
}

function setConsoleInputsEnabled(on: boolean) {
  consoleInput.disabled = !on;
  consoleSendBtn.disabled = !on;
}

/** Plain UART write (Transport.write SLIP-wraps data for the flasher protocol). */
async function writeRawSerial(port: SerialPort, data: Uint8Array) {
  if (!port.writable) return;
  const writer = port.writable.getWriter();
  try {
    await writer.write(data);
  } finally {
    writer.releaseLock();
  }
}

async function stopConsoleReadLoop() {
  const hadConsoleLoop = consoleActive || !!consoleReader || !!consoleReadPromise;
  consoleActive = false;
  try {
    await consoleReader?.cancel();
  } catch {}
  if (consoleReadPromise) {
    try {
      await consoleReadPromise;
    } catch {}
    consoleReadPromise = null;
  }
  consoleReader = null;
  if (transport && hadConsoleLoop) {
    await transport.waitForUnlock(80);
  }
}

function startConsoleReadLoop() {
  if (!transport || consoleReadPromise) return;
  const port = transport.device;
  if (!port.readable) {
    appendConsole("\n[serial] Port not readable — USB may have re-enumerated; try disconnect and connect again.\n");
    return;
  }

  consoleActive = true;
  consoleReadPromise = (async () => {
    try {
      consoleReader = port.readable!.getReader();
      while (consoleActive) {
        const { value, done } = await consoleReader.read();
        if (done) break;
        if (value?.length) appendConsole(new TextDecoder().decode(value));
      }
    } catch (err) {
      console.error("[flash-tool] Failed to read serial console", err);
    } finally {
      try {
        consoleReader?.releaseLock();
      } catch (err) {
        console.error("[flash-tool] Failed to release console read lock");
      }
      consoleReader = null;
      consoleActive = false;
      consoleReadPromise = null;
    }
  })();
}

async function switchToFlashTab() {
  if (tabFlash.classList.contains("active") && (!transport || esptoolReadLoopRunning) && !consoleReadPromise) {
    return;
  }
  await stopConsoleReadLoop();
  setFlashTabUi("flash");
  if (transport && !esptoolReadLoopRunning) {
    void transport.readLoop();
    esptoolReadLoopRunning = true;
  }
}

const CONSOLE_BAUD_RATE = 115200;

async function reopenPortForConsole() {
  if (!transport) return;
  try {
    await transport.disconnect();
  } catch (err) {
    console.warn("[flash-tool] disconnect before console reopen failed", err);
  }
  await transport.connect(CONSOLE_BAUD_RATE);
}

async function switchToConsoleTab() {
  if (!transport || isFlashing) return;
  await stopEsptoolReadLoop(transport);
  esptoolReadLoopRunning = false;
  await transport.waitForUnlock(100);
  if (transport.baudrate !== CONSOLE_BAUD_RATE) {
    await reopenPortForConsole();
  }
  setFlashTabUi("console");
  startConsoleReadLoop();
}

if (!("serial" in navigator)) {
  unsupportedBanner.classList.add("visible");
  connectBtn.disabled = true;
}

tabFlashBtn.addEventListener("click", () => {
  void switchToFlashTab();
});

tabConsoleBtn.addEventListener("click", () => {
  if (!transport || isFlashing) return;
  void switchToConsoleTab();
});

connectBtn.addEventListener("click", async () => {
  connectBtn.disabled = true;
  connectBtn.textContent = s.connectingMsg;
  flashResult.classList.remove("visible");

  try {
    const port = await navigator.serial.requestPort();
    transport = new Transport(port, false);

    const terminal = {
      clean() {
        appendConsole("\x1bc");
      },
      writeLine(data: string) {
        appendConsole(data + "\n");
      },
      write(data: string) {
        appendConsole(data);
      },
    };

    loader = new ESPLoader({
      transport,
      baudrate: 921600,
      terminal,
      debugLogging: false,
    });
    const rawChipName = await loader.main("default_reset");
    esptoolReadLoopRunning = true;

    let flashSizeMB = 0;
    try {
      const fstr = await loader.detectFlashSize();
      const m = fstr.match(/^(\d+)/);
      flashSizeMB = m ? parseInt(m[1]!, 10) : 0;
    } catch {}

    chipInfo = { chipName: rawChipName, flashSizeMB };

    statusBadge.className = "status-badge connected";
    statusText.textContent = s.connectedTo + " · " + rawChipName;
    infoChip.textContent = rawChipName;
    infoFlash.textContent = flashSizeMB + " MB";

    connectBarIdle.style.display = "none";
    connectBarActive.style.display = "flex";
    connectBar.classList.add("is-connected");

    mainContent.style.display = "block";
    mainContent.classList.remove("revealed");
    void mainContent.offsetWidth;
    mainContent.classList.add("revealed");

    setConsoleInputsEnabled(true);
    updateConsoleTabEnabled();

    populateFirmware(rawChipName, flashSizeMB);
    updateFlashBtn();
  } catch (err) {
    connectBtn.disabled = false;
    connectBtn.innerHTML = connectBtnOriginalHTML;
    connectBarIdle.style.display = "flex";
    const message = err instanceof Error ? err.message : String(err);
    if (message !== "No port selected") {
      appendConsole("[Error] " + message + "\n");
      console.error("[flash-tool] Failed to connect to device", err);
    }
    esptoolReadLoopRunning = false;
    transport = null;
    loader = null;
  }
});

disconnectBtn.addEventListener("click", async () => {
  resetWechatLoginState();
  await stopConsoleReadLoop();
  if (transport) {
    try {
      await transport.disconnect();
    } catch {
      /* ignore */
    }
  }
  transport = null;
  loader = null;
  chipInfo = null;
  selectedFirmware = null;
  isFlashing = false;
  esptoolReadLoopRunning = false;

  connectBarActive.style.display = "none";
  connectBarIdle.style.display = "flex";
  connectBar.classList.remove("is-connected");
  connectBtn.disabled = false;
  connectBtn.innerHTML = connectBtnOriginalHTML;

  mainContent.style.display = "none";
  mainContent.classList.remove("revealed");

  setFlashTabUi("flash");
  setConsoleInputsEnabled(false);
  updateConsoleTabEnabled();

  firmwareGrid.innerHTML = "";
  configForm.style.display = "none";
  configForm.classList.remove("config-revealed");
  firmwareDesc.classList.remove("visible");
  updateFlashBtn();
});

function populateFirmware(chipName: string, flashSizeMB: number) {
  const list = getFirmwareList(firmwareDb, chipName, flashSizeMB);
  firmwareGrid.innerHTML = "";
  firmwareDesc.classList.remove("visible");
  noFirmwareMsg.style.display = "none";

  if (!list.length) {
    noFirmwareMsg.style.display = "block";
    return;
  }

  list.forEach((fw, idx) => {
    const card = document.createElement("div");
    card.className = "firmware-card";
    card.dataset.idx = String(idx);

    const tags = fw.features.map((f) => `<span class="fw-tag">${f}</span>`).join("");
    card.innerHTML = `
      <div class="fw-board">${fw.board}</div>
      <div class="fw-meta">Flash ≥ ${fw.min_flash_size} MB</div>
      ${tags ? `<div class="fw-features">${tags}</div>` : ""}
    `;
    card.addEventListener("click", () => selectFirmware(fw, card));
    firmwareGrid.appendChild(card);
  });
}

function selectFirmware(fw: FirmwareEntry, card: HTMLDivElement) {
  selectedFirmware = fw;
  document.querySelectorAll(".firmware-card").forEach((c) => c.classList.remove("selected"));
  card.classList.add("selected");

  const descText = fw.description.replace(/^#+\s*/gm, "");
  firmwareDesc.textContent = descText;
  firmwareDesc.classList.add("visible");

  configForm.style.display = "block";
  configForm.classList.remove("config-revealed");
  void configForm.offsetWidth;
  configForm.classList.add("config-revealed");
  syncLlmPanels();
  renderImChannels();
  updateFlashBtn();
}

function updateFlashBtn() {
  if (!chipInfo) {
    flashBtn.disabled = true;
    flashHint.textContent = s.flashBtnDisabledNoDevice;
    return;
  }
  if (!selectedFirmware) {
    flashBtn.disabled = true;
    flashHint.textContent = s.flashBtnDisabledNoFirmware;
    return;
  }
  const validationFailure = getValidationFailureReason();
  if (validationFailure) {
    flashBtn.disabled = true;
    flashHint.textContent = s.flashBtnDisabledNoConfig;
    return;
  }
  flashBtn.disabled = false;
  flashHint.textContent = "";
}

function syncLlmPanels(): void {
  const mode = llmModeSelect?.value;
  const baseWrap = document.getElementById("llm_base_url_wrap");
  if (baseWrap) baseWrap.classList.toggle("open", mode === "openai_compat" || mode === "anthropic_compat");
  const baseUrlLabelText = document.getElementById("llm_base_url_label_text");
  if (baseUrlLabelText) {
    baseUrlLabelText.textContent =
      mode === "anthropic_compat" ? s.llmBaseUrlLabelAnthropicCompat : s.llmBaseUrlLabel;
  }

  const vendorLinks = mode === "qwen" || mode === "openai" || mode === "anthropic" ? LLM_VENDOR_LINKS[mode] : null;
  if (llmVendorActions) {
    llmVendorActions.classList.toggle("hidden", !vendorLinks);
  }
  if (vendorLinks) {
    if (llmApiKeyLink) llmApiKeyLink.href = vendorLinks.apiKey;
    if (llmDocsLink) llmDocsLink.href = vendorLinks.docs;
  }

  if (llmModelInput && !llmModelUserEdited && mode) {
    const defaultModel = LLM_DEFAULT_MODELS[mode];
    if (defaultModel) {
      llmModelInput.value = defaultModel;
    } else {
      llmModelInput.value = "";
    }
  }
}

if (configForm) {
  configForm.addEventListener("input", () => {
    syncLlmPanels();
    updateFlashBtn();
  });
  configForm.addEventListener("change", () => {
    syncLlmPanels();
    updateFlashBtn();
  });
}
llmModeSelect?.addEventListener("change", () => {
  if (llmModelInput) {
    const current = llmModelInput.value.trim();
    const allDefaults = Object.values(LLM_DEFAULT_MODELS);
    if (current === "" || allDefaults.includes(current)) {
      llmModelUserEdited = false;
    }
  }
  syncLlmPanels();
});
syncLlmPanels();
renderImChannels();
syncDerivedDefaults();

function validateConfig(): boolean {
  return getValidationFailureReason() === null;
}

function getConfig(): NvsConfig {
  const get = (id: string) =>
    (document.getElementById(id) as HTMLInputElement | HTMLSelectElement | null)?.value?.trim() ||
    undefined;
  const mode = (document.getElementById("llm_mode") as HTMLSelectElement | null)?.value ?? "qwen";
  let llm_profile: string;
  let llm_backend_type: string;
  let llm_base_url: string | undefined;
  if (mode === "openai") {
    llm_profile = "openai";
    llm_backend_type = "openai_compatible";
  } else if (mode === "openai_compat") {
    llm_profile = "custom_openai_compatible";
    llm_backend_type = "openai_compatible";
    llm_base_url = get("llm_base_url");
  } else if (mode === "anthropic") {
    llm_profile = "anthropic";
    llm_backend_type = "anthropic";
  } else if (mode === "anthropic_compat") {
    llm_profile = "anthropic";
    llm_backend_type = "anthropic";
    llm_base_url = get("llm_base_url");
  } else {
    llm_profile = "qwen_compatible";
    llm_backend_type = "openai_compatible";
  }

  const qqOn    = activeChannels.includes("qq");
  const feishuOn = activeChannels.includes("feishu");
  const tgOn    = activeChannels.includes("tg");
  const wechatOn = activeChannels.includes("wechat");

  return {
    wifi_ssid: get("wifi_ssid") || "",
    wifi_password: get("wifi_password") || "",
    time_timezone: get("time_timezone") || "",
    llm_backend_type,
    llm_profile,
    llm_model: get("llm_model") || "",
    llm_api_key: get("llm_api_key") || "",
    ...(llm_base_url ? { llm_base_url } : {}),
    ...(qqOn
      ? { qq_app_id: get("qq_app_id"), qq_app_secret: get("qq_app_secret") }
      : {}),
    ...(feishuOn
      ? { feishu_app_id: get("feishu_app_id"), feishu_app_secret: get("feishu_app_secret") }
      : {}),
    ...(tgOn ? { tg_bot_token: get("tg_bot_token") } : {}),
    ...(wechatOn
      ? {
          wechat_token: wechatLoginState.token.trim(),
          wechat_base_url:
            wechatLoginState.baseUrl.trim() ||
            wechatLoginState.currentApiBaseUrl.trim() ||
            WECHAT_ILINK_BASE_URL,
        }
      : {}),
    search_brave_key: get("search_brave_key"),
    search_tavily_key: get("search_tavily_key"),
  };
}

function showFlashActionBlocked(message: string) {
  flashResult.textContent = "✗ " + message;
  flashResult.className = "flash-result error visible";
}

flashBtn.addEventListener("click", async () => {
  if (!loader || !chipInfo) {
    showFlashActionBlocked(s.flashBtnDisabledNoDevice);
    updateFlashBtn();
    return;
  }
  if (!selectedFirmware) {
    showFlashActionBlocked(s.flashBtnDisabledNoFirmware);
    updateFlashBtn();
    return;
  }
  if (!validateConfig()) {
    showFlashActionBlocked(s.flashBtnDisabledNoConfig);
    updateFlashBtn();
    return;
  }

  try {
    await switchToFlashTab();

    isFlashing = true;
    updateConsoleTabEnabled();

    flashBtn.disabled = true;
    flashResult.className = "flash-result";
    progressWrap.classList.add("visible");
    progressLog.textContent = "";
    progressBar.style.width = "0%";

    function onProgress(p: { percent: number; message: string }) {
      progressStage.textContent = p.message;
      progressPct.textContent = p.percent + "%";
      progressBar.style.width = p.percent + "%";
      progressLog.textContent += p.message + "\n";
      progressLog.scrollTop = progressLog.scrollHeight;
    }

    onProgress({ percent: 0, message: s.downloadingFirmware });
    const resp = await fetch(selectedFirmware.merged_binary);
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    const buf = await resp.arrayBuffer();
    const fwBytes = new Uint8Array(buf);
    onProgress({ percent: 30, message: "Firmware downloaded." });

    onProgress({ percent: 35, message: s.generatingNvs });
    const nvsStart = parseAddr(selectedFirmware.nvs_info.start_addr);
    const nvsSize = parseAddr(selectedFirmware.nvs_info.size);
    const nvsData = generateNvsPartition(getConfig(), nvsSize);

    const merged = new Uint8Array(fwBytes);
    if (nvsStart + nvsSize <= merged.length) {
      merged.set(nvsData, nvsStart);
    }
    onProgress({ percent: 40, message: "NVS partition generated." });

    const fileArray: { data: Uint8Array; address: number }[] = [{ data: merged, address: 0x0 }];
    if (nvsStart + nvsSize > fwBytes.length) {
      fileArray.push({ data: nvsData, address: nvsStart });
    }

    onProgress({ percent: 45, message: s.writingFlash });
    await loader.writeFlash({
      fileArray,
      flashSize: "keep",
      flashMode: "keep",
      flashFreq: "keep",
      eraseAll: false,
      compress: true,
      reportProgress(fileIndex, written, total) {
        const base = 45 + (fileIndex / fileArray.length) * 50;
        const pct = Math.round(base + (written / total) * (50 / fileArray.length));
        onProgress({ percent: pct, message: `${s.writingFlash} ${pct}%` });
      },
    });

    await loader.after("hard_reset");
    onProgress({ percent: 100, message: s.flashSuccess });

    flashResult.textContent = "✓ " + s.flashSuccess;
    flashResult.className = "flash-result success visible";

    isFlashing = false;
    updateConsoleTabEnabled();
    flashBtn.disabled = false;

    await sleep(400);
    await switchToConsoleTab();
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    console.error("[flash-tool] Flash flow failed", err);
    progressStage.textContent = s.flashError + message;
    progressPct.textContent = "0%";
    progressBar.style.width = "0%";
    progressLog.textContent += s.flashError + message + "\n";
    flashResult.textContent = "✗ " + s.flashError + message;
    flashResult.className = "flash-result error visible";

    isFlashing = false;
    updateConsoleTabEnabled();
    flashBtn.disabled = false;
  }
});

consoleSendBtn.addEventListener("click", sendInput);
consoleInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") sendInput();
});
async function sendInput() {
  if (!transport || !consoleInput.value || consoleInput.disabled) return;
  await writeRawSerial(transport.device, new TextEncoder().encode(consoleInput.value + "\r\n"));
  consoleInput.value = "";
}

consoleClearBtn.addEventListener("click", () => {
  consoleOutput.textContent = "";
});

consoleResetBtn.addEventListener("click", async () => {
  if (!transport) return;
  try {
    await transport.setRTS(true);
    await sleep(100);
    await transport.setRTS(false);
  } catch {
    /* Port may reconnect after USB reset; ignore NetworkError */
  }
});

updateConsoleTabEnabled();
updateFlashBtn();
