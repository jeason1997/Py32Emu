#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
sdk=/mnt/e/BaiduNetdiskDownload/Py32资料/PY32F0xx_Firmware_V1.1.3
project=$sdk/Projects/PY32F002A-STK/Example/TIM1/TIM1_ARR
build=$root/build/official-tim1
mkdir -p "$build/obj"

includes="-I$project/Inc -I$sdk/Drivers/BSP/PY32F002xx_Start_Kit -I$sdk/Drivers/CMSIS/Include -I$sdk/Drivers/CMSIS/Device/PY32F0xx/Include -I$sdk/Drivers/PY32F0xx_HAL_Driver/Inc"
common="-mcpu=cortex-m0plus -mthumb -Os -ffunction-sections -fdata-sections -DPY32F002Ax5 -DUSE_HAL_DRIVER $includes"
sources="
$project/Src/system_py32f0xx.c
$project/Src/main.c
$project/Src/py32f0xx_hal_msp.c
$project/Src/py32f0xx_it.c
$sdk/Drivers/BSP/PY32F002xx_Start_Kit/py32f002xx_Start_Kit.c
$sdk/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal.c
$sdk/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_rcc.c
$sdk/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_gpio.c
$sdk/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_pwr.c
$sdk/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_cortex.c
$sdk/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_tim.c
$sdk/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_tim_ex.c
"

objects=""
for source in $sources; do
    object=$build/obj/$(basename "$source" .c).o
    arm-none-eabi-gcc $common -std=c11 -c "$source" -o "$object"
    objects="$objects $object"
done
arm-none-eabi-gcc $common -std=c11 -ffreestanding -fno-builtin -c \
    "$root/tests/firmware/runtime.c" -o "$build/obj/runtime.o"
objects="$objects $build/obj/runtime.o"
arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -c \
    "$root/tests/firmware/startup_py32f002a.s" -o "$build/obj/startup.o"
arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb -nostdlib \
    -Wl,--gc-sections -T "$root/tests/firmware/official_py32f002a.ld" \
    "$build/obj/startup.o" $objects -lgcc -o "$build/tim1_arr.elf"
arm-none-eabi-objcopy -O binary "$build/tim1_arr.elf" "$build/tim1_arr.bin"
arm-none-eabi-size "$build/tim1_arr.elf"
