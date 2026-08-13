#include "py32emu/core/disassembler.h"

#include <stdbool.h>
#include <stdio.h>

static int32_t extend(uint32_t value, unsigned bits)
{
    uint32_t sign = 1u << (bits - 1u);
    return (int32_t)((value ^ sign) - sign);
}

unsigned py32_thumb_disassemble(uint32_t pc, uint16_t op, uint16_t next,
                                char *text, size_t size)
{
    static const char *conditions[14] = {
        "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
        "hi", "ls", "ge", "lt", "gt", "le"
    };
    static const char *alu[16] = {
        "ands", "eors", "lsls", "lsrs", "asrs", "adcs", "sbcs", "rors",
        "tst", "negs", "cmp", "cmn", "orrs", "muls", "bics", "mvns"
    };
    unsigned rd = op & 7u, rn = (op >> 3) & 7u, rm = (op >> 6) & 7u;
    if (text == NULL || size == 0u) return 1u;
    if ((op & 0xE000u) == 0u && (op & 0x1800u) != 0x1800u) {
        const char *name = ((op >> 11) & 3u) == 0u ? "lsls" :
            ((op >> 11) & 3u) == 1u ? "lsrs" : "asrs";
        snprintf(text, size, "%s r%u, r%u, #%u", name, rd, rn,
                 (op >> 6) & 31u);
    } else if ((op & 0xF800u) == 0x1800u) {
        snprintf(text, size, "%s r%u, r%u, %s%u",
                 (op & 0x0200u) ? "subs" : "adds", rd, rn,
                 (op & 0x0400u) ? "#" : "r", rm);
    } else if ((op & 0xE000u) == 0x2000u) {
        static const char *names[4] = {"movs", "cmp", "adds", "subs"};
        snprintf(text, size, "%s r%u, #%u", names[(op >> 11) & 3u],
                 (op >> 8) & 7u, op & 0xFFu);
    } else if ((op & 0xFC00u) == 0x4000u) {
        snprintf(text, size, "%s r%u, r%u", alu[(op >> 6) & 15u], rd, rn);
    } else if ((op & 0xFC00u) == 0x4400u) {
        static const char *names[4] = {"add", "cmp", "mov", "bx"};
        unsigned high_rd = rd | ((op >> 4) & 8u);
        unsigned high_rm = (op >> 3) & 15u;
        snprintf(text, size, "%s r%u, r%u", names[(op >> 8) & 3u],
                 high_rd, high_rm);
    } else if ((op & 0xF800u) == 0x4800u) {
        snprintf(text, size, "ldr r%u, [pc, #%u]", (op >> 8) & 7u,
                 (op & 0xFFu) << 2);
    } else if ((op & 0xF000u) == 0xD000u && ((op >> 8) & 15u) < 14u) {
        unsigned cond = (op >> 8) & 15u;
        uint32_t target = pc + 4u + (uint32_t)(extend(op & 0xFFu, 8) << 1);
        snprintf(text, size, "b%s 0x%08X", conditions[cond], target);
    } else if ((op & 0xFF00u) == 0xDF00u) {
        snprintf(text, size, "svc #%u", op & 0xFFu);
    } else if ((op & 0xFF00u) == 0xBE00u) {
        snprintf(text, size, "bkpt #%u", op & 0xFFu);
    } else if ((op & 0xFF0Fu) == 0xBF00u) {
        static const char *hints[5] = {"nop", "yield", "wfe", "wfi", "sev"};
        unsigned hint = (op >> 4) & 15u;
        snprintf(text, size, "%s", hint < 5u ? hints[hint] : "hint");
    } else if ((op & 0xF800u) == 0xE000u) {
        uint32_t target = pc + 4u + (uint32_t)(extend(op & 0x7FFu, 11) << 1);
        snprintf(text, size, "b 0x%08X", target);
    } else if ((op & 0xF800u) == 0xF000u && (next & 0xD000u) == 0xD000u) {
        unsigned s = (op >> 10) & 1u, j1 = (next >> 13) & 1u;
        unsigned j2 = (next >> 11) & 1u;
        unsigned i1 = !(j1 ^ s), i2 = !(j2 ^ s);
        uint32_t imm = (s << 24) | (i1 << 23) | (i2 << 22) |
            ((op & 0x03FFu) << 12) | ((next & 0x07FFu) << 1);
        snprintf(text, size, "bl 0x%08X", pc + 4u + (uint32_t)extend(imm, 25));
        return 2u;
    } else if ((op & 0xF000u) == 0x6000u) {
        bool load = (op & 0x0800u) != 0u, byte = (op & 0x1000u) != 0u;
        snprintf(text, size, "%s%s r%u, [r%u, #%u]", load ? "ldr" : "str",
                 byte ? "b" : "", rd, rn,
                 ((op >> 6) & 31u) << (byte ? 0u : 2u));
    } else if ((op & 0xFE00u) == 0xB400u || (op & 0xFE00u) == 0xBC00u) {
        snprintf(text, size, "%s {0x%03X}", (op & 0x0800u) ? "pop" : "push",
                 (op & 0x1FFu));
    } else {
        snprintf(text, size, ".hword 0x%04X", op);
    }
    return 1u;
}
