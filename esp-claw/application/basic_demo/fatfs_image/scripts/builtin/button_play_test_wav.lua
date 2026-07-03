-- Button single_click plays test.wav. Requires board_manager for audio_dac codec params only.
-- Optional args: pin, duration_ms, poll_ms, test_wav (filename under FATFS root, default test.wav).
-- Pattern: xpcall(run) + cleanup() closes button + audio; integer args via args table.
local audio   = require("audio")
local bm      = require("board_manager")
local btn     = require("button")
local delay   = require("delay")
local storage = require("storage")

local a = type(args) == "table" and args or {}
local function int_arg(k, default)
    local v = a[k]
    if type(v) == "number" then
        return math.floor(v)
    end
    return default
end

local BUTTON_GPIO_NUM = int_arg("pin", 28)
local RUN_TIME_MS     = int_arg("duration_ms", 60000)
local POLL_INTERVAL_MS = int_arg("poll_ms", 10)
local wav_name = (type(a.test_wav) == "string" and a.test_wav ~= "") and a.test_wav or "test.wav"
local TEST_WAV_PATH = storage.join_path(storage.get_root_dir(), wav_name)

local output
local handle
local is_playing = false

local function cleanup()
    if handle then
        pcall(btn.off, handle)
        pcall(btn.close, handle)
        handle = nil
    end
    if output then
        pcall(audio.close, output)
        output = nil
    end
end

local function run()
    local output_codec, output_rate, output_channels, output_bits =
        bm.get_audio_codec_output_params("audio_dac")
    if not output_codec then
        error("[button_play_test_wav] get_audio_codec_output_params(audio_dac) failed: " .. tostring(output_rate))
    end

    local out_h, out_err = audio.new_output(output_codec, output_rate, output_channels, output_bits)
    if not out_h then
        error("[button_play_test_wav] new_output failed: " .. tostring(out_err))
    end
    output = out_h

    local btn_h, herr = btn.new(BUTTON_GPIO_NUM, 0)
    if not btn_h then
        error("[button_play_test_wav] button.new failed: " .. tostring(herr))
    end
    handle = btn_h

    btn.on(handle, "single_click", function()
        if is_playing then
            print("[button_play_test_wav] playback already in progress")
            return
        end

        is_playing = true
        print("[button_play_test_wav] playing " .. TEST_WAV_PATH .. " ...")
        audio.play_wav(output, TEST_WAV_PATH)
        print("[button_play_test_wav] playback done")
        is_playing = false
    end)

    print(string.format("[button_play_test_wav] button gpio=%d output=%dHz/%dch/%dbit wav=%s",
          BUTTON_GPIO_NUM, output_rate, output_channels, output_bits, wav_name))
    print("[button_play_test_wav] press the button once to play")
    print("[button_play_test_wav] running for " .. tostring(RUN_TIME_MS) .. " ms")

    for _ = 1, math.max(1, math.floor(RUN_TIME_MS / POLL_INTERVAL_MS)) do
        btn.dispatch()
        delay.delay_ms(POLL_INTERVAL_MS)
    end
end

local ok, err = xpcall(run, debug.traceback)
cleanup()
if not ok then
    error(err)
end

print("[button_play_test_wav] done")
