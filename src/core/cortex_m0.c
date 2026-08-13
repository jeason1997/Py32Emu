#include "py32emu/core/cortex_m0.h"

#include <string.h>

#define BIT(v, n) (((v) >> (n)) & 1u)
#define FLAG(n) (1u << (n))

static void set_flag(CortexM0 *cpu, unsigned bit, bool value)
{
    if (value) cpu->xpsr |= FLAG(bit);
    else cpu->xpsr &= ~FLAG(bit);
}

static void set_nz(CortexM0 *cpu, uint32_t value)
{
    set_flag(cpu, CORTEX_M0_XPSR_N, BIT(value, 31) != 0);
    set_flag(cpu, CORTEX_M0_XPSR_Z, value == 0);
}

static uint32_t add_flags(CortexM0 *cpu, uint32_t a, uint32_t b,
                          unsigned carry)
{
    uint64_t wide = (uint64_t)a + b + carry;
    uint32_t result = (uint32_t)wide;
    set_nz(cpu, result);
    set_flag(cpu, CORTEX_M0_XPSR_C, (wide >> 32) != 0);
    set_flag(cpu, CORTEX_M0_XPSR_V,
             ((~(a ^ b) & (a ^ result)) >> 31) != 0);
    return result;
}

static uint32_t sub_flags(CortexM0 *cpu, uint32_t a, uint32_t b,
                          unsigned borrow)
{
    uint64_t subtrahend = (uint64_t)b + borrow;
    uint32_t result = a - (uint32_t)subtrahend;
    set_nz(cpu, result);
    set_flag(cpu, CORTEX_M0_XPSR_C, (uint64_t)a >= subtrahend);
    set_flag(cpu, CORTEX_M0_XPSR_V,
             (((a ^ b) & (a ^ result)) >> 31) != 0);
    return result;
}

static int32_t sign_extend(uint32_t value, unsigned bits)
{
    uint32_t sign = 1u << (bits - 1u);
    return (int32_t)((value ^ sign) - sign);
}

static bool load(CortexM0 *cpu, uint32_t address, unsigned size,
                 uint32_t *value)
{
    if (py32_bus_read(cpu->bus, address, size, value)) return true;
    cpu->stopped = true;
    cpu->stop_reason = CORTEX_M0_STOP_BUS_FAULT;
    return false;
}

static bool store(CortexM0 *cpu, uint32_t address, unsigned size,
                  uint32_t value)
{
    if (py32_bus_write(cpu->bus, address, size, value)) return true;
    cpu->stopped = true;
    cpu->stop_reason = CORTEX_M0_STOP_BUS_FAULT;
    return false;
}

void cortex_m0_init(CortexM0 *cpu, Py32Bus *bus)
{
    memset(cpu, 0, sizeof(*cpu));
    cpu->bus = bus;
    cpu->xpsr = FLAG(CORTEX_M0_XPSR_T);
    cpu->vector_table = 0;
}

bool cortex_m0_reset(CortexM0 *cpu, uint32_t vector_base)
{
    Py32Bus *bus = cpu->bus;
    uint32_t sp;
    uint32_t pc;

    cortex_m0_init(cpu, bus);
    if (!load(cpu, vector_base, 4, &sp) ||
        !load(cpu, vector_base + 4u, 4, &pc)) return false;
    /* ARMv6-M 向量中的入口必须声明 Thumb 状态，实际 PC 始终半字对齐。 */
    if ((pc & 1u) == 0u || (sp & 3u) != 0u) {
        cpu->stopped = true;
        cpu->stop_reason = CORTEX_M0_STOP_UNDEFINED;
        return false;
    }
    cpu->msp = sp;
    cpu->r[CORTEX_M0_SP] = sp;
    cpu->r[CORTEX_M0_PC] = pc & ~1u;
    cpu->stopped = false;
    cpu->stop_reason = CORTEX_M0_STOP_NONE;
    return true;
}

bool cortex_m0_enter_exception(CortexM0 *cpu, unsigned exception_number)
{
    uint32_t sp, handler;
    uint32_t frame[8];
    unsigned i;
    if (cpu == NULL || cpu->stopped || exception_number == 0u) return false;
    sp = cpu->r[CORTEX_M0_SP] - 32u;
    frame[0] = cpu->r[0]; frame[1] = cpu->r[1];
    frame[2] = cpu->r[2]; frame[3] = cpu->r[3];
    frame[4] = cpu->r[12]; frame[5] = cpu->r[CORTEX_M0_LR];
    frame[6] = cpu->r[CORTEX_M0_PC]; frame[7] = cpu->xpsr;
    for (i = 0; i < 8u; ++i)
        if (!store(cpu, sp + i * 4u, 4, frame[i])) return false;
    if (!load(cpu, cpu->vector_table + exception_number * 4u, 4, &handler))
        return false;
    if ((handler & 1u) == 0u) {
        cpu->stopped = true; cpu->stop_reason = CORTEX_M0_STOP_UNDEFINED;
        return false;
    }
    cpu->r[CORTEX_M0_SP] = sp;
    cpu->msp = sp;
    cpu->r[CORTEX_M0_LR] = 0xFFFFFFF9u;
    cpu->r[CORTEX_M0_PC] = handler & ~1u;
    cpu->exception_number = exception_number;
    cpu->xpsr = (cpu->xpsr & 0xF0000000u) |
                FLAG(CORTEX_M0_XPSR_T) | exception_number;
    return true;
}

static bool exception_return(CortexM0 *cpu, uint32_t exc_return)
{
    uint32_t sp = cpu->r[CORTEX_M0_SP];
    uint32_t frame[8];
    unsigned i;
    if (exc_return != 0xFFFFFFF9u) return false;
    for (i = 0; i < 8u; ++i)
        if (!load(cpu, sp + i * 4u, 4, &frame[i])) return true;
    cpu->r[0] = frame[0]; cpu->r[1] = frame[1];
    cpu->r[2] = frame[2]; cpu->r[3] = frame[3];
    cpu->r[12] = frame[4]; cpu->r[CORTEX_M0_LR] = frame[5];
    cpu->r[CORTEX_M0_PC] = frame[6] & ~1u;
    cpu->xpsr = frame[7] | FLAG(CORTEX_M0_XPSR_T);
    cpu->r[CORTEX_M0_SP] = sp + 32u;
    cpu->msp = cpu->r[CORTEX_M0_SP];
    cpu->exception_number = 0;
    return true;
}

static bool condition_passed(const CortexM0 *cpu, unsigned condition)
{
    bool n = BIT(cpu->xpsr, CORTEX_M0_XPSR_N) != 0;
    bool z = BIT(cpu->xpsr, CORTEX_M0_XPSR_Z) != 0;
    bool c = BIT(cpu->xpsr, CORTEX_M0_XPSR_C) != 0;
    bool v = BIT(cpu->xpsr, CORTEX_M0_XPSR_V) != 0;
    bool result;

    switch (condition >> 1) {
    case 0: result = z; break;
    case 1: result = c; break;
    case 2: result = n; break;
    case 3: result = v; break;
    case 4: result = c && !z; break;
    case 5: result = n == v; break;
    case 6: result = (n == v) && !z; break;
    default: result = true; break;
    }
    return (condition & 1u) != 0u ? !result : result;
}

static uint32_t shift_with_flags(CortexM0 *cpu, uint32_t value,
                                 unsigned type, unsigned amount,
                                 bool register_form)
{
    uint32_t result = value;

    if (register_form && amount == 0u) return value;
    if (type == 0u) { /* LSL */
        if (amount < 32u) {
            if (amount != 0u)
                set_flag(cpu, CORTEX_M0_XPSR_C, BIT(value, 32u - amount));
            result = value << amount;
        } else {
            set_flag(cpu, CORTEX_M0_XPSR_C,
                     amount == 32u && (value & 1u) != 0u);
            result = 0;
        }
    } else if (type == 1u) { /* LSR */
        if (!register_form && amount == 0u) amount = 32u;
        if (amount < 32u) {
            set_flag(cpu, CORTEX_M0_XPSR_C, BIT(value, amount - 1u));
            result = value >> amount;
        } else {
            set_flag(cpu, CORTEX_M0_XPSR_C,
                     amount == 32u && BIT(value, 31) != 0u);
            result = 0;
        }
    } else { /* ASR */
        if (!register_form && amount == 0u) amount = 32u;
        if (amount < 32u) {
            set_flag(cpu, CORTEX_M0_XPSR_C, BIT(value, amount - 1u));
            result = (uint32_t)((int32_t)value >> amount);
        } else {
            set_flag(cpu, CORTEX_M0_XPSR_C, BIT(value, 31) != 0u);
            result = BIT(value, 31) != 0u ? UINT32_MAX : 0;
        }
    }
    set_nz(cpu, result);
    return result;
}

static void branch_exchange(CortexM0 *cpu, uint32_t address)
{
    if ((address & 0xFFFFFFF0u) == 0xFFFFFFF0u &&
        cpu->exception_number != 0u) {
        if (!exception_return(cpu, address)) {
            cpu->stopped = true;
            cpu->stop_reason = CORTEX_M0_STOP_UNDEFINED;
        }
        return;
    }
    if ((address & 1u) == 0u) {
        cpu->stopped = true;
        cpu->stop_reason = CORTEX_M0_STOP_UNDEFINED;
    } else {
        cpu->r[CORTEX_M0_PC] = address & ~1u;
    }
}

CortexM0StepResult cortex_m0_step(CortexM0 *cpu)
{
    CortexM0StepResult out = {0};
    uint32_t pc;
    uint32_t raw;
    uint16_t op;
    unsigned rd, rn, rm;
    uint32_t a, b, value, address;

    if (cpu == NULL || cpu->stopped) {
        if (cpu != NULL) out.stop_reason = cpu->stop_reason;
        return out;
    }
    pc = cpu->r[CORTEX_M0_PC];
    if (!load(cpu, pc, 2, &raw)) goto done;
    op = (uint16_t)raw;
    cpu->last_pc = pc;
    cpu->last_instruction = op;
    cpu->r[CORTEX_M0_PC] = pc + 2u;
    out.executed_pc = pc;
    out.instruction = op;
    out.halfwords = 1;
    out.cycles = 1;

    if ((op & 0xE000u) == 0x0000u && (op & 0x1800u) != 0x1800u) {
        /* LSL/LSR/ASR (immediate). */
        rd = op & 7u; rm = (op >> 3) & 7u;
        cpu->r[rd] = shift_with_flags(cpu, cpu->r[rm], (op >> 11) & 3u,
                                      (op >> 6) & 31u, false);
    } else if ((op & 0xF800u) == 0x1800u) {
        rd = op & 7u; rn = (op >> 3) & 7u;
        b = BIT(op, 10) ? ((op >> 6) & 7u) : cpu->r[(op >> 6) & 7u];
        cpu->r[rd] = BIT(op, 9) ? sub_flags(cpu, cpu->r[rn], b, 0)
                                : add_flags(cpu, cpu->r[rn], b, 0);
    } else if ((op & 0xE000u) == 0x2000u) {
        rd = (op >> 8) & 7u; value = op & 0xFFu;
        switch ((op >> 11) & 3u) {
        case 0: cpu->r[rd] = value; set_nz(cpu, value); break; /* MOVS */
        case 1: (void)sub_flags(cpu, cpu->r[rd], value, 0); break;
        case 2: cpu->r[rd] = add_flags(cpu, cpu->r[rd], value, 0); break;
        default: cpu->r[rd] = sub_flags(cpu, cpu->r[rd], value, 0); break;
        }
    } else if ((op & 0xFC00u) == 0x4000u) {
        rd = op & 7u; rm = (op >> 3) & 7u; a = cpu->r[rd]; b = cpu->r[rm];
        switch ((op >> 6) & 15u) {
        case 0: cpu->r[rd] = a & b; set_nz(cpu, cpu->r[rd]); break;
        case 1: cpu->r[rd] = a ^ b; set_nz(cpu, cpu->r[rd]); break;
        case 2: cpu->r[rd] = shift_with_flags(cpu, a, 0, b & 0xFFu, true); break;
        case 3: cpu->r[rd] = shift_with_flags(cpu, a, 1, b & 0xFFu, true); break;
        case 4: cpu->r[rd] = shift_with_flags(cpu, a, 2, b & 0xFFu, true); break;
        case 5: cpu->r[rd] = add_flags(cpu, a, b, BIT(cpu->xpsr, CORTEX_M0_XPSR_C)); break;
        case 6: cpu->r[rd] = sub_flags(cpu, a, b, !BIT(cpu->xpsr, CORTEX_M0_XPSR_C)); break;
        case 8: set_nz(cpu, a & b); break; /* TST */
        case 9: cpu->r[rd] = sub_flags(cpu, 0, b, 0); break;
        case 10: (void)sub_flags(cpu, a, b, 0); break;
        case 11: (void)add_flags(cpu, a, b, 0); break;
        case 12: cpu->r[rd] = a | b; set_nz(cpu, cpu->r[rd]); break;
        case 13: cpu->r[rd] = a * b; set_nz(cpu, cpu->r[rd]); out.cycles = 32; break;
        case 14: cpu->r[rd] = a & ~b; set_nz(cpu, cpu->r[rd]); break;
        case 15: cpu->r[rd] = ~b; set_nz(cpu, cpu->r[rd]); break;
        default: cpu->stopped = true; cpu->stop_reason = CORTEX_M0_STOP_UNDEFINED; break;
        }
    } else if ((op & 0xFC00u) == 0x4400u) {
        rd = (op & 7u) | ((op >> 4) & 8u); rm = (op >> 3) & 15u;
        if (((op >> 8) & 3u) == 0u) cpu->r[rd] += cpu->r[rm];
        else if (((op >> 8) & 3u) == 1u) (void)sub_flags(cpu, cpu->r[rd], cpu->r[rm], 0);
        else if (((op >> 8) & 3u) == 2u) cpu->r[rd] = cpu->r[rm];
        else {
            value = cpu->r[rm];
            if (BIT(op, 7)) cpu->r[CORTEX_M0_LR] = (pc + 2u) | 1u;
            branch_exchange(cpu, value);
        }
        if (rd == CORTEX_M0_PC && ((op >> 8) & 3u) != 1u)
            cpu->r[rd] &= ~1u;
    } else if ((op & 0xF800u) == 0x4800u) {
        rd = (op >> 8) & 7u;
        address = ((pc + 4u) & ~3u) + ((op & 0xFFu) << 2);
        if (load(cpu, address, 4, &value)) cpu->r[rd] = value;
    } else if ((op & 0xF200u) == 0x5000u) {
        rm = (op >> 6) & 7u; rn = (op >> 3) & 7u; rd = op & 7u;
        address = cpu->r[rn] + cpu->r[rm];
        switch ((op >> 9) & 7u) {
        case 0: store(cpu, address, 4, cpu->r[rd]); break;
        case 1: store(cpu, address, 2, cpu->r[rd]); break;
        case 2: store(cpu, address, 1, cpu->r[rd]); break;
        case 3: if (load(cpu, address, 1, &value)) cpu->r[rd] = (uint32_t)sign_extend(value, 8); break;
        case 4: if (load(cpu, address, 4, &value)) cpu->r[rd] = value; break;
        case 5: if (load(cpu, address, 2, &value)) cpu->r[rd] = value; break;
        case 6: if (load(cpu, address, 1, &value)) cpu->r[rd] = value; break;
        case 7: if (load(cpu, address, 2, &value)) cpu->r[rd] = (uint32_t)sign_extend(value, 16); break;
        }
    } else if ((op & 0xE000u) == 0x6000u) {
        unsigned byte = BIT(op, 12); unsigned load_op = BIT(op, 11);
        rd = op & 7u; rn = (op >> 3) & 7u;
        address = cpu->r[rn] + (((op >> 6) & 31u) << (byte ? 0u : 2u));
        if (load_op) { if (load(cpu, address, byte ? 1u : 4u, &value)) cpu->r[rd] = value; }
        else store(cpu, address, byte ? 1u : 4u, cpu->r[rd]);
    } else if ((op & 0xF000u) == 0x8000u) {
        rd = op & 7u; rn = (op >> 3) & 7u;
        address = cpu->r[rn] + (((op >> 6) & 31u) << 1);
        if (BIT(op, 11)) { if (load(cpu, address, 2, &value)) cpu->r[rd] = value; }
        else store(cpu, address, 2, cpu->r[rd]);
    } else if ((op & 0xF000u) == 0x9000u) {
        rd = (op >> 8) & 7u; address = cpu->r[CORTEX_M0_SP] + ((op & 0xFFu) << 2);
        if (BIT(op, 11)) { if (load(cpu, address, 4, &value)) cpu->r[rd] = value; }
        else store(cpu, address, 4, cpu->r[rd]);
    } else if ((op & 0xF000u) == 0xA000u) {
        rd = (op >> 8) & 7u;
        a = BIT(op, 11) ? cpu->r[CORTEX_M0_SP] : ((pc + 4u) & ~3u);
        cpu->r[rd] = a + ((op & 0xFFu) << 2);
    } else if ((op & 0xFF00u) == 0xB000u) {
        value = (op & 0x7Fu) << 2;
        cpu->r[CORTEX_M0_SP] += BIT(op, 7) ? (uint32_t)-value : value;
        cpu->msp = cpu->r[CORTEX_M0_SP];
    } else if ((op & 0xFF00u) == 0xB200u) {
        rd = op & 7u; rm = (op >> 3) & 7u;
        switch ((op >> 6) & 3u) {
        case 0: cpu->r[rd] = (uint32_t)(int32_t)(int16_t)cpu->r[rm]; break;
        case 1: cpu->r[rd] = (uint32_t)(int32_t)(int8_t)cpu->r[rm]; break;
        case 2: cpu->r[rd] = cpu->r[rm] & 0xFFFFu; break;
        default: cpu->r[rd] = cpu->r[rm] & 0xFFu; break;
        }
    } else if ((op & 0xFF00u) == 0xBA00u) {
        rd = op & 7u; rm = (op >> 3) & 7u; value = cpu->r[rm];
        switch ((op >> 6) & 3u) {
        case 0:
            cpu->r[rd] = ((value & 0x000000FFu) << 24) |
                         ((value & 0x0000FF00u) << 8) |
                         ((value & 0x00FF0000u) >> 8) |
                         ((value & 0xFF000000u) >> 24);
            break;
        case 1:
            cpu->r[rd] = ((value & 0x00FF00FFu) << 8) |
                         ((value & 0xFF00FF00u) >> 8);
            break;
        case 3:
            value = ((value & 0xFFu) << 8) | ((value >> 8) & 0xFFu);
            cpu->r[rd] = (uint32_t)(int32_t)(int16_t)value;
            break;
        default:
            cpu->stopped = true;
            cpu->stop_reason = CORTEX_M0_STOP_UNDEFINED;
            break;
        }
    } else if ((op & 0xFE00u) == 0xB400u) {
        unsigned mask = op & 0xFFu;
        if (BIT(op, 8)) mask |= 1u << CORTEX_M0_LR;
        for (rn = 16u; rn-- > 0u;) if (mask & (1u << rn)) {
            cpu->r[CORTEX_M0_SP] -= 4u;
            if (!store(cpu, cpu->r[CORTEX_M0_SP], 4, cpu->r[rn])) break;
        }
        cpu->msp = cpu->r[CORTEX_M0_SP];
    } else if ((op & 0xFE00u) == 0xBC00u) {
        unsigned mask = op & 0xFFu;
        for (rn = 0; rn < 8u; ++rn) if (mask & (1u << rn)) {
            if (!load(cpu, cpu->r[CORTEX_M0_SP], 4, &cpu->r[rn])) break;
            cpu->r[CORTEX_M0_SP] += 4u;
        }
        if (!cpu->stopped && BIT(op, 8)) {
            if (load(cpu, cpu->r[CORTEX_M0_SP], 4, &value)) {
                cpu->r[CORTEX_M0_SP] += 4u; branch_exchange(cpu, value);
            }
        }
        cpu->msp = cpu->r[CORTEX_M0_SP];
    } else if ((op & 0xF000u) == 0xC000u) {
        rn = (op >> 8) & 7u; address = cpu->r[rn];
        for (rd = 0; rd < 8u; ++rd) if (op & (1u << rd)) {
            if (BIT(op, 11)) { if (!load(cpu, address, 4, &cpu->r[rd])) break; }
            else if (!store(cpu, address, 4, cpu->r[rd])) break;
            address += 4u;
        }
        cpu->r[rn] = address;
    } else if ((op & 0xF000u) == 0xD000u && (op & 0x0F00u) < 0x0E00u) {
        if (condition_passed(cpu, (op >> 8) & 15u))
            cpu->r[CORTEX_M0_PC] = pc + 4u + (uint32_t)(sign_extend(op & 0xFFu, 8) << 1);
    } else if ((op & 0xFF00u) == 0xBE00u) {
        cpu->stopped = true; cpu->stop_reason = CORTEX_M0_STOP_BKPT;
    } else if ((op & 0xFF00u) == 0xBF00u) {
        if ((op & 0x00FFu) == 0x30u) cpu->stopped = true, cpu->stop_reason = CORTEX_M0_STOP_WFI;
        else if ((op & 0x000Fu) != 0u) cpu->stopped = true, cpu->stop_reason = CORTEX_M0_STOP_UNDEFINED;
    } else if ((op & 0xF800u) == 0xE000u) {
        cpu->r[CORTEX_M0_PC] = pc + 4u + (uint32_t)(sign_extend(op & 0x7FFu, 11) << 1);
    } else if ((op & 0xF800u) == 0xF000u) {
        uint32_t second_raw;
        if (!load(cpu, pc + 2u, 2, &second_raw)) goto done;
        if (((uint16_t)second_raw & 0xD000u) != 0xD000u) {
            cpu->stopped = true; cpu->stop_reason = CORTEX_M0_STOP_UNDEFINED;
        } else {
            uint16_t op2 = (uint16_t)second_raw;
            unsigned s = BIT(op, 10), j1 = BIT(op2, 13), j2 = BIT(op2, 11);
            unsigned i1 = !(j1 ^ s), i2 = !(j2 ^ s);
            uint32_t imm25 = (s << 24) | (i1 << 23) | (i2 << 22) |
                             ((op & 0x03FFu) << 12) | ((op2 & 0x07FFu) << 1);
            cpu->r[CORTEX_M0_LR] = pc + 5u;
            cpu->r[CORTEX_M0_PC] = pc + 4u + (uint32_t)sign_extend(imm25, 25);
            out.halfwords = 2; out.cycles = 3;
        }
    } else {
        cpu->stopped = true;
        cpu->stop_reason = CORTEX_M0_STOP_UNDEFINED;
    }

done:
    cpu->cycles += out.cycles;
    out.stop_reason = cpu->stop_reason;
    return out;
}

const char *cortex_m0_stop_reason_name(CortexM0StopReason reason)
{
    switch (reason) {
    case CORTEX_M0_STOP_NONE: return "running";
    case CORTEX_M0_STOP_BKPT: return "breakpoint";
    case CORTEX_M0_STOP_WFI: return "wait-for-interrupt";
    case CORTEX_M0_STOP_BUS_FAULT: return "bus-fault";
    case CORTEX_M0_STOP_UNDEFINED: return "undefined-instruction";
    }
    return "unknown";
}
