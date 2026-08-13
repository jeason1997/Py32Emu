#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_flash.sh"
output=$($root/build/py32emu "$root/build/official-flash/flash.elf" \
    --steps 300000 --pulse PB2 --pulse-at 1000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q 'FLASH: erase=2 program-words=64'
printf '%s\n' "$output" | grep -q '@0x08002000=0x01010101'
