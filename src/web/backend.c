#include "py32emu/chips/chip.h"
#include "py32emu/chips/soc.h"
#include "py32emu/core/disassembler.h"
#include "py32emu/firmware/image.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BREAKPOINTS 64u

static Py32Soc soc;
static Py32FirmwareImage image;
static bool loaded;
static uint32_t breakpoints[MAX_BREAKPOINTS];
static size_t breakpoint_count;
static bool breakpoint_hit;
static uint32_t resume_breakpoint;
static bool resume_pending;

static char *field(char **cursor)
{
    char *value = *cursor;
    char *tab;
    if (value == NULL) return NULL;
    tab = strchr(value, '\t');
    if (tab != NULL) {
        *tab = '\0';
        *cursor = tab + 1;
    } else *cursor = NULL;
    return value;
}

static void json_string(const char *value)
{
    const unsigned char *p = (const unsigned char *)(value != NULL ? value : "");
    putchar('"');
    while (*p != '\0') {
        if (*p == '"' || *p == '\\') putchar('\\');
        if (*p >= 0x20u) putchar(*p);
        ++p;
    }
    putchar('"');
}

static void error_json(const char *message)
{
    fputs("{\"ok\":false,\"error\":", stdout);
    json_string(message);
    puts("}");
    fflush(stdout);
}

static bool is_breakpoint(uint32_t address)
{
    size_t i;
    for (i = 0; i < breakpoint_count; ++i)
        if (breakpoints[i] == address) return true;
    return false;
}

static void disassembly_json(void)
{
    uint32_t address = soc.cpu.r[CORTEX_M0_PC];
    unsigned rows = 0u;
    address = address >= 8u ? (address - 8u) & ~1u : address & ~1u;
    fputs(",\"disassembly\":[", stdout);
    while (rows < 12u) {
        uint32_t raw, next = 0u;
        char assembly[96];
        unsigned halfwords;
        const Py32FirmwareSymbol *symbol;
        if (!py32_bus_read(&soc.bus, address, 2u, &raw)) break;
        (void)py32_bus_read(&soc.bus, address + 2u, 2u, &next);
        halfwords = py32_thumb_disassemble(address, (uint16_t)raw,
                                            (uint16_t)next,
                                            assembly, sizeof(assembly));
        if (rows != 0u) putchar(',');
        printf("{\"address\":%u,\"opcode\":%u,\"text\":", address, raw);
        json_string(assembly);
        symbol = py32_firmware_find_symbol(&image, address);
        if (symbol != NULL) {
            fputs(",\"symbol\":", stdout);
            json_string(symbol->name);
            printf(",\"symbolOffset\":%u", address - symbol->address);
        }
        putchar('}');
        address += halfwords * 2u;
        ++rows;
    }
    putchar(']');
}

static void state_json(void)
{
    unsigned i;
    printf("{\"ok\":true,\"loaded\":%s", loaded ? "true" : "false");
    if (!loaded) {
        puts("}");
        fflush(stdout);
        return;
    }
    fputs(",\"chip\":", stdout);
    json_string(soc.description->name);
    printf(",\"cycles\":%" PRIu64 ",\"pc\":%u,\"xpsr\":%u"
           ",\"msp\":%u,\"psp\":%u,\"control\":%u"
           ",\"exception\":%u,\"stopped\":%s,\"stopReason\":",
           soc.cpu.cycles, soc.cpu.r[CORTEX_M0_PC], soc.cpu.xpsr,
           soc.cpu.msp, soc.cpu.psp, soc.cpu.control,
           soc.cpu.exception_number, soc.cpu.stopped ? "true" : "false");
    json_string(cortex_m0_stop_reason_name(soc.cpu.stop_reason));
    printf(",\"breakpointHit\":%s,\"registers\":[",
           breakpoint_hit ? "true" : "false");
    for (i = 0; i < 16u; ++i) {
        if (i != 0u) putchar(',');
        printf("%u", soc.cpu.r[i]);
    }
    printf("],\"gpio\":{\"A\":{\"moder\":%u,\"idr\":%u,\"odr\":%u},"
           "\"B\":{\"moder\":%u,\"idr\":%u,\"odr\":%u},"
           "\"F\":{\"moder\":%u,\"idr\":%u,\"odr\":%u}},"
           "\"usartTxCount\":%zu,\"usartTx\":[",
           soc.gpioa.moder, py32_gpio_input_data(&soc.gpioa), soc.gpioa.odr,
           soc.gpiob.moder, py32_gpio_input_data(&soc.gpiob), soc.gpiob.odr,
           soc.gpiof.moder, py32_gpio_input_data(&soc.gpiof), soc.gpiof.odr,
           soc.usart1.tx_count);
    for (i = 0; i < soc.usart1.tx_count; ++i) {
        if (i != 0u) putchar(',');
        printf("%u", soc.usart1.tx[i]);
    }
    fputs("],\"breakpoints\":[", stdout);
    for (i = 0; i < breakpoint_count; ++i) {
        if (i != 0u) putchar(',');
        printf("%u", breakpoints[i]);
    }
    putchar(']');
    disassembly_json();
    putchar('}');
    putchar('\n');
    fflush(stdout);
}

static bool load_firmware(const char *path, const char *chip_name)
{
    const Py32ChipDescription *chip = py32_chip_by_name(chip_name);
    size_t length;
    char error[192];
    bool ok;
    if (path == NULL || chip == NULL) {
        error_json("无效的固件路径或芯片型号");
        return false;
    }
    py32_firmware_image_free(&image);
    py32_firmware_image_init(&image);
    length = strlen(path);
    ok = length >= 4u && strcmp(path + length - 4u, ".elf") == 0
        ? py32_firmware_load_elf(&image, path, chip->flash_base,
                                 chip->flash_size, error, sizeof(error))
        : length >= 4u && strcmp(path + length - 4u, ".hex") == 0
            ? py32_firmware_load_hex(&image, path, chip->flash_base,
                                     chip->flash_size, error, sizeof(error))
            : py32_firmware_load_bin(&image, path, chip->flash_base,
                                     chip->flash_size, error, sizeof(error));
    if (!ok || !py32_soc_configure(&soc, chip, &image, error, sizeof(error)) ||
        !py32_soc_reset(&soc, error, sizeof(error))) {
        error_json(error);
        return false;
    }
    loaded = true;
    breakpoint_count = 0u;
    breakpoint_hit = false;
    resume_pending = false;
    return true;
}

static void run_steps(uint64_t count)
{
    uint64_t done = 0u;
    bool skip = resume_pending && soc.cpu.r[CORTEX_M0_PC] == resume_breakpoint;
    breakpoint_hit = false;
    resume_pending = false;
    while (!soc.cpu.stopped && done < count) {
        uint32_t pc = soc.cpu.r[CORTEX_M0_PC];
        if (!skip && is_breakpoint(pc)) {
            breakpoint_hit = true;
            resume_breakpoint = pc;
            resume_pending = true;
            break;
        }
        skip = false;
        py32_soc_step(&soc);
        ++done;
    }
}

static void set_breakpoints(char *list)
{
    breakpoint_count = 0u;
    while (list != NULL && *list != '\0' && breakpoint_count < MAX_BREAKPOINTS) {
        char *end;
        uint32_t address = (uint32_t)strtoul(list, &end, 0);
        if (end == list) break;
        breakpoints[breakpoint_count++] = address & ~1u;
        list = *end == ',' ? end + 1 : NULL;
    }
    breakpoint_hit = false;
    resume_pending = false;
}

static void memory_json(uint32_t address, unsigned count)
{
    unsigned i;
    if (count > 256u) count = 256u;
    printf("{\"ok\":true,\"address\":%u,\"data\":[", address);
    for (i = 0; i < count; ++i) {
        uint32_t value;
        if (!py32_bus_read(&soc.bus, address + i, 1u, &value)) {
            puts("],\"error\":\"地址不可读\"}");
            fflush(stdout);
            return;
        }
        if (i != 0u) putchar(',');
        printf("%u", value);
    }
    puts("]}");
    fflush(stdout);
}

static void receive_usart(char *values)
{
    uint8_t bytes[256];
    size_t count = 0u;
    while (values != NULL && *values != '\0' && count < sizeof(bytes)) {
        char *end;
        unsigned long value = strtoul(values, &end, 0);
        if (end == values || value > 255u) break;
        bytes[count++] = (uint8_t)value;
        values = *end == ',' ? end + 1 : NULL;
    }
    py32_usart_receive(&soc.usart1, bytes, count);
}

int main(void)
{
    char line[2048];
    py32_soc_init(&soc);
    py32_firmware_image_init(&image);
    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *cursor = line;
        char *command;
        line[strcspn(line, "\r\n")] = '\0';
        command = field(&cursor);
        if (strcmp(command, "load") == 0) {
            char *path = field(&cursor);
            char *chip = field(&cursor);
            if (load_firmware(path, chip != NULL ? chip : "py32f002ax5"))
                state_json();
        } else if (!loaded) error_json("尚未加载固件");
        else if (strcmp(command, "state") == 0) state_json();
        else if (strcmp(command, "reset") == 0) {
            char error[160];
            if (!py32_soc_reset(&soc, error, sizeof(error))) error_json(error);
            else state_json();
        } else if (strcmp(command, "step") == 0) {
            breakpoint_hit = false;
            resume_pending = false;
            if (!soc.cpu.stopped) py32_soc_step(&soc);
            state_json();
        } else if (strcmp(command, "run") == 0) {
            char *count = field(&cursor);
            run_steps(count != NULL ? strtoull(count, NULL, 0) : 10000u);
            state_json();
        } else if (strcmp(command, "breakpoints") == 0) {
            set_breakpoints(field(&cursor));
            state_json();
        } else if (strcmp(command, "memory") == 0) {
            char *address = field(&cursor);
            char *count = field(&cursor);
            memory_json((uint32_t)strtoul(address, NULL, 0),
                        count != NULL ? (unsigned)strtoul(count, NULL, 0) : 64u);
        } else if (strcmp(command, "gpio") == 0) {
            char *port = field(&cursor), *pin = field(&cursor);
            char *driven = field(&cursor), *high = field(&cursor);
            py32_soc_set_gpio_input(&soc,
                port != NULL ? (unsigned)strtoul(port, NULL, 0) : 0u,
                pin != NULL ? (unsigned)strtoul(pin, NULL, 0) : 0u,
                driven != NULL && strtoul(driven, NULL, 0) != 0u,
                high != NULL && strtoul(high, NULL, 0) != 0u);
            state_json();
        } else if (strcmp(command, "usart_rx") == 0) {
            receive_usart(field(&cursor));
            state_json();
        } else error_json("未知调试命令");
    }
    py32_soc_destroy(&soc);
    py32_firmware_image_free(&image);
    return 0;
}
