Project-local emote assets override directory.

`build_speaker_assets_bin(... --external_path)` searches this directory first,
then falls back to the packaged `esp_emote_assets` component defaults.

If you want to override assets locally, mirror the structure expected by
`esp_emote_assets`, for example:

- `360_360/config.json`
- `360_360/layout.json`
- `360_360/emote.json`
- `emoji_large/...`
- `emoji_small/...`
- `font/...`
