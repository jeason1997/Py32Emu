#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_usart.sh"
output=$($root/build/py32emu \
    "$root/build/official-usart/usart_polling.bin" --steps 300000)
printf '%s\n' "$output"
# 官方例程进入接收等待前，必须真实发送十二个 0xFF。
printf '%s\n' "$output" | grep -q \
    'USART1 TX (12): FF FF FF FF FF FF FF FF FF FF FF FF'

