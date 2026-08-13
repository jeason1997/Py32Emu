CC ?= gcc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
TARGET := $(BUILD_DIR)/py32emu
TEST_TARGET := $(BUILD_DIR)/tests/test_foundation
WEB_TARGET := $(BUILD_DIR)/py32emu-web-core

CORE_SOURCES := src/core/bus.c src/core/cortex_m0.c src/core/disassembler.c \
	src/chips/chip.c src/chips/py32f002a.c src/chips/soc.c \
	src/peripherals/rcc.c src/peripherals/gpio.c \
	src/peripherals/system.c \
	src/peripherals/usart.c \
	src/peripherals/timer.c \
	src/peripherals/spi.c \
	src/peripherals/i2c.c \
	src/peripherals/adc.c \
	src/peripherals/exti.c \
	src/peripherals/crc.c \
	src/firmware/image.c
CLI_SOURCES := src/cli/main.c
WEB_SOURCES := src/web/backend.c
TARGET_OBJECTS := $(addprefix $(OBJ_DIR)/,$(CORE_SOURCES:.c=.o) \
	$(CLI_SOURCES:.c=.o))
TEST_OBJECTS := $(addprefix $(OBJ_DIR)/,tests/unit/test_foundation.o \
	src/core/bus.o src/core/cortex_m0.o src/chips/chip.o \
	src/core/disassembler.o \
	src/chips/py32f002a.o src/chips/soc.o src/firmware/image.o)

TEST_OBJECTS += $(OBJ_DIR)/src/peripherals/rcc.o \
	$(OBJ_DIR)/src/peripherals/gpio.o $(OBJ_DIR)/src/peripherals/system.o
TEST_OBJECTS += $(OBJ_DIR)/src/peripherals/usart.o
TEST_OBJECTS += $(OBJ_DIR)/src/peripherals/timer.o
TEST_OBJECTS += $(OBJ_DIR)/src/peripherals/spi.o
TEST_OBJECTS += $(OBJ_DIR)/src/peripherals/i2c.o
TEST_OBJECTS += $(OBJ_DIR)/src/peripherals/adc.o
TEST_OBJECTS += $(OBJ_DIR)/src/peripherals/exti.o
TEST_OBJECTS += $(OBJ_DIR)/src/peripherals/crc.o
DEPFILES := $(sort $(TARGET_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d))
WEB_OBJECTS := $(addprefix $(OBJ_DIR)/,$(CORE_SOURCES:.c=.o) \
	$(WEB_SOURCES:.c=.o))
DEPFILES += $(WEB_OBJECTS:.o=.d)

.PHONY: all web-core run-web test unit-test integration-test clean

all: $(TARGET)

$(TARGET): $(TARGET_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_TARGET): $(TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

$(WEB_TARGET): $(WEB_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@

web-core: $(WEB_TARGET)

run-web: web-core
	sh frontends/web/scripts/start.sh

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

unit-test: $(TEST_TARGET)
	$(TEST_TARGET)

integration-test: $(TARGET) $(WEB_TARGET)
	sh tests/integration/test_minimal_firmware.sh
	sh tests/integration/test_official_gpio.sh
	sh tests/integration/test_official_usart.sh
	sh tests/integration/test_official_tim16.sh
	sh tests/integration/test_official_tim1.sh
	sh tests/integration/test_hello_world.sh
	sh tests/integration/test_official_spi.sh
	sh tests/integration/test_official_i2c.sh
	sh tests/integration/test_official_adc.sh
	sh tests/integration/test_official_crc.sh
	sh tests/integration/test_official_exti.sh
	sh tests/integration/test_nvic_preemption.sh
	sh tests/integration/test_dual_stack.sh
	sh tests/integration/test_thumb_control_flow.sh
	sh tests/integration/test_hardfault_paths.sh
	sh tests/integration/test_web_backend.sh
	sh tests/integration/test_web_server.sh

test: unit-test integration-test

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C examples/hello_world clean

-include $(DEPFILES)
