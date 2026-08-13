#include "py32emu/peripherals/system.h"

#include <string.h>

enum { SYST_ENABLE = 1u, SYST_TICKINT = 2u, SYST_COUNTFLAG = 1u << 16 };

void py32_system_reset(Py32System *system, CortexM0 *cpu,
                       uint32_t clock_hz)
{
    memset(system, 0, sizeof(*system));
    system->cpu = cpu;
    /* TENMS 保存 10 ms 所需周期数，NOREF/SKEW 均为 0。 */
    system->syst_calib = clock_hz / 100u;
}

bool py32_system_read(void *context, uint32_t offset, unsigned size,
                      uint32_t *value)
{
    Py32System *s = context;
    if (size != 4u || (offset & 3u) != 0u) return false;
    switch (offset) {
    case 0x010: *value = s->syst_csr; s->syst_csr &= ~SYST_COUNTFLAG; break;
    case 0x014: *value = s->syst_rvr; break;
    case 0x018: *value = s->syst_cvr; break;
    case 0x01C: *value = s->syst_calib; break;
    case 0x100: *value = s->nvic_enable; break;
    case 0x180: *value = s->nvic_enable; break;
    case 0x200: *value = s->nvic_pending; break;
    case 0x280: *value = s->nvic_pending; break;
    case 0xD00: *value = 0x410CC601u; break; /* Cortex-M0+ CPUID */
    case 0xD04: *value = s->icsr; break;
    case 0xD08: *value = s->cpu->vector_table; break;
    case 0xD0C: *value = 0xFA050000u; break;
    case 0xD10: *value = s->scb_scr; break;
    case 0xD14: *value = s->scb_ccr; break;
    case 0xD1C: *value = s->scb_shp[0]; break;
    case 0xD20: *value = s->scb_shp[1]; break;
    case 0xD24: *value = s->scb_shcsr; break;
    case 0xD28: *value = 0; break;
    case 0xD2C: *value = 0; break;
    default:
        if (offset >= 0x400u && offset < 0x420u) {
            unsigned i = offset - 0x400u;
            *value = (uint32_t)s->nvic_priority[i] |
                     ((uint32_t)s->nvic_priority[i + 1] << 8) |
                     ((uint32_t)s->nvic_priority[i + 2] << 16) |
                     ((uint32_t)s->nvic_priority[i + 3] << 24);
            break;
        }
        return false;
    }
    return true;
}

bool py32_system_write(void *context, uint32_t offset, unsigned size,
                       uint32_t value)
{
    Py32System *s = context;
    if (size != 4u || (offset & 3u) != 0u) return false;
    switch (offset) {
    case 0x010: s->syst_csr = value & 7u; break;
    case 0x014: s->syst_rvr = value & 0x00FFFFFFu; break;
    case 0x018: s->syst_cvr = 0; s->syst_csr &= ~SYST_COUNTFLAG; break;
    case 0x100: s->nvic_enable |= value; break;
    case 0x180: s->nvic_enable &= ~value; break;
    case 0x200: s->nvic_pending |= value; break;
    case 0x280: s->nvic_pending &= ~value; break;
    case 0xD04:
        if (value & (1u << 25)) s->systick_pending = true;
        if (value & (1u << 27)) s->systick_pending = false;
        break;
    case 0xD08: s->cpu->vector_table = value & 0xFFFFFF80u; break;
    case 0xD0C: break; /* AIRCR 中暂不支持复位请求。 */
    case 0xD10: s->scb_scr = value; break;
    case 0xD14: s->scb_ccr = value; break;
    case 0xD1C: s->scb_shp[0] = value; break;
    case 0xD20: s->scb_shp[1] = value; break;
    case 0xD24: s->scb_shcsr = value; break;
    default:
        if (offset >= 0x400u && offset < 0x420u) {
            unsigned i = offset - 0x400u;
            s->nvic_priority[i] = (uint8_t)value;
            s->nvic_priority[i + 1] = (uint8_t)(value >> 8);
            s->nvic_priority[i + 2] = (uint8_t)(value >> 16);
            s->nvic_priority[i + 3] = (uint8_t)(value >> 24);
            break;
        }
        return false;
    }
    return true;
}

void py32_system_tick(Py32System *s, unsigned cpu_cycles)
{
    while (cpu_cycles-- > 0u && (s->syst_csr & SYST_ENABLE) != 0u) {
        if (s->syst_cvr == 0u) {
            s->syst_cvr = s->syst_rvr;
            s->syst_csr |= SYST_COUNTFLAG;
            if (s->syst_csr & SYST_TICKINT) s->systick_pending = true;
        } else --s->syst_cvr;
    }
}

bool py32_system_service_exception(Py32System *s)
{
    unsigned irq;
    if (s->cpu->exception_number != 0u || s->cpu->primask) return false;
    if (s->systick_pending) {
        s->systick_pending = false;
        return cortex_m0_enter_exception(s->cpu, 15u);
    }
    for (irq = 0; irq < 32u; ++irq) {
        uint32_t mask = 1u << irq;
        if ((s->nvic_enable & s->nvic_pending & mask) != 0u) {
            s->nvic_pending &= ~mask;
            return cortex_m0_enter_exception(s->cpu, 16u + irq);
        }
    }
    return false;
}
