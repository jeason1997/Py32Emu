#include "py32emu/chips/chip.h"
#include "py32emu/chips/soc.h"
#include "py32emu/firmware/image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const Py32ChipDescription *chip = py32f002ax5_description();
    Py32FirmwareImage image;
    Py32Soc soc;
    uint64_t limit = 1000000u;
    uint64_t steps = 0;
    bool trace = false;
    bool pulse = false;
    unsigned pulse_port = 0u, pulse_pin = 0u;
    uint64_t pulse_at = 10000u;
    int i;
    char error[160];

    if (argc < 2) {
        fprintf(stderr, "用法: %s firmware [--chip 型号] [--steps N] [--trace]\n", argv[0]);
        return 2;
    }
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--trace") == 0) trace = true;
        else if (strcmp(argv[i], "--chip") == 0 && i + 1 < argc) {
            chip = py32_chip_by_name(argv[++i]);
            if (chip == NULL) {
                fprintf(stderr, "未知芯片型号: %s\n", argv[i]);
                return 2;
            }
        }
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            limit = strtoull(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--pulse") == 0 && i + 1 < argc) {
            const char *pin = argv[++i];
            char *end;
            if (strlen(pin) < 3u || pin[0] != 'P' ||
                (pin[1] != 'A' && pin[1] != 'B' && pin[1] != 'F')) {
                fprintf(stderr, "无效 GPIO 引脚: %s\n", pin);
                return 2;
            }
            pulse_port = pin[1] == 'A' ? 0u : pin[1] == 'B' ? 1u : 2u;
            pulse_pin = (unsigned)strtoul(pin + 2, &end, 10);
            if (*end != '\0' || pulse_pin >= 16u) {
                fprintf(stderr, "无效 GPIO 引脚: %s\n", pin);
                return 2;
            }
            pulse = true;
        }
        else if (strcmp(argv[i], "--pulse-at") == 0 && i + 1 < argc)
            pulse_at = strtoull(argv[++i], NULL, 0);
        else {
            fprintf(stderr, "未知参数: %s\n", argv[i]);
            return 2;
        }
    }
    py32_firmware_image_init(&image);
    py32_soc_init(&soc);
    {
        size_t path_length = strlen(argv[1]);
        bool is_hex = path_length >= 4u &&
            strcmp(argv[1] + path_length - 4u, ".hex") == 0;
        bool is_elf = path_length >= 4u &&
            strcmp(argv[1] + path_length - 4u, ".elf") == 0;
        bool loaded = is_elf
            ? py32_firmware_load_elf(&image, argv[1], chip->flash_base,
                                     chip->flash_size, error, sizeof(error))
            : is_hex
                ? py32_firmware_load_hex(&image, argv[1], chip->flash_base,
                                         chip->flash_size, error, sizeof(error))
                : py32_firmware_load_bin(&image, argv[1], chip->flash_base,
                                         chip->flash_size, error, sizeof(error));
        if (!loaded) {
        fprintf(stderr, "加载失败: %s\n", error);
        return 1;
        }
    }
    printf("已加载 %zu 字节到 %s Flash @ 0x%08X\n", image.size,
           chip->name, chip->flash_base);
    if (!py32_soc_configure(&soc, chip, &image, error, sizeof(error)) ||
        !py32_soc_reset(&soc, error, sizeof(error))) {
        fprintf(stderr, "启动失败: %s\n", error);
        py32_soc_destroy(&soc);
        py32_firmware_image_free(&image);
        return 1;
    }
    while (!soc.cpu.stopped && steps < limit) {
        if (pulse && steps == pulse_at)
            py32_soc_set_gpio_input(&soc, pulse_port, pulse_pin, true, false);
        CortexM0StepResult result = py32_soc_step(&soc);
        if (trace) {
            const Py32FirmwareSymbol *symbol =
                py32_firmware_find_symbol(&image, result.executed_pc);
            printf("%08X  %04X  r0=%08X sp=%08X",
                   result.executed_pc, result.instruction, soc.cpu.r[0],
                   soc.cpu.r[CORTEX_M0_SP]);
            if (symbol != NULL)
                printf("  <%s+0x%X>", symbol->name,
                       result.executed_pc - symbol->address);
            putchar('\n');
        }
        ++steps;
    }
    printf("执行 %llu 条，%llu 周期，PC=0x%08X，状态=%s\n",
           (unsigned long long)steps,
           (unsigned long long)soc.cpu.cycles,
           soc.cpu.r[CORTEX_M0_PC],
           soc.cpu.stopped ? cortex_m0_stop_reason_name(soc.cpu.stop_reason)
                           : "step-limit");
    printf("GPIOA: MODER=0x%08X ODR=0x%04X\n",
           soc.gpioa.moder, soc.gpioa.odr);
    if (soc.reset_count != 0u)
        printf("RESET: IWDG=%llu CSR=0x%08X\n",
               (unsigned long long)soc.reset_count, soc.rcc.csr);
    if (soc.flash_controller.erase_count != 0u ||
        soc.flash_controller.program_word_count != 0u) {
        uint32_t flash_word = 0u;
        py32_flash_memory_read(&soc.flash_controller,
                               0x2000u, 4u, &flash_word);
        printf("FLASH: erase=%llu program-words=%llu last=0x%08X "
               "@0x08002000=0x%08X\n",
               (unsigned long long)soc.flash_controller.erase_count,
               (unsigned long long)soc.flash_controller.program_word_count,
               soc.flash_controller.last_address, flash_word);
    }
    if ((soc.rcc.apbenr2 & (1u << 11)) != 0u)
        printf("TIM1: CNT=%u PSC=%u ARR=%u SR=0x%X\n",
               (unsigned)soc.tim1.reg[9], (unsigned)soc.tim1.reg[10],
               (unsigned)soc.tim1.reg[11], (unsigned)soc.tim1.reg[4]);
    if ((soc.rcc.apbenr2 & (1u << 20)) != 0u)
        printf("ADC1: DR=%u CHSELR=0x%X conversions=%llu\n",
               (unsigned)soc.adc1.dr, (unsigned)soc.adc1.chselr,
               (unsigned long long)soc.adc1.conversion_count);
    if (py32_chip_has_peripheral(chip, PY32_PERIPHERAL_LPTIM1) &&
        (soc.rcc.apbenr1 & (1u << 31)) != 0u)
        printf("LPTIM1: CNT=%u ARR=%u ISR=0x%X matches=%llu\n",
               (unsigned)soc.lptim1.cnt, (unsigned)soc.lptim1.arr,
               (unsigned)soc.lptim1.isr,
               (unsigned long long)soc.lptim1.match_count);
    if (py32_chip_has_peripheral(chip, PY32_PERIPHERAL_CRC) &&
        (soc.rcc.ahbenr & (1u << 12)) != 0u)
        printf("CRC: DR=0x%08X IDR=0x%02X\n",
               soc.crc.dr, soc.crc.idr);
    if (soc.usart1.tx_count > 0u) {
        size_t n;
        printf("USART1 TX (%zu):", soc.usart1.tx_count);
        for (n = 0; n < soc.usart1.tx_count; ++n)
            printf(" %02X", soc.usart1.tx[n]);
        putchar('\n');
    }
    if (soc.spi1.tx_count > 0u) {
        size_t n;
        printf("SPI1 TX (%zu):", soc.spi1.tx_count);
        for (n = 0; n < soc.spi1.tx_count; ++n)
            printf(" %02X", soc.spi1.tx[n]);
        putchar('\n');
    }
    if (soc.i2c1.tx_count > 0u) {
        printf("I2C1 TX (%zu):", soc.i2c1.tx_count);
        for (i = 0; i < (int)soc.i2c1.tx_count; ++i)
            printf(" %02X", soc.i2c1.tx[i]);
        putchar('\n');
    }
    if (soc.i2c1.rx_count > 0u) {
        printf("I2C1 RX (%zu):", soc.i2c1.rx_count);
        for (i = 0; i < (int)soc.i2c1.rx_count; ++i)
            printf(" %02X", soc.i2c1.rx[i]);
        putchar('\n');
    }
    py32_soc_destroy(&soc);
    py32_firmware_image_free(&image);
    return 0;
}
