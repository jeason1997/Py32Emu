ROOT := ../..
SDK ?= /mnt/e/BaiduNetdiskDownload/Py32资料/PY32F0xx_Firmware_V1.1.3
BUILD := build
TARGET := $(BUILD)/hello_world
CROSS ?= arm-none-eabi-
CC := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

PROJECT_INC := $(SDK)/Drivers
INCLUDES := -I$(SDK)/Drivers/CMSIS/Include \
	-I$(SDK)/Drivers/CMSIS/Device/PY32F0xx/Include \
	-I$(SDK)/Drivers/PY32F0xx_HAL_Driver/Inc \
	-I$(SDK)/Projects/PY32F002A-STK/Example/GPIO/GPIO_Toggle/Inc
FLAGS := -mcpu=cortex-m0plus -mthumb -Os -ffunction-sections \
	-fdata-sections -DPY32F002Ax5 -DUSE_HAL_DRIVER $(INCLUDES)
SOURCES := main.c \
	$(SDK)/Projects/PY32F002A-STK/Example/GPIO/GPIO_Toggle/Src/system_py32f0xx.c \
	$(SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal.c \
	$(SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_rcc.c \
	$(SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_gpio.c \
	$(SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_uart.c \
	$(SDK)/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_cortex.c \
	$(ROOT)/tests/firmware/runtime.c

.PHONY: all clean
all: $(TARGET).elf $(TARGET).bin

$(BUILD):
	mkdir -p $@

$(TARGET).elf: $(SOURCES) | $(BUILD)
	$(CC) $(FLAGS) -std=c11 -ffreestanding -fno-builtin \
		$(ROOT)/tests/firmware/startup_py32f002a.s $(SOURCES) \
		-nostdlib -Wl,--gc-sections \
		-T $(ROOT)/tests/firmware/official_py32f002a.ld -lgcc -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD)
