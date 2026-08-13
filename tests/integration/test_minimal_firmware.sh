#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build="$root/build/firmware"
mkdir -p "$build"

arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -Os -ffreestanding \
    -fno-builtin -nostdlib -Wl,--gc-sections \
    -T "$root/tests/firmware/py32f002a.ld" \
    "$root/tests/firmware/minimal_gpio.c" -o "$build/minimal_gpio.elf"
arm-none-eabi-objcopy -O binary "$build/minimal_gpio.elf" \
    "$build/minimal_gpio.bin"

output=$($root/build/py32emu "$build/minimal_gpio.bin" --steps 100)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=breakpoint'
printf '%s\n' "$output" | grep -q 'ODR=0x0020'
