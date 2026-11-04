#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build="$root/build/firmware"
mkdir -p "$build"

arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -Os -ffreestanding \
    -fno-builtin -nostdlib -Wl,--gc-sections \
    -T "$root/tests/firmware/py32f002a.ld" \
    "$root/tests/firmware/wav_tone.c" -o "$build/wav_tone.elf"

output=$("$root/build/py32emu" "$build/wav_tone.elf" --steps 10000 \
    --wav "$build/wav_tone.wav")
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q 'WAV: .* 256 样本, 0.01 秒'

python3 - "$build/wav_tone.wav" <<'PY'
import struct
import sys
import wave

with wave.open(sys.argv[1], "rb") as wav:
    assert wav.getnchannels() == 1
    assert wav.getsampwidth() == 2
    assert wav.getframerate() == 22050
    assert wav.getnframes() == 256
    samples = struct.unpack("<256h", wav.readframes(256))
expected = [(((i * 37) & 0xFF) - 128) * 256 for i in range(256)]
assert list(samples) == expected, (samples[:4], expected[:4])
PY

echo "WAV export test passed."
