#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=$root/build/iwdg-reset
mkdir -p "$build"
arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -Os -ffreestanding \
    -fno-builtin -ffunction-sections -fdata-sections -nostdlib \
    -Wl,--gc-sections -T "$root/tests/firmware/official_py32f002a.ld" \
    "$root/tests/firmware/startup_py32f002a.s" \
    "$root/tests/firmware/iwdg_reset.c" \
    "$root/tests/firmware/runtime.c" -lgcc -o "$build/iwdg_reset.elf"
output=$($root/build/py32emu "$build/iwdg_reset.elf" --steps 100000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=breakpoint'
printf '%s\n' "$output" | grep -q 'GPIOA: .*ODR=0x0020'
printf '%s\n' "$output" | grep -q 'RESET: IWDG=1 CSR=0x20000000'
