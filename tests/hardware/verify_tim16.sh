#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
common=/mnt/e/Projects/MCU_Project/32/Py32/Common
openocd=$common/tools/openocd-linux/bin/openocd
scripts=$common/tools/openocd-linux/share/openocd/scripts
target=$common/Core/OpenOCD/py32f002a_32k.cfg
firmware=$root/build/official-tim16/tim16_arr.elf

test -x "$openocd"
test -f "$firmware"
"$openocd" -s "$scripts" \
    -f interface/cmsis-dap.cfg -f "$target" \
    -c "program $firmware verify reset" \
    -c 'sleep 200; halt; echo [format "GPIOA_ODR=0x%08X" [mrw 0x50000014]]; echo [format "TIM16_CNT=0x%08X" [mrw 0x40014424]]; echo [format "TIM16_SR=0x%08X" [mrw 0x40014410]]; shutdown'
