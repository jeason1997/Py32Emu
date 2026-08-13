#ifndef PY32EMU_CHIPS_SOC_H
#define PY32EMU_CHIPS_SOC_H

#include "py32emu/chips/chip.h"
#include "py32emu/core/bus.h"
#include "py32emu/core/cortex_m0.h"
#include "py32emu/firmware/image.h"
#include "py32emu/peripherals/gpio.h"
#include "py32emu/peripherals/rcc.h"
#include "py32emu/peripherals/system.h"
#include "py32emu/peripherals/usart.h"
#include "py32emu/peripherals/timer.h"
#include "py32emu/peripherals/spi.h"
#include "py32emu/peripherals/i2c.h"
#include "py32emu/peripherals/adc.h"
#include "py32emu/peripherals/exti.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const Py32ChipDescription *description;
    Py32Bus bus;
    CortexM0 cpu;
    Py32Rcc rcc;
    Py32Gpio gpioa, gpiob, gpiof;
    Py32System system;
    Py32Usart usart1;
    Py32Timer tim1, tim16;
    Py32Spi spi1;
    Py32I2c i2c1;
    Py32Adc adc1;
    Py32Exti exti;
    uint8_t *flash;
    uint8_t *sram;
    uint8_t system_memory[4096];
    uint8_t flash_registers[0x124];
    uint8_t syscfg_registers[0x88];
    uint32_t adc_common_ccr;
} Py32Soc;

void py32_soc_init(Py32Soc *soc);
void py32_soc_destroy(Py32Soc *soc);
bool py32_soc_configure(Py32Soc *soc,
                        const Py32ChipDescription *description,
                        const Py32FirmwareImage *firmware,
                        char *error, size_t error_size);
bool py32_soc_reset(Py32Soc *soc, char *error, size_t error_size);
CortexM0StepResult py32_soc_step(Py32Soc *soc);
void py32_soc_set_gpio_input(Py32Soc *soc, unsigned port, unsigned pin,
                             bool driven, bool high);

#endif
