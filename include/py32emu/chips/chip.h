#ifndef PY32EMU_CHIPS_CHIP_H
#define PY32EMU_CHIPS_CHIP_H

#include <stdint.h>

enum {
    PY32_PERIPHERAL_GPIOA  = 1u << 0,
    PY32_PERIPHERAL_GPIOB  = 1u << 1,
    PY32_PERIPHERAL_GPIOF  = 1u << 2,
    PY32_PERIPHERAL_USART1 = 1u << 3,
    PY32_PERIPHERAL_TIM1   = 1u << 4,
    PY32_PERIPHERAL_TIM16  = 1u << 5,
    PY32_PERIPHERAL_SPI1   = 1u << 6,
    PY32_PERIPHERAL_I2C1   = 1u << 7,
    PY32_PERIPHERAL_ADC1   = 1u << 8,
    PY32_PERIPHERAL_EXTI   = 1u << 9
};

typedef struct {
    const char *name;
    uint32_t flash_base;
    uint32_t flash_size;
    uint32_t flash_page_size;
    uint32_t sram_base;
    uint32_t sram_size;
    unsigned external_irq_count;
    uint32_t reset_clock_hz;
    uint32_t peripherals;
} Py32ChipDescription;

static inline int py32_chip_has_peripheral(
    const Py32ChipDescription *chip, uint32_t peripheral)
{
    return chip != 0 && (chip->peripherals & peripheral) != 0u;
}

const Py32ChipDescription *py32_chip_by_name(const char *name);
const Py32ChipDescription *py32f002ax5_description(void);
const Py32ChipDescription *py32f002a_32k_description(void);

#endif
