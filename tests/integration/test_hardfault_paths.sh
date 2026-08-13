#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=$root/build/hardfault-paths
mkdir -p "$build"
arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -Os -ffreestanding \
    -fno-builtin -ffunction-sections -fdata-sections -nostdlib \
    -Wl,--gc-sections -T "$root/tests/firmware/official_py32f002a.ld" \
    "$root/tests/firmware/startup_py32f002a.s" \
    "$root/tests/firmware/hardfault_paths.s" \
    "$root/tests/firmware/hardfault_paths.c" \
    "$root/tests/firmware/runtime.c" -lgcc \
    -o "$build/hardfault_paths.elf"
output=$($root/build/py32emu "$build/hardfault_paths.elf" --steps 10000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=breakpoint'
printf '%s\n' "$output" | grep -q 'GPIOA: .*ODR=0x0020'
