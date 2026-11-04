#include "py32emu/chips/chip.h"
#include "py32emu/chips/soc.h"
#include "py32emu/core/bus.h"
#include "py32emu/core/cortex_m0.h"
#include "py32emu/core/disassembler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put_le16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static unsigned compare_count, compare_channel;
static uint32_t compare_value;

static void capture_compare(void *context, unsigned channel, uint32_t value)
{
    (void)context;
    ++compare_count;
    compare_channel = channel;
    compare_value = value;
}

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
    Py32Soc trimmed_soc;
    Py32ChipDescription trimmed_chip;
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
    assert(py32_chip_has_peripheral(chip, PY32_PERIPHERAL_CRC));
    assert(py32_chip_has_peripheral(chip, PY32_PERIPHERAL_LPTIM1));
    assert(py32_chip_has_peripheral(chip, PY32_PERIPHERAL_COMP12));
    py32_firmware_image_init(&image);
    assert(py32_firmware_load_hex(&image, "tests/fixtures/minimal.hex",
                                  chip->flash_base, chip->flash_size,
                                  error, sizeof(error)));
    assert(image.size == 4u && image.data[0] == 1u && image.data[3] == 4u);
    py32_firmware_image_free(&image);

    /*
     * 链接脚本常为 .bss/.stack 生成 Filesz=0、PAddr 位于 SRAM 的 PT_LOAD。
     * 这类段没有 Flash 内容，不应让整个 ELF 装载失败。
     */
    {
        uint8_t elf[52u + 2u * 32u + 4u] = {0};
        Py32FirmwareImage zero_image;
        FILE *elf_file;

        elf[0] = 0x7Fu; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
        elf[4] = 1u; elf[5] = 1u; elf[6] = 1u;
        put_le16(elf + 16u, 2u);         /* ET_EXEC */
        put_le16(elf + 18u, 40u);        /* EM_ARM */
        put_le32(elf + 20u, 1u);
        put_le32(elf + 24u, 0x08000001u);
        put_le32(elf + 28u, 52u);        /* e_phoff */
        put_le16(elf + 40u, 52u);
        put_le16(elf + 42u, 32u);
        put_le16(elf + 44u, 2u);
        /* 第一个 PT_LOAD：4 字节 Flash 内容。 */
        put_le32(elf + 52u, 1u);
        put_le32(elf + 52u + 4u, 52u + 64u);
        put_le32(elf + 52u + 8u, 0x08000000u);
        put_le32(elf + 52u + 12u, 0x08000000u);
        put_le32(elf + 52u + 16u, 4u);
        put_le32(elf + 52u + 20u, 4u);
        put_le32(elf + 52u + 24u, 5u);
        put_le32(elf + 52u + 28u, 4u);
        /* 第二个 PT_LOAD：仅 .bss/.stack，Filesz=0 且位于 SRAM。 */
        put_le32(elf + 84u, 1u);
        put_le32(elf + 84u + 8u, 0x20000000u);
        put_le32(elf + 84u + 12u, 0x20000000u);
        put_le32(elf + 84u + 20u, 8u);
        put_le32(elf + 84u + 24u, 6u);
        put_le32(elf + 84u + 28u, 4u);
        elf[116] = 0x78u; elf[117] = 0x56u;
        elf[118] = 0x34u; elf[119] = 0x12u;
        elf_file = fopen("build/test-zero-filesz.elf", "wb");
        assert(elf_file != NULL);
        assert(fwrite(elf, 1u, sizeof(elf), elf_file) == sizeof(elf));
        assert(fclose(elf_file) == 0);
        py32_firmware_image_init(&zero_image);
        assert(py32_firmware_load_elf(&zero_image, "build/test-zero-filesz.elf",
                                      chip->flash_base, chip->flash_size,
                                      error, sizeof(error)));
        assert(zero_image.size == 4u &&
               memcmp(zero_image.data, elf + 116u, 4u) == 0);
        py32_firmware_image_free(&zero_image);
    }

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

    /*
     * 寄存器形式 STRH/LDRH 曾因 0xF200 掩码被误判为未定义指令。用真实
     * 编码 0x5313/0x5B15 覆盖半字寄存器访问和立即数偏移形式。
     */
    {
        uint8_t half_flash[32] = {0};
        uint8_t half_ram[1024] = {0};
        Py32Bus half_bus;
        CortexM0 half_cpu;

        put_le32(half_flash, 0x20000C00u);
        put_le32(half_flash + 4u, 0x08000009u);
        put_le16(half_flash + 8u, 0x5310u);  /* STRH r0,[r2,r4] */
        put_le16(half_flash + 10u, 0x5B15u); /* LDRH r5,[r2,r4] */
        put_le16(half_flash + 12u, 0x80C8u); /* STRH r0,[r1,#6] */
        put_le16(half_flash + 14u, 0x885Au); /* LDRH r2,[r3,#2] */
        put_le16(half_flash + 16u, 0xBE00u); /* BKPT #0 */
        put_le16(half_ram + 0x12u, 0x1234u);
        py32_bus_init(&half_bus);
        assert(py32_bus_add_memory(&half_bus, "flash", 0x08000000u,
                                   half_flash, sizeof(half_flash), true));
        assert(py32_bus_add_memory(&half_bus, "sram", 0x20000000u,
                                   half_ram, sizeof(half_ram), false));
        cortex_m0_init(&half_cpu, &half_bus);
        assert(cortex_m0_reset(&half_cpu, 0x08000000u));
        half_cpu.r[0] = 0xABCDu;
        half_cpu.r[1] = 0x20000020u;
        half_cpu.r[2] = 0x20000000u;
        half_cpu.r[3] = 0x20000010u;
        half_cpu.r[4] = 0u;
        while (!half_cpu.stopped) cortex_m0_step(&half_cpu);
        assert(half_cpu.stop_reason == CORTEX_M0_STOP_BKPT);
        assert(half_cpu.r[5] == 0xABCDu && half_cpu.r[2] == 0x1234u);
        assert(half_ram[0] == 0xCDu && half_ram[1] == 0xABu);
        assert(half_ram[0x26u] == 0xCDu && half_ram[0x27u] == 0xABu);
    }

    /* SoC 装配必须同时提供 0 地址别名和 0x08000000 主 Flash。 */
    py32_soc_init(&soc);
    py32_firmware_image_init(&image);
    image.data = program;
    image.size = sizeof(program);
    image.load_address = chip->flash_base;
    trimmed_chip = *chip;
    trimmed_chip.name = "test-minimal";
    trimmed_chip.peripherals = PY32_PERIPHERAL_GPIOA;
    trimmed_chip.external_irq_count = 8u;
    py32_soc_init(&trimmed_soc);
    assert(py32_soc_configure(&trimmed_soc, &trimmed_chip, &image,
                              error, sizeof(error)));
    assert(py32_soc_reset(&trimmed_soc, error, sizeof(error)));
    assert(py32_bus_read(&trimmed_soc.bus, 0x50000000u, 4u, &value));
    assert(!py32_bus_read(&trimmed_soc.bus, 0x50000400u, 4u, &value));
    assert(!py32_bus_read(&trimmed_soc.bus, 0x40013800u, 4u, &value));
    assert(!py32_bus_read(&trimmed_soc.bus, 0x40012400u, 4u, &value));
    assert(!py32_bus_read(&trimmed_soc.bus, 0x40023000u, 4u, &value));
    assert(py32_bus_write(&trimmed_soc.bus, 0xE000E100u, 4u,
                          (1u << 3) | (1u << 12)));
    assert(py32_bus_read(&trimmed_soc.bus, 0xE000E100u, 4u, &value));
    assert(value == (1u << 3));
    py32_soc_set_gpio_input(&trimmed_soc, 1u, 2u, true, true);
    assert(py32_gpio_input_data(&trimmed_soc.gpiob) == 0u);
    py32_soc_destroy(&trimmed_soc);

    assert(py32_soc_configure(&soc, chip, &image, error, sizeof(error)));
    assert(py32_soc_reset(&soc, error, sizeof(error)));
    /* 隐藏 PLL 的 CR/PLLRDY 握手和 PLLCFGR 必须可读写。 */
    assert(py32_bus_read(&soc.bus, 0x40021000u, 4u, &value));
    assert((value & (1u << 10)) != 0u); /* 复位后 HSI 已就绪 */
    assert(py32_bus_write(&soc.bus, 0x40021000u, 4u, (1u << 8) | (1u << 24)));
    assert(py32_bus_read(&soc.bus, 0x40021000u, 4u, &value));
    assert((value & (1u << 25)) != 0u); /* PLLRDY */
    assert(py32_bus_write(&soc.bus, 0x40021000u, 4u, 1u << 8));
    assert(py32_bus_read(&soc.bus, 0x40021000u, 4u, &value));
    assert((value & (1u << 25)) == 0u);
    assert(py32_bus_write(&soc.bus, 0x4002100Cu, 4u, 0x5Au));
    assert(py32_bus_read(&soc.bus, 0x4002100Cu, 4u, &value));
    assert(value == 0x5Au);
    /* DBGMCU 是固件冻结定时器所需的普通可写窗口。 */
    assert(py32_bus_write(&soc.bus, 0x4001580Cu, 4u, 0x00000600u));
    assert(py32_bus_read(&soc.bus, 0x4001580Cu, 4u, &value));
    assert(value == 0x00000600u);
    assert(py32_bus_write(&soc.bus, 0x40021038u, 4u, 1u << 12));
    assert(py32_bus_write(&soc.bus, 0x40023008u, 4u, 1u));
    assert(py32_bus_write(&soc.bus, 0x40023000u, 4u, 0x12345678u));
    assert(py32_bus_read(&soc.bus, 0x40023000u, 4u, &value));
    assert(value == 0xDF8A8A2Bu);
    assert(py32_bus_write(&soc.bus, 0x40023004u, 1u, 0xA5u));
    assert(py32_bus_read(&soc.bus, 0x40023004u, 4u, &value));
    assert(value == 0xA5u);
    /* FLASH 必须先按官方双密钥序列解锁，页擦除和编程只能令位从 1 变 0。 */
    value = soc.flash[0x2000u];
    assert(py32_bus_write(&soc.bus, 0x08002000u, 4u, 0u));
    assert(soc.flash[0x2000u] == (uint8_t)value);
    assert(py32_bus_read(&soc.bus, 0x40022010u, 4u, &value));
    assert((value & (1u << 4)) != 0u);
    assert(py32_bus_write(&soc.bus, 0x40022008u, 4u, 0x45670123u));
    assert(py32_bus_write(&soc.bus, 0x40022008u, 4u, 0xCDEF89ABu));
    assert(py32_bus_read(&soc.bus, 0x40022014u, 4u, &value));
    assert((value & (1u << 31)) == 0u);
    assert(py32_bus_write(&soc.bus, 0x40022014u, 4u, 1u << 1));
    assert(py32_bus_write(&soc.bus, 0x08002000u, 4u, 0xFFu));
    assert(soc.flash[0x2000u] == 0xFFu &&
           soc.flash[0x207Fu] == 0xFFu);
    assert(py32_bus_write(&soc.bus, 0x40022014u, 4u, 1u));
    assert(py32_bus_write(&soc.bus, 0x08002000u, 4u, 0x12345678u));
    assert(py32_bus_read(&soc.bus, 0x08002000u, 4u, &value));
    assert(value == 0x12345678u);
    /* LPTIM1 使用独立低速时钟和预分频，在自动重装匹配时请求 IRQ17。 */
    assert(py32_bus_write(&soc.bus, 0x4002103Cu, 4u,
                          soc.rcc.apbenr1 | (1u << 31)));
    assert(py32_bus_write(&soc.bus, 0x40007C0Cu, 4u, 7u << 9));
    assert(py32_bus_write(&soc.bus, 0x40007C18u, 4u, 1u));
    assert(py32_bus_write(&soc.bus, 0x40007C08u, 4u, 1u << 1));
    assert(py32_bus_write(&soc.bus, 0x40007C10u, 4u, 3u));
    assert(!py32_lptim_tick(&soc.lptim1, 187499u, 24000000u, 32768u));
    assert(py32_lptim_tick(&soc.lptim1, 1u, 24000000u, 32768u));
    assert(soc.lptim1.match_count == 1u &&
           (soc.lptim1.isr & (1u << 1)) != 0u);
    assert(py32_bus_write(&soc.bus, 0x40007C04u, 4u, 1u << 1));
    assert(soc.lptim1.isr == 0u);
    /* COMP1 的 PA1 模拟输入跨越 VREFINT 时，经 EXTI17 请求共享 IRQ12。 */
    assert(py32_bus_write(&soc.bus, 0x40021040u, 4u,
                          soc.rcc.apbenr2 | (1u << 21)));
    assert(py32_bus_write(&soc.bus, 0x40021800u, 4u, 1u << 17));
    assert(py32_bus_write(&soc.bus, 0x40021804u, 4u, 1u << 17));
    assert(py32_bus_write(&soc.bus, 0x40021880u, 4u, 1u << 17));
    assert(py32_bus_write(&soc.bus, 0x40010200u, 4u,
                          1u | (3u << 4) | (2u << 8)));
    assert(!py32_comp_output(&soc.comp12, 0u));
    py32_soc_set_analog_input(&soc, 0u, 1u, 1800u);
    assert(py32_comp_output(&soc.comp12, 0u));
    assert((soc.exti.pr & (1u << 17)) != 0u);
    assert((py32_exti_irq_mask(&soc.exti) & (1u << 12)) != 0u);
    assert(py32_bus_write(&soc.bus, 0x4002180Cu, 4u, 1u << 17));
    py32_soc_set_analog_input(&soc, 0u, 1u, 500u);
    assert(!py32_comp_output(&soc.comp12, 0u));
    assert(py32_bus_write(&soc.bus, 0x4002180Cu, 4u, 1u << 17));
    assert(py32_bus_write(&soc.bus, 0x08002000u, 4u, 0xFFFFFFFFu));
    assert(py32_bus_read(&soc.bus, 0x08002000u, 4u, &value));
    assert(value == 0x12345678u);
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
                          (1u << 13) | (1u << 3) | (1u << 2) | (1u << 5)));
    assert(py32_bus_write(&soc.bus, 0x40013804u, 1, 0xA5u));
    assert(soc.usart1.tx_count == 1u && soc.usart1.tx[0] == 0xA5u);
    {
        const uint8_t input[] = {0x11u, 0x22u};
        uint32_t received;
        assert(py32_usart_receive(&soc.usart1, input, sizeof(input)) == 2u);
        assert(py32_usart_irq_pending(&soc.usart1)); /* RXNEIE -> IRQ27 */
        assert(py32_bus_read(&soc.bus, 0x40013800u, 4, &received));
        assert((received & (1u << 5)) != 0u);
        assert(py32_bus_read(&soc.bus, 0x40013804u, 1, &received));
        assert(received == 0x11u);
        assert(py32_usart_irq_pending(&soc.usart1));
        assert(py32_bus_read(&soc.bus, 0x40013804u, 1, &received));
        assert(received == 0x22u && !py32_usart_irq_pending(&soc.usart1));
    }
    assert(py32_bus_write(&soc.bus, 0x40021040u, 4,
                          (1u << 11) | (1u << 14) | (1u << 17)));
    assert(py32_bus_write(&soc.bus, 0x40012C28u, 4, 0u));
    assert(py32_bus_write(&soc.bus, 0x40012C2Cu, 4, 1u));
    assert(py32_bus_write(&soc.bus, 0x40012C3Cu, 4, 0x11u));
    assert(py32_bus_write(&soc.bus, 0x40012C40u, 4, 0x22u));
    assert(py32_bus_read(&soc.bus, 0x40012C3Cu, 4, &value));
    assert(value == 0x11u);
    assert(py32_bus_read(&soc.bus, 0x40012C40u, 4, &value));
    assert(value == 0x22u);
    soc.tim1.compare_callback = capture_compare;
    soc.tim1.compare_context = NULL;
    assert(py32_bus_write(&soc.bus, 0x40012C40u, 4, 0x5Au));
    assert(compare_count == 1u && compare_channel == 4u &&
           compare_value == 0x5Au);
    assert(py32_bus_write(&soc.bus, 0x40012C3Cu, 4, 0x33u));
    assert(compare_count == 2u && compare_channel == 3u &&
           compare_value == 0x33u);
    soc.tim1.compare_callback = NULL;
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
