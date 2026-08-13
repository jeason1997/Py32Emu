#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_exti.sh"
output=$($root/build/py32emu "$root/build/official-exti/exti_it.elf" \
    --steps 50000 --pulse PB2 --pulse-at 10000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=step-limit'
printf '%s\n' "$output" | grep -q 'GPIOA: .*ODR=0x0000'
