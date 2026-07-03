/**
 * nvs-gen.ts
 * Browser-side ESP-IDF NVS v2 binary partition generator.
 *
 * Format derived from ESPConnect's nvsParser.ts
 * (https://github.com/thelastoutpostworkshop/ESPConnect/blob/main/src/lib/nvs/nvsParser.ts)
 *
 * Key constants:
 *   PAGE_SIZE       = 0x1000 (4096 bytes)
 *   ENTRY_SIZE      = 32 bytes
 *   ENTRY_COUNT     = 126 entries per page
 *   HEADER_SIZE     = 32 bytes  (at page offset 0x00)
 *   ENTRY_TABLE_SIZE= 32 bytes  (at page offset 0x20, bitmap of 126×2 bits)
 *   ENTRY_DATA_OFF  = 0x40     (first entry starts here)
 *
 * Page header (32 bytes):
 *   [0x00] u32 state      — ACTIVE=0xFFFFFFFE, FULL=0xFFFFFFFC
 *   [0x04] u32 seq        — page sequence number (0-based)
 *   [0x08] u8  version    — 0xFE for NVS v2
 *   [0x09..0x1B] reserved 0xFF (19 bytes)
 *   [0x1C] u32 headerCrc  — nvsCrc32(page[4..28))
 *
 * Entry state bitmap:
 *   2 bits per entry, packed little-endian into 32 bytes (8 × u32).
 *   EMPTY=0b11 (0x3), WRITTEN=0b10 (0x2), ERASED=0b00 (0x0)
 *
 * Entry (32 bytes):
 *   [0]   u8  nsIndex     — 0 = namespace definition entry
 *   [1]   u8  typeCode    — SZ=0x21, U8=0x01
 *   [2]   u8  spanCount   — how many consecutive entries this item uses
 *   [3]   u8  chunkIndex  — 0xFF for non-blob (v2)
 *   [4]   u32 itemCrc     — nvsCrc32 of entry bytes [0..4) ++ [8..32)
 *   [8]   16B key         — null-terminated, rest padded with 0xFF
 *   [24]  u16 dataSize    — declared payload size (variable-length types)
 *   [26]  u16 reserved    — 0xFFFF
 *   [28]  u32 dataCrc (variable-length) OR inline data (fixed-width)
 *
 * CRC: ESP-IDF nvsCrc32 = CRC32/ISO-HDLC with init=0x00000000, poly LE,
 *      then XOR 0xFFFFFFFF (matches nvsParser.ts nvsCrc32 implementation).
 * Item CRC covers bytes [0..4) + [8..32) (skips the crc field itself).
 */

export interface NvsConfig {
  // Required - Basic
  wifi_ssid: string;
  wifi_password: string;
  time_timezone: string;
  // Required - LLM (matches basic_demo_settings.c)
  llm_backend_type: string;
  llm_profile: string;
  llm_model: string;
  llm_api_key: string;
  llm_base_url?: string;
  // Optional - IM (keys only when user enables channel in UI)
  qq_app_id?: string;
  qq_app_secret?: string;
  feishu_app_id?: string;
  feishu_app_secret?: string;
  tg_bot_token?: string;
  wechat_token?: string;
  wechat_base_url?: string;
  // Optional - Search
  search_brave_key?: string;
  search_tavily_key?: string;
}

// ─── CRC32 ────────────────────────────────────────────────────────────────────

const CRC32_TABLE: Uint32Array = (() => {
  const t = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
    }
    t[i] = c >>> 0;
  }
  return t;
})();

function crc32Update(crc: number, data: Uint8Array, start = 0, length = data.length - start): number {
  let c = crc >>> 0;
  const end = Math.min(data.length, start + length);
  for (let i = start; i < end; i++) {
    c = CRC32_TABLE[(c ^ data[i]) & 0xff] ^ (c >>> 8);
  }
  return c >>> 0;
}

/** nvsCrc32: init=0x00000000, poly CRC32 LE, final XOR 0xFFFFFFFF */
function nvsCrc32(data: Uint8Array, start = 0, length = data.length - start): number {
  return (crc32Update(0x00000000, data, start, length) ^ 0xffffffff) >>> 0;
}

/** Item CRC: covers entry bytes [0..4) + [8..32) */
function computeItemCrc32(entry: Uint8Array): number {
  let crc = 0x00000000;
  crc = crc32Update(crc, entry, 0, 4);
  crc = crc32Update(crc, entry, 8, 24);
  return (crc ^ 0xffffffff) >>> 0;
}

// ─── Constants ────────────────────────────────────────────────────────────────

const PAGE_SIZE        = 0x1000;
const ENTRY_SIZE       = 32;
const ENTRY_COUNT      = 126;
const HEADER_SIZE      = 32;
const ENTRY_TABLE_SIZE = 32;
const ENTRY_DATA_OFF   = HEADER_SIZE + ENTRY_TABLE_SIZE; // 0x40

const PAGE_STATE_ACTIVE = 0xfffffffe >>> 0;
const PAGE_STATE_FULL   = 0xfffffffc >>> 0;
const NVS_VERSION       = 0xfe; // v2

const TYPE_U8  = 0x01;
const TYPE_SZ  = 0x21; // null-terminated string

const ENTRY_STATE_WRITTEN = 0x2; // bits: 10
// const ENTRY_STATE_EMPTY   = 0x3; // bits: 11 // unused

// ─── Page builder ─────────────────────────────────────────────────────────────

interface PageCtx {
  buf: Uint8Array;
  view: DataView;
  seq: number;
  entryIndex: number; // next free entry slot
}

function newPage(seq: number): PageCtx {
  const buf = new Uint8Array(PAGE_SIZE).fill(0xff);
  const view = new DataView(buf.buffer);

  // state = ACTIVE
  view.setUint32(0, PAGE_STATE_ACTIVE, true);
  // seq
  view.setUint32(4, seq, true);
  // version = v2
  buf[8] = NVS_VERSION;
  // bytes [9..27] already 0xFF
  // header CRC written later

  return { buf, view, seq, entryIndex: 0 };
}

function writeHeaderCrc(page: PageCtx): void {
  const crc = nvsCrc32(page.buf, 4, 24);
  page.view.setUint32(0x1c, crc, true);
}

function markPageFull(page: PageCtx): void {
  page.view.setUint32(0, PAGE_STATE_FULL, true);
  writeHeaderCrc(page);
}

function setEntryState(page: PageCtx, idx: number, state: number): void {
  // Entry bitmap: 2 bits per entry, 16 entries per u32 word
  const wordIdx   = Math.floor(idx / 16);
  const bitOffset = (idx % 16) * 2;
  const byteOff   = HEADER_SIZE + wordIdx * 4; // offset into page.buf

  let word = page.view.getUint32(byteOff, true);
  // clear the 2 bits then set them
  word &= ~(0x3 << bitOffset);
  word |= (state & 0x3) << bitOffset;
  page.view.setUint32(byteOff, word >>> 0, true);
}

/**
 * Write a 32-byte entry into the current page.
 * Returns false if page is full (caller must start a new page).
 */
function writeEntry(page: PageCtx, entry: Uint8Array): boolean {
  if (page.entryIndex >= ENTRY_COUNT) return false;
  const off = ENTRY_DATA_OFF + page.entryIndex * ENTRY_SIZE;
  page.buf.set(entry, off);
  setEntryState(page, page.entryIndex, ENTRY_STATE_WRITTEN);
  page.entryIndex++;
  return true;
}

// ─── Entry builders ───────────────────────────────────────────────────────────

function encodeKey(key: string): Uint8Array {
  // Must match nvs_partition_gen.py exactly:
  //   key_array = b'\x00' * 16
  //   entry_struct[8:24] = key_array
  //   entry_struct[8:8 + len(key)] = key.encode()
  // i.e. a 16-byte buffer of 0x00 with the UTF-8 key copied over the prefix.
  // The item CRC is computed across the key bytes, so any mismatch with the
  // reference implementation makes the firmware reject the entry as corrupt.
  const encoded = new TextEncoder().encode(key);
  if (encoded.length > 15) {
    throw new Error(
      `NVS key "${key}" is ${encoded.length} bytes; max is 15 (ESP-IDF NVS_KEY_NAME_MAX_SIZE - 1).`
    );
  }
  const out = new Uint8Array(16); // zero-filled by default
  out.set(encoded);
  return out;
}

/**
 * Build the namespace definition entry.
 * nsIndex=0 (sys), typeCode=U8, key=namespace_name, data=namespace_id
 */
function buildNamespaceEntry(nsName: string, nsId: number): Uint8Array {
  const entry = new Uint8Array(ENTRY_SIZE).fill(0xff);
  const view = new DataView(entry.buffer);

  entry[0] = 0x00;    // nsIndex = 0 (system)
  entry[1] = TYPE_U8;
  entry[2] = 0x01;    // span = 1
  entry[3] = 0xff;    // chunkIndex unused (non-blob)
  // key
  entry.set(encodeKey(nsName), 8);
  // inline data at [24]: namespace id
  entry[24] = nsId;
  entry[25] = 0xff;
  entry[26] = 0xff;
  entry[27] = 0xff;
  // itemCrc
  const crc = computeItemCrc32(entry);
  view.setUint32(4, crc, true);
  return entry;
}

/**
 * Build one or more NVS entries for a string value.
 * Returns an array of 32-byte Uint8Arrays (header entry + data entries).
 */
function buildStringEntries(nsIndex: number, key: string, value: string): Uint8Array[] {
  const enc = new TextEncoder();
  const valueBytes = enc.encode(value); // no null terminator in payload
  const dataLen = valueBytes.length + 1; // +1 for null terminator

  // How many extra data entries needed (each holds 32 bytes of payload)
  const payloadEntries = Math.ceil(dataLen / ENTRY_SIZE);
  const span = 1 + payloadEntries;

  // Header entry
  const header = new Uint8Array(ENTRY_SIZE).fill(0xff);
  const headerView = new DataView(header.buffer);
  header[0] = nsIndex;
  header[1] = TYPE_SZ;
  header[2] = span;
  header[3] = 0xff; // chunkIndex
  header.set(encodeKey(key), 8);
  headerView.setUint16(24, dataLen, true);
  // reserved [26..27] = 0xFFFF (already)

  // Assemble payload (null-terminated, padded with 0xFF)
  const payload = new Uint8Array(payloadEntries * ENTRY_SIZE).fill(0xff);
  payload.set(valueBytes, 0);
  payload[valueBytes.length] = 0x00; // null terminator

  // dataCrc over full payload
  const dataCrc = nvsCrc32(payload, 0, dataLen > payload.length ? payload.length : dataLen);
  headerView.setUint32(28, dataCrc, true);

  // itemCrc (header entry only, covers [0..4) + [8..32))
  const itemCrc = computeItemCrc32(header);
  headerView.setUint32(4, itemCrc, true);

  // Split payload into 32-byte chunks
  const entries: Uint8Array[] = [header];
  for (let i = 0; i < payloadEntries; i++) {
    entries.push(payload.slice(i * ENTRY_SIZE, (i + 1) * ENTRY_SIZE));
  }
  return entries;
}

// ─── Public API ───────────────────────────────────────────────────────────────

/**
 * Generate an ESP-IDF NVS v2 binary partition for the "basic_demo" namespace.
 *
 * @param config     - Key-value NVS configuration
 * @param partitionSize - Byte size of the NVS partition (from nvs_info.size)
 * @returns          - Uint8Array of exactly partitionSize bytes (0xFF padded)
 */
export function generateNvsPartition(
  config: NvsConfig,
  partitionSize: number
): Uint8Array {

  // Collect (key, value) pairs — skip empty/undefined values
  const pairs: [string, string][] = [];
  const add = (k: string, v: string | undefined): void => {
    const s = v?.trim();
    if (s) pairs.push([k, s]);
  };

  // Key names MUST match basic_demo_settings.c exactly. NVS keys are capped at
  // 15 chars, so the firmware uses shortened names (llm_backend, feishu_secret,
  // brave_key, tavily_key) that we have to mirror here.
  add("wifi_ssid",       config.wifi_ssid);
  add("wifi_password",   config.wifi_password);
  add("time_timezone",   config.time_timezone);
  add("llm_api_key",     config.llm_api_key);
  add("llm_backend",     config.llm_backend_type);
  add("llm_profile",     config.llm_profile);
  add("llm_model",       config.llm_model);
  add("llm_base_url",    config.llm_base_url);
  add("qq_app_id",       config.qq_app_id);
  add("qq_app_secret",   config.qq_app_secret);
  add("feishu_app_id",   config.feishu_app_id);
  add("feishu_secret",   config.feishu_app_secret);
  add("tg_bot_token",    config.tg_bot_token);
  add("wechat_token",    config.wechat_token);
  add("wechat_base_url", config.wechat_base_url);
  add("brave_key",       config.search_brave_key);
  add("tavily_key",      config.search_tavily_key);

  const NS_NAME = "basic_demo";
  const NS_ID   = 1;

  // Build all entries (namespace def + data entries)
  const allEntries: Uint8Array[] = [];
  allEntries.push(buildNamespaceEntry(NS_NAME, NS_ID));
  for (const [key, value] of pairs) {
    allEntries.push(...buildStringEntries(NS_ID, key, value));
  }

  // Fill pages
  const totalPages = Math.floor(partitionSize / PAGE_SIZE);
  if (totalPages < 1) {
    throw new Error(`NVS partition size ${partitionSize} is too small (need at least ${PAGE_SIZE} bytes).`);
  }

  const partitionBuf = new Uint8Array(partitionSize).fill(0xff);
  let seq = 0;
  let page = newPage(seq);

  for (const entry of allEntries) {
    if (writeEntry(page, entry)) continue;

    // Current page is full — mark FULL, flush, advance to a new ACTIVE page.
    if (seq >= totalPages - 1) {
      throw new Error("NVS partition is too small to hold all config entries.");
    }
    markPageFull(page);
    partitionBuf.set(page.buf, seq * PAGE_SIZE);
    seq++;
    page = newPage(seq);
    if (!writeEntry(page, entry)) {
      // A single logical item spans more than one page worth of entries; the
      // firmware side doesn't support that for strings we generate here.
      throw new Error("NVS entry too large to fit in a single page.");
    }
  }

  // Flush the last (still ACTIVE) page.
  writeHeaderCrc(page);
  partitionBuf.set(page.buf, seq * PAGE_SIZE);

  return partitionBuf;
}
