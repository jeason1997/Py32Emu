#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_lptim.sh"
output=$($root/build/py32emu "$root/build/official-lptim/lptim.elf" \
    --steps 12000000 --pulse PB2 --pulse-at 1000)
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -Eq 'LPTIM1: .*matches=[2-9][0-9]*'
