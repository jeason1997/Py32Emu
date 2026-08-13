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
    int i;
    char error[160];

    if (argc < 2) {
        fprintf(stderr, "用法: %s firmware.bin [--steps N] [--trace]\n", argv[0]);
        return 2;
    }
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--trace") == 0) trace = true;
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            limit = strtoull(argv[++i], NULL, 0);
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
    if (soc.usart1.tx_count > 0u) {
        size_t n;
        printf("USART1 TX (%zu):", soc.usart1.tx_count);
        for (n = 0; n < soc.usart1.tx_count; ++n)
            printf(" %02X", soc.usart1.tx[n]);
        putchar('\n');
    }
    py32_soc_destroy(&soc);
    py32_firmware_image_free(&image);
    return 0;
}
