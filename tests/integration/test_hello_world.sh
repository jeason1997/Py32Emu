#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
make -C "$root/examples/hello_world" clean all
output=$($root/build/py32emu \
    "$root/examples/hello_world/build/hello_world.elf" --steps 100000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=breakpoint'
printf '%s\n' "$output" | grep -q \
    'USART1 TX (13): 48 65 6C 6C 6F 20 57 6F 72 6C 64 0D 0A'
