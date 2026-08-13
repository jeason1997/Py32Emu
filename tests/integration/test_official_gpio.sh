#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_gpio.sh"
trace=$($root/build/py32emu \
    "$root/build/official-gpio/gpio_toggle.elf" --steps 400 --trace)
printf '%s\n' "$trace" | grep -q '<HAL_GPIO_Init+'
output=$($root/build/py32emu \
    "$root/build/official-gpio/gpio_toggle.bin" --steps 3000000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=step-limit'
# 板级 LED 初始化先把 PA5 置高；250 ms 后官方主循环第一次翻转应将它拉低。
printf '%s\n' "$output" | grep -q 'ODR=0x0000'
