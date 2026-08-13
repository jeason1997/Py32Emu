#include "py32emu/chips/chip.h"

enum { PY32F002A_PERIPHERALS = PY32_PERIPHERAL_GPIOA |
    PY32_PERIPHERAL_GPIOB | PY32_PERIPHERAL_GPIOF |
    PY32_PERIPHERAL_USART1 | PY32_PERIPHERAL_TIM1 |
    PY32_PERIPHERAL_TIM16 | PY32_PERIPHERAL_SPI1 |
    PY32_PERIPHERAL_I2C1 | PY32_PERIPHERAL_ADC1 |
    PY32_PERIPHERAL_EXTI | PY32_PERIPHERAL_CRC |
    PY32_PERIPHERAL_IWDG };

static const Py32ChipDescription py32f002ax5 = {
    .name = "py32f002ax5",
    .flash_base = 0x08000000u,
    .flash_size = 20u * 1024u,
    .flash_page_size = 128u,
    .sram_base = 0x20000000u,
    .sram_size = 3u * 1024u,
    .external_irq_count = 32u,
    .reset_clock_hz = 24000000u,
    .peripherals = PY32F002A_PERIPHERALS
};

static const Py32ChipDescription py32f002a_32k = {
    .name = "py32f002a-32k",
    .flash_base = 0x08000000u,
    .flash_size = 32u * 1024u,
    .flash_page_size = 128u,
    .sram_base = 0x20000000u,
    .sram_size = 4u * 1024u,
    .external_irq_count = 32u,
    .reset_clock_hz = 24000000u,
    .peripherals = PY32F002A_PERIPHERALS
};

const Py32ChipDescription *py32f002ax5_description(void)
{
    return &py32f002ax5;
}

const Py32ChipDescription *py32f002a_32k_description(void)
{
    return &py32f002a_32k;
}
