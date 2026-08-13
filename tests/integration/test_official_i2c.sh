#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_i2c.sh"
output=$($root/build/py32emu \
    "$root/build/official-i2c/i2c_master.elf" --steps 300000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q \
    'I2C1 TX (15): 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F'
printf '%s\n' "$output" | grep -q \
    'I2C1 RX (15): FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF'
