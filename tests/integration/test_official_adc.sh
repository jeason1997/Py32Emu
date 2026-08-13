#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_adc.sh"
output=$($root/build/py32emu \
    "$root/build/official-adc/adc_vrefint.elf" --steps 100000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=step-limit'
printf '%s\n' "$output" | grep -Eq \
    'ADC1: DR=1489 CHSELR=0x1000 conversions=[1-9][0-9]*'
