#include "py32emu/chips/chip.h"
#include "py32emu/chips/soc.h"
#include "py32emu/core/bus.h"
#include "py32emu/core/cortex_m0.h"
#include "py32emu/core/disassembler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t spi_echo(void *context, uint16_t output, unsigned bits)
{
    unsigned *calls = context;
    ++*calls;
    return (uint16_t)(output ^ (bits == 8u ? 0xFFu : 0xFFFFu));
}

typedef struct { unsigned starts, writes, reads, stops; uint8_t last; } I2cTest;

static bool i2c_start(void *context, uint8_t address, bool read)
{
    I2cTest *test = context;
    ++test->starts;
    test->last = (uint8_t)((address << 1) | read);
    return address == 0x50u;
}

static bool i2c_write(void *context, uint8_t value)
{
    I2cTest *test = context;
    ++test->writes; test->last = value;
    return true;
}

static uint8_t i2c_read(void *context)
{
    I2cTest *test = context;
    ++test->reads;
    return (uint8_t)(0x40u + test->reads);
}

static void i2c_stop(void *context) { ++((I2cTest *)context)->stops; }

static const Py32I2cTargetOps i2c_ops = {
    i2c_start, i2c_write, i2c_read, i2c_stop
};

int main(void)
{
    Py32Bus bus;
    uint8_t ram[3u * 1024u] = {0};
    uint8_t flash[8] = {0x78, 0x56, 0x34, 0x12};
    uint32_t value;
    const Py32ChipDescription *chip;
    unsigned spi_calls = 0;
    I2cTest i2c_test = {0};
    CortexM0 cpu;
    Py32Soc soc;
    Py32FirmwareImage image;
    char error[128];
    uint8_t program[96];
    char assembly[96];

    assert(py32_thumb_disassemble(0x08000100u, 0x2005u, 0u,
                                  assembly, sizeof(assembly)) == 1u);
    assert(strcmp(assembly, "movs r0, #5") == 0);
    assert(py32_thumb_disassemble(0x08000100u, 0xF000u, 0xF800u,
                                  assembly, sizeof(assembly)) == 2u);
    assert(strncmp(assembly, "bl 0x", 5u) == 0);

    py32_bus_init(&bus);
    assert(py32_bus_add_memory(&bus, "flash", 0x08000000u,
                               flash, sizeof(flash), true));
    assert(py32_bus_add_memory(&bus, "sram", 0x20000000u,
                               ram, sizeof(ram), false));
    assert(!py32_bus_add_memory(&bus, "overlap", 0x20000008u,
                                ram, sizeof(ram), false));
    assert(py32_bus_read(&bus, 0x08000000u, 4, &value));
    assert(value == 0x12345678u);
    assert(!py32_bus_write(&bus, 0x08000000u, 1, 0));
    assert(bus.faulted && bus.fault_address == 0x08000000u);
    assert(py32_bus_write(&bus, 0x20000004u, 4, 0xAABBCCDDu));
    assert(py32_bus_read(&bus, 0x20000004u, 4, &value));
    assert(value == 0xAABBCCDDu);
    assert(!py32_bus_read(&bus, 0x20000001u, 2, &value));
    assert(bus.faulted && bus.fault_address == 0x20000001u);

    chip = py32_chip_by_name("py32f002a");
    assert(chip != NULL);
    assert(chip->flash_size == 20u * 1024u);
    assert(chip->sram_size == 3u * 1024u);
    assert(py32_chip_by_name("unknown") == NULL);
    assert(py32_chip_by_name("py32f002a-32k")->flash_size == 32u * 1024u);
    assert(py32_chip_by_name("py32f002a-32k")->sram_size == 4u * 1024u);
    py32_firmware_image_init(&image);
    assert(py32_firmware_load_hex(&image, "tests/fixtures/minimal.hex",
                                  chip->flash_base, chip->flash_size,
                                  error, sizeof(error)));
    assert(image.size == 4u && image.data[0] == 1u && image.data[3] == 4u);
    py32_firmware_image_free(&image);

    /*
     * 最小 Thumb 程序：MOVS r0,#5; ADDS r0,#3; SUBS r0,#1;
     * STR r0,[r1,#0]; LDR r2,[r1,#0]; PUSH {r2}; POP {r3}; BKPT。
     */
    memset(program, 0, sizeof(program));
    program[0] = 0x00; program[1] = 0x0C;
    program[2] = 0x00; program[3] = 0x20; /* 初始 MSP = 0x20000C00 */
    program[4] = 0x09; program[5] = 0x00;
    program[6] = 0x00; program[7] = 0x08; /* reset = 0x08000009 */
    program[8] = 0x05; program[9] = 0x20; /* MOVS r0,#5 */
    program[10] = 0x03; program[11] = 0x30; /* ADDS r0,#3 */
    program[12] = 0x01; program[13] = 0x38; /* SUBS r0,#1 */
    program[14] = 0x08; program[15] = 0x60; /* STR r0,[r1,#0] */
    program[16] = 0x0A; program[17] = 0x68; /* LDR r2,[r1,#0] */
    program[18] = 0x04; program[19] = 0xB4; /* PUSH {r2} */
    program[20] = 0x08; program[21] = 0xBC; /* POP {r3} */
    program[22] = 0x72; program[23] = 0xB6; /* CPSID i */
    program[24] = 0x62; program[25] = 0xB6; /* CPSIE i */
    program[26] = 0x00; program[27] = 0xBE; /* BKPT #0 */
    program[28] = 0x00; program[29] = 0xB5; /* SysTick: PUSH {lr} */
    program[30] = 0x2A; program[31] = 0x24; /* MOVS r4,#42 */
    program[32] = 0x00; program[33] = 0xBD; /* POP {pc} / EXC_RETURN */
    program[34] = 0x02; program[35] = 0x26; /* IRQ5(high): MOVS r6,#2 */
    program[36] = 0x70; program[37] = 0x47; /* BX LR */
    program[38] = 0x01; program[39] = 0x25; /* IRQ6(low): MOVS r5,#1 */
    program[40] = 0x00; program[41] = 0xBF; /* NOP */
    program[42] = 0x03; program[43] = 0x25; /* MOVS r5,#3 */
    program[44] = 0x70; program[45] = 0x47; /* BX LR */
    program[60] = 0x1D; program[61] = 0x00;
    program[62] = 0x00; program[63] = 0x08; /* SysTick 向量 */
    program[84] = 0x23; program[85] = 0x00;
    program[86] = 0x00; program[87] = 0x08; /* IRQ5 vector */
    program[88] = 0x27; program[89] = 0x00;
    program[90] = 0x00; program[91] = 0x08; /* IRQ6 vector */

    py32_bus_init(&bus);
    memset(ram, 0, sizeof(ram));
    assert(py32_bus_add_memory(&bus, "flash", 0x08000000u,
                               program, sizeof(program), true));
    assert(py32_bus_add_memory(&bus, "sram", 0x20000000u,
                               ram, sizeof(ram), false));
    cortex_m0_init(&cpu, &bus);
    assert(cortex_m0_reset(&cpu, 0x08000000u));
    cpu.r[1] = 0x20000000u;
    while (!cpu.stopped) cortex_m0_step(&cpu);
    if (cpu.stop_reason != CORTEX_M0_STOP_BKPT) {
        fprintf(stderr, "unexpected stop=%s pc=%08X op=%04X fault=%08X sp=%08X\n",
                cortex_m0_stop_reason_name(cpu.stop_reason),
                cpu.last_pc, cpu.last_instruction, bus.fault_address,
                cpu.r[CORTEX_M0_SP]);
    }
    assert(cpu.stop_reason == CORTEX_M0_STOP_BKPT);
    assert(cpu.r[0] == 7u && cpu.r[2] == 7u && cpu.r[3] == 7u);
    assert(cpu.r[CORTEX_M0_SP] == 0x20000C00u);
    assert(ram[0] == 7u);

    /* SoC 装配必须同时提供 0 地址别名和 0x08000000 主 Flash。 */
    py32_soc_init(&soc);
    py32_firmware_image_init(&image);
    image.data = program;
    image.size = sizeof(program);
    image.load_address = chip->flash_base;
    assert(py32_soc_configure(&soc, chip, &image, error, sizeof(error)));
    assert(py32_soc_reset(&soc, error, sizeof(error)));
    soc.cpu.r[1] = chip->sram_base;
    while (!soc.cpu.stopped) py32_soc_step(&soc);
    assert(soc.cpu.stop_reason == CORTEX_M0_STOP_BKPT);
    assert(soc.sram[0] == 7u);

    /* RCC 时钟门控和 GPIO 原子置位/复位必须符合固件可见语义。 */
    assert(py32_bus_write(&soc.bus, 0x50000000u, 4, 0));
    assert(soc.gpioa.moder == 0xFFFFFFFFu); /* 未开时钟时写入无效 */
    assert(py32_bus_write(&soc.bus, 0x40021034u, 4, 1u));
    assert(py32_bus_write(&soc.bus, 0x50000000u, 4,
                          (soc.gpioa.moder & ~(3u << 10)) | (1u << 10)));
    assert(py32_bus_write(&soc.bus, 0x50000018u, 4, 1u << 5));
    {
        bool driven, high;
        assert(py32_gpio_pin_output(&soc.gpioa, 5, &driven, &high));
        assert(driven && high);
    }
    assert(py32_bus_write(&soc.bus, 0x50000028u, 4, 1u << 5));
    assert((soc.gpioa.odr & (1u << 5)) == 0u);
    assert(py32_bus_write(&soc.bus, 0x40021040u, 4, 1u << 14));
    assert(py32_bus_write(&soc.bus, 0x4001380Cu, 4,
                          (1u << 13) | (1u << 3) | (1u << 2)));
    assert(py32_bus_write(&soc.bus, 0x40013804u, 1, 0xA5u));
    assert(soc.usart1.tx_count == 1u && soc.usart1.tx[0] == 0xA5u);
    {
        const uint8_t input[] = {0x11u, 0x22u};
        uint32_t received;
        assert(py32_usart_receive(&soc.usart1, input, sizeof(input)) == 2u);
        assert(py32_bus_read(&soc.bus, 0x40013800u, 4, &received));
        assert((received & (1u << 5)) != 0u);
        assert(py32_bus_read(&soc.bus, 0x40013804u, 1, &received));
        assert(received == 0x11u);
    }
    assert(py32_bus_write(&soc.bus, 0x40021040u, 4,
                          (1u << 11) | (1u << 14) | (1u << 17)));
    assert(py32_bus_write(&soc.bus, 0x40012C28u, 4, 0u));
    assert(py32_bus_write(&soc.bus, 0x40012C2Cu, 4, 1u));
    assert(py32_bus_write(&soc.bus, 0x40012C0Cu, 4, 1u));
    assert(py32_bus_write(&soc.bus, 0x40012C00u, 4, 1u));
    assert(py32_timer_tick(&soc.tim1, 2u));
    assert((soc.tim1.reg[4] & 1u) != 0u);
    assert(py32_bus_write(&soc.bus, 0x40014428u, 4, 1u)); /* PSC=1 */
    assert(py32_bus_write(&soc.bus, 0x4001442Cu, 4, 2u)); /* ARR=2 */
    assert(py32_bus_write(&soc.bus, 0x4001440Cu, 4, 1u)); /* UIE */
    assert(py32_bus_write(&soc.bus, 0x40014400u, 4, 1u)); /* CEN */
    assert(!py32_timer_tick(&soc.tim16, 5u));
    assert(py32_timer_tick(&soc.tim16, 1u));
    assert((soc.tim16.reg[4] & 1u) != 0u);
    assert(py32_bus_write(&soc.bus, 0x40021040u, 4,
                          (1u << 12) | (1u << 14) | (1u << 17)));
    py32_spi_connect(&soc.spi1, spi_echo, &spi_calls);
    assert(py32_bus_write(&soc.bus, 0x40013000u, 4, 1u << 6));
    assert(py32_bus_write(&soc.bus, 0x40013004u, 4, 1u << 6));
    assert(py32_bus_write(&soc.bus, 0x4001300Cu, 1, 0x35u));
    assert(spi_calls == 1u && soc.spi1.tx[0] == 0x35u);
    assert(py32_spi_irq_pending(&soc.spi1));
    {
        uint32_t response;
        assert(py32_bus_read(&soc.bus, 0x4001300Cu, 1, &response));
        assert(response == 0xCAu);
        assert(!py32_spi_irq_pending(&soc.spi1));
    }
    assert(py32_bus_write(&soc.bus, 0x4002103Cu, 4, 1u << 21));
    py32_i2c_connect(&soc.i2c1, &i2c_ops, &i2c_test);
    assert(py32_bus_write(&soc.bus, 0x40005400u, 4, 1u | (1u << 8)));
    assert(py32_bus_write(&soc.bus, 0x40005410u, 1, 0xA0u));
    assert(py32_bus_read(&soc.bus, 0x40005414u, 4, &value));
    assert(py32_bus_read(&soc.bus, 0x40005418u, 4, &value));
    assert(py32_bus_write(&soc.bus, 0x40005410u, 1, 0x5Au));
    assert(i2c_test.starts == 1u && i2c_test.writes == 1u);
    assert(soc.i2c1.tx_count == 1u && soc.i2c1.tx[0] == 0x5Au);
    assert(py32_bus_write(&soc.bus, 0x40005400u, 4, 1u | (1u << 9)));
    assert(i2c_test.stops == 1u);
    assert(py32_bus_write(&soc.bus, 0x40021040u, 4,
                          soc.rcc.apbenr2 | (1u << 20)));
    py32_adc_set_channel(&soc.adc1, 2u, 0x456u);
    assert(py32_bus_write(&soc.bus, 0x40012428u, 4, 1u << 2));
    assert(py32_bus_write(&soc.bus, 0x40012404u, 4, 1u << 2));
    assert(py32_bus_write(&soc.bus, 0x40012408u, 4, 1u | (1u << 2)));
    assert(soc.adc1.conversion_count == 1u && soc.adc1.dr == 0x456u);
    assert(py32_adc_irq_pending(&soc.adc1));
    assert(py32_bus_read(&soc.bus, 0x40012440u, 4, &value));
    assert(value == 0x456u && !py32_adc_irq_pending(&soc.adc1));
    assert(py32_bus_write(&soc.bus, 0x40021860u, 4, 1u << 16)); /* PB2 */
    assert(py32_bus_write(&soc.bus, 0x40021804u, 4, 1u << 2)); /* falling */
    assert(py32_bus_write(&soc.bus, 0x40021880u, 4, 1u << 2)); /* unmask */
    py32_soc_set_gpio_input(&soc, 1u, 2u, true, true);
    py32_soc_set_gpio_input(&soc, 1u, 2u, true, false);
    assert((soc.exti.pr & (1u << 2)) != 0u);
    assert(py32_exti_irq_mask(&soc.exti) == (1u << 6));
    assert(py32_bus_write(&soc.bus, 0x4002180Cu, 4, 1u << 2));
    assert(soc.exti.pr == 0u);
    assert(py32_bus_write(&soc.bus, 0x40021808u, 4, 1u << 7));
    assert((soc.exti.pr & (1u << 7)) != 0u);

    assert(py32_soc_reset(&soc, error, sizeof(error)));
    assert(py32_bus_write(&soc.bus, 0xE000E014u, 4, 1u));
    assert(py32_bus_write(&soc.bus, 0xE000E018u, 4, 0u));
    assert(py32_bus_write(&soc.bus, 0xE000E010u, 4, 3u));
    while (soc.cpu.r[4] != 42u && !soc.cpu.stopped) py32_soc_step(&soc);
    assert(soc.cpu.r[4] == 42u);
    assert(soc.cpu.exception_number == 15u);
    py32_soc_step(&soc); /* BX LR 恢复线程上下文。 */
    assert(soc.cpu.exception_number == 0u);
    assert(soc.cpu.r[CORTEX_M0_SP] == 0x20000C00u);

    assert(py32_soc_reset(&soc, error, sizeof(error)));
    assert(py32_bus_write(&soc.bus, 0xE000E100u, 4,
                          (1u << 5) | (1u << 6)));
    assert(py32_bus_write(&soc.bus, 0xE000E404u, 4, 0x00C04000u));
    assert(py32_bus_write(&soc.bus, 0xE000E200u, 4, 1u << 6));
    assert(py32_system_service_exception(&soc.system));
    assert(soc.cpu.exception_number == 22u && soc.cpu.exception_depth == 1u);
    cortex_m0_step(&soc.cpu);
    assert(soc.cpu.r[5] == 1u);
    assert(py32_bus_write(&soc.bus, 0xE000E200u, 4, 1u << 5));
    assert(py32_system_service_exception(&soc.system));
    assert(soc.cpu.exception_number == 21u && soc.cpu.exception_depth == 2u);
    cortex_m0_step(&soc.cpu);
    cortex_m0_step(&soc.cpu);
    assert(soc.cpu.r[6] == 2u && soc.cpu.exception_number == 22u);
    cortex_m0_step(&soc.cpu);
    cortex_m0_step(&soc.cpu);
    cortex_m0_step(&soc.cpu);
    assert(soc.cpu.r[5] == 3u && soc.cpu.exception_number == 0u);
    assert(soc.cpu.exception_depth == 0u &&
           soc.cpu.r[CORTEX_M0_SP] == 0x20000C00u);
    py32_soc_destroy(&soc);

    puts("foundation tests passed");
    return 0;
}
