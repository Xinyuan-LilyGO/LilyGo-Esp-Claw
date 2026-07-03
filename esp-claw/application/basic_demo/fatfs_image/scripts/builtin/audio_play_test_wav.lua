local audio = require("audio")
local bm = require("board_manager")
local storage = require("storage")

local TEST_WAV_PATH = storage.join_path(storage.get_root_dir(), "test.wav")

local output_codec, output_rate, output_channels, output_bits =
    bm.get_audio_codec_output_params("audio_dac")
if not output_codec then
    print("[audio_play_test_wav] ERROR: get_audio_codec_output_params(audio_dac) failed: " .. tostring(output_rate))
    return
end

local output, out_err = audio.new_output(output_codec, output_rate, output_channels, output_bits)
if not output then
    print("[audio_play_test_wav] ERROR: new_output failed: " .. tostring(out_err))
    return
end

local ok, err = xpcall(function()
    print(string.format("[audio_play_test_wav] output=%dHz/%dch/%dbit",
          output_rate, output_channels, output_bits))
    print("[audio_play_test_wav] playing " .. TEST_WAV_PATH .. " ...")
    audio.play_wav(output, TEST_WAV_PATH)
end, debug.traceback)

pcall(audio.close, output)
if not ok then
    error(err)
end
print("[audio_play_test_wav] done")
