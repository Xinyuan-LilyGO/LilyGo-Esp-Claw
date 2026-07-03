#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

import json
import os
import sys
from pathlib import Path
from typing import Any, Dict, List


def _log(msg: str) -> None:
    print(msg, file=sys.stderr)


def _parse_flash_mb(value: Any) -> int:
    if isinstance(value, int):
        return value

    if not isinstance(value, str):
        raise ValueError(f'unsupported min_flash_size type: {type(value)}')

    text = value.strip()
    if not text:
        raise ValueError('empty min_flash_size')

    upper = text.upper()
    if upper.endswith('MB'):
        return int(upper[:-2].strip(), 0)
    if upper.endswith('M'):
        return int(upper[:-1].strip(), 0)

    num = int(text, 0)
    if num % (1024 * 1024) == 0:
        return num // (1024 * 1024)

    return num


def _join_base_url(base_url: str, filename: str) -> str:
    if not base_url:
        base_url = '/'

    if base_url.endswith('/'):
        return f'{base_url}{filename}'

    return f'{base_url}/{filename}'


def _load_metadata_files(merged_dir: Path) -> List[Dict[str, Any]]:
    metadata_files = sorted(merged_dir.glob('*.json'))
    if not metadata_files:
        raise RuntimeError(f'No metadata json found in {merged_dir}')

    records: List[Dict[str, Any]] = []
    for path in metadata_files:
        with path.open('r', encoding='utf-8') as fr:
            data = json.load(fr)
        if not isinstance(data, dict):
            raise RuntimeError(f'Invalid metadata format: {path}')
        records.append(data)

    return records


def main() -> int:
    cwd = Path.cwd()
    merged_dir = cwd / 'merged_binary'

    if not merged_dir.is_dir():
        _log(f'merged_binary directory not found: {merged_dir}')
        return 1

    base_url = os.getenv('MERGED_BINARY_BASE_URL', '/').strip() or '/'

    try:
        records = _load_metadata_files(merged_dir)
    except Exception as e:
        _log(str(e))
        return 1

    firmware: Dict[str, List[Dict[str, Any]]] = {}

    for record in records:
        chip = record.get('chip')
        board = record.get('board')
        merged_binary = record.get('merged_binary')
        min_flash_size = record.get('min_flash_size')
        nvs_info = record.get('nvs_info')

        if not isinstance(chip, str) or not chip.strip():
            _log(f'skip one metadata: invalid chip ({record})')
            continue
        if not isinstance(board, str) or not board.strip():
            _log(f'skip one metadata: invalid board ({record})')
            continue
        if not isinstance(merged_binary, str) or not merged_binary.strip():
            _log(f'skip one metadata: invalid merged_binary ({record})')
            continue
        if not isinstance(nvs_info, dict):
            _log(f'skip one metadata: invalid nvs_info ({record})')
            continue

        try:
            min_flash_mb = _parse_flash_mb(min_flash_size)
        except Exception as e:
            _log(f'skip one metadata: invalid min_flash_size ({record}) ({e})')
            continue

        item = {
            'board': board,
            'features': [],
            'description': '',
            'merged_binary': _join_base_url(base_url, merged_binary),
            'min_flash_size': min_flash_mb,
            'nvs_info': {
                'start_addr': str(nvs_info.get('start_addr', '')),
                'size': str(nvs_info.get('size', '')),
            },
        }

        firmware.setdefault(chip, []).append(item)

    if not firmware:
        _log('No valid metadata collected from merged_binary/*.json')
        return 1

    for chip in firmware:
        firmware[chip].sort(key=lambda x: x['board'])

    out_file = cwd / 'firmware.json'
    out_file.write_text(json.dumps(firmware, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Generated: {out_file}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
