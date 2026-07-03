export interface NvsInfo {
  start_addr: string;
  size: string;
}

export interface FirmwareEntry {
  board: string;
  features: string[];
  description: string;
  merged_binary: string;
  min_flash_size: number; // MB
  nvs_info: NvsInfo;
}

export type FirmwareDb = Record<string, FirmwareEntry[]>;

// Embedded firmware database (firmware.json content baked in at build time)
// The actual data is injected by the Astro page via define:vars.
declare global {
  interface Window {
    __FIRMWARE_DB__: FirmwareDb;
    __FLASH_LANG__: string;
  }
}

function normalizeChipTarget(chipTarget: string): string {
  const normalized = chipTarget.trim().toLowerCase();
  const esp32Match = normalized.match(/\besp32(?:-[a-z0-9]+)?\b/);
  if (esp32Match) {
    return esp32Match[0].replace(/-/g, "");
  }

  return normalized.replace(/[^a-z0-9]/g, "");
}

export function getFirmwareList(
  db: FirmwareDb,
  chipTarget: string,
  flashSizeMB: number
): FirmwareEntry[] {
  // Normalize chip target: e.g. "ESP32-S3 (QFN56) (revision v0.2)" => "esp32s3"
  const key = normalizeChipTarget(chipTarget);
  const list = db[key] ?? [];
  return list.filter((fw) => flashSizeMB >= fw.min_flash_size);
}

/** Parse a hex or decimal string like "0x9000" or "36864" into a number */
export function parseAddr(s: string): number {
  return s.startsWith("0x") || s.startsWith("0X")
    ? parseInt(s, 16)
    : parseInt(s, 10);
}
