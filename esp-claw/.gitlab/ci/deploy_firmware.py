#!/usr/bin/env python3
"""
Deploy firmware artifacts to the download server.

Behavior is controlled by the DEPLOY_FIRMWARE environment variable:
  "1" - package firmware.json + merged_binary/ into esp-claw-firmware.zip and upload
  "0" - print a skip message and exit
"""

import os
import subprocess
import sys
import zipfile
from pathlib import Path

ARCHIVE_NAME = "esp-claw-firmware.zip"


def build_archive() -> Path:
    archive_path = Path(ARCHIVE_NAME)
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as zf:
        firmware_json = Path("firmware.json")
        if firmware_json.exists():
            zf.write(firmware_json, firmware_json.name)
            print(f"  added: {firmware_json}")
        else:
            print("WARNING: firmware.json not found, skipping.", file=sys.stderr)

        merged_binary = Path("merged_binary")
        if merged_binary.exists():
            for file in sorted(merged_binary.rglob("*")):
                if file.is_file():
                    zf.write(file, file.as_posix())
                    print(f"  added: {file}")
        else:
            print("WARNING: merged_binary/ directory not found, skipping.", file=sys.stderr)

    print(f"Archive created: {archive_path} ({archive_path.stat().st_size} bytes)")
    return archive_path


def upload_archive(archive_path: Path) -> None:
    upload_key = os.environ.get("DL_UPLOAD_KEY")
    upload_api_url = os.environ.get("DL_UPLOAD_API_URL")
    upload_path = os.environ.get("DL_UPLOAD_PATH")

    missing = [var for var, val in [
        ("DL_UPLOAD_KEY", upload_key),
        ("DL_UPLOAD_API_URL", upload_api_url),
        ("DL_UPLOAD_PATH", upload_path),
    ] if not val]

    if missing:
        print(f"ERROR: Missing required environment variables: {', '.join(missing)}", file=sys.stderr)
        sys.exit(1)

    cmd = [
        "curl",
        "--fail",
        "-u", upload_key,
        "-F", f"file=@{archive_path}",
        "-F", f"path={upload_path}",
        upload_api_url,
    ]

    print(f"Uploading {archive_path} to {upload_api_url} (path={upload_path}) ...")
    result = subprocess.run(cmd, check=False)
    if result.returncode != 0:
        print(f"ERROR: Upload failed with exit code {result.returncode}", file=sys.stderr)
        sys.exit(result.returncode)

    print("Upload succeeded.")


def main() -> None:
    deploy_firmware = os.environ.get("DEPLOY_FIRMWARE", "0")

    if deploy_firmware != "1":
        print(f"DEPLOY_FIRMWARE={deploy_firmware!r}: skipping firmware deployment.")
        sys.exit(0)

    print("DEPLOY_FIRMWARE=1: packaging and uploading firmware artifacts ...")
    archive_path = build_archive()
    upload_archive(archive_path)


if __name__ == "__main__":
    main()
