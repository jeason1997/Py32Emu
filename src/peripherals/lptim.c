#include "py32emu/peripherals/lptim.h"

#include <string.h>

#define LPTIM_ARRM       (1u << 1)
#define LPTIM_ENABLE     (1u << 0)
#define LPTIM_SNGSTRT    (1u << 1)

static bool clock_enabled(const Py32Lptim *lptim)
{
    return lptim->clock_enable_register != 0 &&
        (*lptim->clock_enable_register & lptim->clock_enable_mask) != 0u;
}

void py32_lptim_reset(Py32Lptim *lptim, const uint32_t *clock_register,
                      uint32_t clock_mask)
{
    memset(lptim, 0, sizeof(*lptim));
    lptim->arr = 0xFFFFu;
    lptim->clock_enable_register = clock_register;
    lptim->clock_enable_mask = clock_mask;
}

bool py32_lptim_read(void *context, uint32_t offset, unsigned size,
                     uint32_t *value)
{
    Py32Lptim *lptim = context;
    if (!clock_enabled(lptim) || size != 4u || value == 0) return false;
    switch (offset) {
    case 0x00u: *value = lptim->isr; break;
    case 0x04u: *value = 0u; break;
    case 0x08u: *value = lptim->ier; break;
    case 0x0Cu: *value = lptim->cfgr; break;
    case 0x10u: *value = lptim->cr; break;
    case 0x18u: *value = lptim->arr; break;
    case 0x1Cu:
        *value = lptim->cnt;
        if ((lptim->cr & (1u << 4)) != 0u) lptim->cnt = 0u;
        break;
    default: return false;
    }
    return true;
}

bool py32_lptim_write(void *context, uint32_t offset, unsigned size,
                      uint32_t value)
{
    Py32Lptim *lptim = context;
    if (!clock_enabled(lptim) || size != 4u) return false;
    switch (offset) {
    case 0x04u: lptim->isr &= ~(value & LPTIM_ARRM); break;
    case 0x08u: lptim->ier = value & LPTIM_ARRM; break;
    case 0x0Cu: lptim->cfgr = value; break;
    case 0x10u:
        if ((value & LPTIM_ENABLE) == 0u) {
            lptim->cnt = 0u;
            lptim->clock_accumulator = 0u;
        }
        lptim->cr = value & 0x13u;
        break;
    case 0x18u: lptim->arr = value & 0xFFFFu; break;
    default: return false;
    }
    return true;
}

bool py32_lptim_tick(Py32Lptim *lptim, uint32_t cpu_cycles,
                     uint32_t cpu_clock_hz, uint32_t source_clock_hz)
{
    uint64_t denominator, ticks;
    uint32_t divider;
    bool matched = false;
    if (!clock_enabled(lptim) || (lptim->cr & 3u) != 3u ||
        cpu_clock_hz == 0u || source_clock_hz == 0u) return false;
    divider = 1u << ((lptim->cfgr >> 9) & 7u);
    denominator = (uint64_t)cpu_clock_hz * divider;
    lptim->clock_accumulator += (uint64_t)cpu_cycles * source_clock_hz;
    ticks = lptim->clock_accumulator / denominator;
    lptim->clock_accumulator %= denominator;
    while (ticks-- != 0u) {
        if (lptim->cnt >= lptim->arr) {
            lptim->cnt = 0u;
            lptim->isr |= LPTIM_ARRM;
            lptim->cr &= ~LPTIM_SNGSTRT;
            ++lptim->match_count;
            matched = true;
            break;
        }
        ++lptim->cnt;
    }
    return matched && (lptim->ier & LPTIM_ARRM) != 0u;
}
