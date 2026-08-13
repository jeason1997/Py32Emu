#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_tim1.sh"
output=$($root/build/py32emu \
    "$root/build/official-tim1/tim1_arr.elf" --steps 5500000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=step-limit'
printf '%s\n' "$output" | grep -q 'GPIOA: .*ODR=0x0000'
printf '%s\n' "$output" | grep -q 'TIM1: .*ARR=6399'
