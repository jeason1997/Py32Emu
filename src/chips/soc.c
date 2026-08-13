#include "py32emu/chips/soc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t size, const char *message)
{
    if (error != NULL && size > 0u) snprintf(error, size, "%s", message);
}

void py32_soc_init(Py32Soc *soc)
{
    memset(soc, 0, sizeof(*soc));
}

void py32_soc_destroy(Py32Soc *soc)
{
    if (soc == NULL) return;
    free(soc->flash);
    free(soc->sram);
    py32_soc_init(soc);
}

bool py32_soc_configure(Py32Soc *soc,
                        const Py32ChipDescription *description,
                        const Py32FirmwareImage *firmware,
                        char *error, size_t error_size)
{
    uint64_t firmware_end;

    if (soc == NULL || description == NULL || firmware == NULL) {
        set_error(error, error_size, "SoC 配置参数无效");
        return false;
    }
    firmware_end = (uint64_t)firmware->load_address + firmware->size;
    if (firmware->load_address < description->flash_base ||
        firmware_end > (uint64_t)description->flash_base +
                       description->flash_size) {
        set_error(error, error_size, "固件装载范围超出目标 Flash");
        return false;
    }

    py32_soc_destroy(soc);
    soc->description = description;
    soc->flash = malloc(description->flash_size);
    soc->sram = calloc(description->sram_size, 1);
    if (soc->flash == NULL || soc->sram == NULL) {
        set_error(error, error_size, "无法分配模拟器存储器");
        py32_soc_destroy(soc);
        return false;
    }
    /* 擦除态 Flash 为全 1，固件不必覆盖完整器件容量。 */
    memset(soc->flash, 0xFF, description->flash_size);
    memset(soc->system_memory, 0, sizeof(soc->system_memory));
    memset(soc->flash_registers, 0, sizeof(soc->flash_registers));
    memset(soc->syscfg_registers, 0, sizeof(soc->syscfg_registers));
    memset(soc->exti_registers, 0, sizeof(soc->exti_registers));
    soc->adc_common_ccr = 0u;
    /* 工厂校准区中固件常用的 Flash 容量字段，单位 KiB。 */
    soc->system_memory[0xFFCu] = (uint8_t)(description->flash_size / 1024u);
    memcpy(soc->flash + (firmware->load_address - description->flash_base),
           firmware->data, firmware->size);

    py32_bus_init(&soc->bus);
    py32_rcc_reset(&soc->rcc);
    py32_gpio_reset(&soc->gpioa, &soc->rcc.iopenr, 1u << 0);
    py32_gpio_reset(&soc->gpiob, &soc->rcc.iopenr, 1u << 1);
    py32_gpio_reset(&soc->gpiof, &soc->rcc.iopenr, 1u << 5);
    py32_usart_reset(&soc->usart1, &soc->rcc.apbenr2, 1u << 14);
    py32_timer_reset(&soc->tim1, &soc->rcc.apbenr2, 1u << 11);
    py32_timer_reset(&soc->tim16, &soc->rcc.apbenr2, 1u << 17);
    py32_spi_reset(&soc->spi1, &soc->rcc.apbenr2, 1u << 12);
    py32_i2c_reset(&soc->i2c1, &soc->rcc.apbenr1, 1u << 21);
    py32_adc_reset(&soc->adc1, &soc->rcc.apbenr2, 1u << 20);
    /* Cortex-M0+ 复位时 Code 区 0x00000000 是主 Flash 的只读别名。 */
    if (!py32_bus_add_memory(&soc->bus, "flash-alias", 0x00000000u,
                             soc->flash, description->flash_size, true) ||
        !py32_bus_add_memory(&soc->bus, "flash", description->flash_base,
                             soc->flash, description->flash_size, true) ||
        !py32_bus_add_memory(&soc->bus, "sram", description->sram_base,
                             soc->sram, description->sram_size, false) ||
        !py32_bus_add_memory(&soc->bus, "system-memory", 0x1FFF0000u,
                             soc->system_memory,
                             sizeof(soc->system_memory), true) ||
        !py32_bus_add_memory(&soc->bus, "flash-registers", 0x40022000u,
                             soc->flash_registers,
                             sizeof(soc->flash_registers), false) ||
        !py32_bus_add_memory(&soc->bus, "syscfg", 0x40010000u,
                             soc->syscfg_registers,
                             sizeof(soc->syscfg_registers), false) ||
        !py32_bus_add_memory(&soc->bus, "exti", 0x40021800u,
                             soc->exti_registers,
                             sizeof(soc->exti_registers), false) ||
        !py32_bus_add_memory(&soc->bus, "adc-common", 0x40012708u,
                             (uint8_t *)&soc->adc_common_ccr,
                             sizeof(soc->adc_common_ccr), false) ||
        !py32_bus_add_device(&soc->bus, "rcc", 0x40021000u, 0x64u,
                             py32_rcc_read, py32_rcc_write, &soc->rcc) ||
        !py32_bus_add_device(&soc->bus, "gpioa", 0x50000000u, 0x2Cu,
                             py32_gpio_read, py32_gpio_write, &soc->gpioa) ||
        !py32_bus_add_device(&soc->bus, "gpiob", 0x50000400u, 0x2Cu,
                             py32_gpio_read, py32_gpio_write, &soc->gpiob) ||
        !py32_bus_add_device(&soc->bus, "gpiof", 0x50001400u, 0x2Cu,
                             py32_gpio_read, py32_gpio_write, &soc->gpiof) ||
        !py32_bus_add_device(&soc->bus, "usart1", 0x40013800u, 0x1Cu,
                             py32_usart_read, py32_usart_write,
                             &soc->usart1) ||
        !py32_bus_add_device(&soc->bus, "tim1", 0x40012C00u, 0x54u,
                             py32_timer_read, py32_timer_write,
                             &soc->tim1) ||
        !py32_bus_add_device(&soc->bus, "tim16", 0x40014400u, 0x54u,
                             py32_timer_read, py32_timer_write,
                             &soc->tim16) ||
        !py32_bus_add_device(&soc->bus, "spi1", 0x40013000u, 0x10u,
                             py32_spi_read, py32_spi_write, &soc->spi1) ||
        !py32_bus_add_device(&soc->bus, "i2c1", 0x40005400u, 0x24u,
                             py32_i2c_read, py32_i2c_write, &soc->i2c1) ||
        !py32_bus_add_device(&soc->bus, "adc1", 0x40012400u, 0x48u,
                             py32_adc_read, py32_adc_write, &soc->adc1)) {
        set_error(error, error_size, "无法建立芯片内存映射");
        py32_soc_destroy(soc);
        return false;
    }
    cortex_m0_init(&soc->cpu, &soc->bus);
    py32_system_reset(&soc->system, &soc->cpu,
                      description->reset_clock_hz);
    if (!py32_bus_add_device(&soc->bus, "system-control", 0xE000E000u,
                             0x1000u, py32_system_read,
                             py32_system_write, &soc->system)) {
        set_error(error, error_size, "无法建立系统控制空间");
        py32_soc_destroy(soc);
        return false;
    }
    return true;
}

bool py32_soc_reset(Py32Soc *soc, char *error, size_t error_size)
{
    if (soc == NULL || soc->description == NULL || soc->flash == NULL) {
        set_error(error, error_size, "SoC 尚未配置");
        return false;
    }
    memset(soc->sram, 0, soc->description->sram_size);
    memset(soc->flash_registers, 0, sizeof(soc->flash_registers));
    memset(soc->syscfg_registers, 0, sizeof(soc->syscfg_registers));
    memset(soc->exti_registers, 0, sizeof(soc->exti_registers));
    soc->adc_common_ccr = 0u;
    py32_rcc_reset(&soc->rcc);
    py32_gpio_reset(&soc->gpioa, &soc->rcc.iopenr, 1u << 0);
    py32_gpio_reset(&soc->gpiob, &soc->rcc.iopenr, 1u << 1);
    py32_gpio_reset(&soc->gpiof, &soc->rcc.iopenr, 1u << 5);
    py32_usart_reset(&soc->usart1, &soc->rcc.apbenr2, 1u << 14);
    py32_timer_reset(&soc->tim1, &soc->rcc.apbenr2, 1u << 11);
    py32_timer_reset(&soc->tim16, &soc->rcc.apbenr2, 1u << 17);
    py32_spi_reset(&soc->spi1, &soc->rcc.apbenr2, 1u << 12);
    py32_i2c_reset(&soc->i2c1, &soc->rcc.apbenr1, 1u << 21);
    py32_adc_reset(&soc->adc1, &soc->rcc.apbenr2, 1u << 20);
    py32_system_reset(&soc->system, &soc->cpu,
                      soc->description->reset_clock_hz);
    soc->bus.faulted = false;
    if (!cortex_m0_reset(&soc->cpu, 0x00000000u)) {
        set_error(error, error_size, "复位向量无效或不可读");
        return false;
    }
    return true;
}

CortexM0StepResult py32_soc_step(Py32Soc *soc)
{
    if (soc == NULL) {
        CortexM0StepResult empty = {0};
        return empty;
    }
    py32_system_service_exception(&soc->system);
    {
        CortexM0StepResult result = cortex_m0_step(&soc->cpu);
        py32_system_tick(&soc->system, result.cycles);
        if (py32_timer_tick(&soc->tim1, result.cycles))
            soc->system.nvic_pending |= 1u << 13;
        if (py32_timer_tick(&soc->tim16, result.cycles))
            soc->system.nvic_pending |= 1u << 21;
        if (py32_spi_irq_pending(&soc->spi1))
            soc->system.nvic_pending |= 1u << 25;
        if (py32_i2c_irq_pending(&soc->i2c1))
            soc->system.nvic_pending |= 1u << 23;
        if (py32_adc_irq_pending(&soc->adc1))
            soc->system.nvic_pending |= 1u << 12;
        return result;
    }
}
