#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build="$root/build/thumb-register-edges"
mkdir -p "$build"
arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -Os -std=c11 \
    -Wall -Wextra -ffreestanding -fno-builtin -ffunction-sections -fdata-sections \
    -nostdlib -Wl,--gc-sections -T "$root/tests/firmware/official_py32f002a.ld" \
    "$root/tests/firmware/startup_py32f002a.s" \
    "$root/tests/firmware/thumb_register_edges.s" \
    "$root/tests/firmware/thumb_register_edges.c" \
    "$root/tests/firmware/runtime.c" -lgcc -o "$build/thumb_register_edges.elf"
output=$("$root/build/py32emu" "$build/thumb_register_edges.elf" --steps 10000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=breakpoint'
printf '%s\n' "$output" | grep -q 'GPIOA: .*ODR=0x0020'
