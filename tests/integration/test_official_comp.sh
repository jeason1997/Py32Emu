#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sh "$root/tests/integration/build_official_comp.sh" Polling
polling=$($root/build/py32emu "$root/build/official-comp-Polling/comp.elf" \
    --steps 50000 --analog PA1=1800 --analog-at 10000)
printf '%s\n' "$polling"
printf '%s\n' "$polling" | grep -Eq 'COMP: CSR1=0x4[0-9A-F]{7} .*transitions=1/'

sh "$root/tests/integration/build_official_comp.sh" IT
interrupt=$($root/build/py32emu "$root/build/official-comp-IT/comp.elf" \
    --steps 50000 --analog PA1=1800 --analog-at 10000)
printf '%s\n' "$interrupt"
printf '%s\n' "$interrupt" | grep -Eq 'COMP: CSR1=0x4[0-9A-F]{7} .*transitions=1/'
printf '%s\n' "$interrupt" | grep -q 'GPIOA: .*ODR=0x0000'
