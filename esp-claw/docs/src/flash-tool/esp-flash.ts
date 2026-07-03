/// <reference types="w3c-web-serial" />
/**
 * esp-flash.ts
 * Wrapper around esptool-js for connecting, reading chip info,
 * flashing firmware with NVS overlay, and running the serial console.
 */

import {
  ESPLoader,
  Transport,
  type IEspLoaderTerminal,
} from "esptool-js";
import { generateNvsPartition, type NvsConfig } from "./nvs-gen";
import type { FirmwareEntry } from "./firmware-db";
import { parseAddr } from "./firmware-db";

// ─── Types ────────────────────────────────────────────────────────────────────

export interface ChipInfo {
  chipName: string;
  flashSizeMB: number;
}

export interface FlashProgress {
  stage: "downloading" | "generating-nvs" | "writing" | "done" | "error";
  percent: number;
  message: string;
}

type ProgressCallback = (p: FlashProgress) => void;
type ConsoleDataCallback = (text: string) => void;

// ─── Module state ─────────────────────────────────────────────────────────────

let _transport: Transport | null = null;
let _loader: ESPLoader | null = null;
let _consoleActive = false;

const BAUD_RATE = 921600;

// ─── Terminal adapter ─────────────────────────────────────────────────────────

function makeTerminal(onWrite: ConsoleDataCallback): IEspLoaderTerminal {
  return {
    clean(): void {
      onWrite("\x1bc");
    },
    writeLine(data: string): void {
      onWrite(data + "\n");
    },
    write(data: string): void {
      onWrite(data);
    },
  };
}

// ─── Connect / disconnect ─────────────────────────────────────────────────────

/**
 * Request serial port access from the browser and connect to the ESP32.
 * Returns chip information read from the device.
 */
export async function connectDevice(
  onLog: ConsoleDataCallback
): Promise<ChipInfo> {
  if (!("serial" in navigator)) {
    throw new Error("WEB_SERIAL_UNSUPPORTED");
  }

  const port = await navigator.serial.requestPort();

  _transport = new Transport(port, false);

  const terminal = makeTerminal(onLog);

  _loader = new ESPLoader({
    transport: _transport,
    baudrate: BAUD_RATE,
    terminal,
    debugLogging: false,
  });

  const chipName = await _loader.main("default_reset");

  // Detect flash size
  let flashSizeMB = 0;
  try {
    const flashSizeStr = await _loader.detectFlashSize();
    const m = flashSizeStr.match(/^(\d+)/);
    flashSizeMB = m ? parseInt(m[1], 10) : 0;
  } catch {
    flashSizeMB = 0;
  }

  return { chipName, flashSizeMB };
}

/** Disconnect from the device */
export async function disconnectDevice(): Promise<void> {
  _consoleActive = false;
  if (_transport) {
    try {
      await _transport.disconnect();
    } catch {
      // ignore
    }
    _transport = null;
  }
  _loader = null;
}

export function isConnected(): boolean {
  return _transport !== null && _loader !== null;
}

// ─── Flash ────────────────────────────────────────────────────────────────────

/**
 * Download the firmware binary, overlay the NVS partition data, then flash.
 */
export async function flashFirmware(
  firmware: FirmwareEntry,
  config: NvsConfig,
  onProgress: ProgressCallback
): Promise<void> {
  if (!_loader || !_transport) {
    throw new Error("Device not connected.");
  }

  // ── Stage 1: Download firmware ──
  onProgress({ stage: "downloading", percent: 0, message: "Downloading firmware…" });

  const response = await fetch(firmware.merged_binary);
  if (!response.ok) {
    throw new Error(`Failed to download firmware: ${response.status} ${response.statusText}`);
  }
  const firmwareBuffer = await response.arrayBuffer();
  const firmwareBytes = new Uint8Array(firmwareBuffer);

  onProgress({ stage: "downloading", percent: 30, message: "Firmware downloaded." });

  // ── Stage 2: Generate NVS ──
  onProgress({ stage: "generating-nvs", percent: 35, message: "Generating NVS partition…" });

  const nvsStart = parseAddr(firmware.nvs_info.start_addr);
  const nvsSize = parseAddr(firmware.nvs_info.size);
  const nvsData = generateNvsPartition(config, nvsSize);

  // Overlay NVS data into the firmware buffer at the correct offset
  const mergedBytes = new Uint8Array(firmwareBytes);
  const nvsInImage = nvsStart + nvsSize <= mergedBytes.length;
  if (nvsInImage) {
    mergedBytes.set(nvsData, nvsStart);
  }

  onProgress({ stage: "generating-nvs", percent: 40, message: "NVS partition generated." });

  // ── Stage 3: Write flash ──
  onProgress({ stage: "writing", percent: 45, message: "Writing to flash…" });

  const fileArray: { data: Uint8Array; address: number }[] = [
    { data: mergedBytes, address: 0x0 },
  ];

  // If NVS is outside the merged image range, add it as a separate region
  if (!nvsInImage) {
    fileArray.push({ data: nvsData, address: nvsStart });
  }

  await _loader.writeFlash({
    fileArray,
    flashSize: "keep",
    flashMode: "keep",
    flashFreq: "keep",
    eraseAll: false,
    compress: true,
    reportProgress(fileIndex: number, written: number, total: number) {
      const base = 45 + (fileIndex / fileArray.length) * 50;
      const pct = Math.round(base + (written / total) * (50 / fileArray.length));
      onProgress({
        stage: "writing",
        percent: pct,
        message: `Writing flash: ${pct}% (file ${fileIndex + 1}/${fileArray.length})`,
      });
    },
    calculateMD5Hash: undefined,
  });

  await _loader.after("hard_reset");

  onProgress({ stage: "done", percent: 100, message: "Flash complete! Device is rebooting." });
}

// ─── Serial Console ───────────────────────────────────────────────────────────

/**
 * Start streaming raw serial output from the device.
 */
export async function startConsole(onData: ConsoleDataCallback): Promise<void> {
  if (!_transport) throw new Error("Device not connected.");

  _consoleActive = true;

  await _transport.rawRead(
    (chunk: Uint8Array) => {
      const text = new TextDecoder().decode(chunk);
      onData(text);
    },
    () => !_consoleActive
  );
}

export function stopConsole(): void {
  _consoleActive = false;
}

/** Send raw bytes to the serial port (interactive console). */
export async function sendConsoleInput(text: string): Promise<void> {
  if (!_transport) throw new Error("Device not connected.");
  const bytes = new TextEncoder().encode(text);
  await _transport.write(bytes);
}

/** DTR pulse to reset the device without disconnecting. */
export async function resetDevice(): Promise<void> {
  if (!_transport) return;
  await _transport.setRTS(true);
  await new Promise<void>((r) => setTimeout(r, 100));
  await _transport.setRTS(false);
}
