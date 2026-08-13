#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_tim16.sh"
output=$($root/build/py32emu \
    "$root/build/official-tim16/tim16_arr.elf" --steps 4000000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=step-limit'
# 官方例程约 133 ms 产生一次 TIM16 更新；首次回调把板载 PA5 从高翻为低。
printf '%s\n' "$output" | grep -q 'ODR=0x0000'
