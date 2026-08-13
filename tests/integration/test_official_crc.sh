#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_crc.sh"
output=$($root/build/py32emu "$root/build/official-crc/crc.elf" --steps 10000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q 'CRC: DR=0x379E9F06'
printf '%s\n' "$output" | grep -q 'GPIOA: .*ODR=0x0000'
