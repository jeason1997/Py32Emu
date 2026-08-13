#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_iwdg.sh"
output=$($root/build/py32emu "$root/build/official-iwdg/iwdg.elf" --steps 25000000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q '状态=step-limit'
printf '%s\n' "$output" | grep -q 'GPIOA: .*ODR=0x0020'
if printf '%s\n' "$output" | grep -q 'RESET: IWDG='; then
    echo 'official IWDG firmware reset unexpectedly' >&2
    exit 1
fi
