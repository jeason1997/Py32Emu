#include "py32emu/peripherals/iwdg.h"

#include <string.h>

enum { LSI_HZ = 32768u };

void py32_iwdg_reset(Py32Iwdg *iwdg)
{
    memset(iwdg, 0, sizeof(*iwdg));
    iwdg->rlr = 0x0FFFu;
    iwdg->counter = iwdg->rlr;
}

bool py32_iwdg_read(void *context, uint32_t offset, unsigned size,
                    uint32_t *value)
{
    Py32Iwdg *iwdg = context;
    if (size != 4u || value == 0) return false;
    switch (offset) {
    case 0x00: *value = 0u; break;
    case 0x04: *value = iwdg->pr; break;
    case 0x08: *value = iwdg->rlr; break;
    case 0x0C: *value = 0u; break;
    default: return false;
    }
    return true;
}

bool py32_iwdg_write(void *context, uint32_t offset, unsigned size,
                     uint32_t value)
{
    Py32Iwdg *iwdg = context;
    if (size != 4u) return false;
    if (offset == 0x00u) {
        value &= 0xFFFFu;
        if (value == 0xCCCCu) iwdg->enabled = true;
        else if (value == 0x5555u) iwdg->write_access = true;
        else if (value == 0u) iwdg->write_access = false;
        else if (value == 0xAAAAu) iwdg->counter = iwdg->rlr;
        return true;
    }
    if (!iwdg->write_access) return true;
    if (offset == 0x04u) iwdg->pr = value & 7u;
    else if (offset == 0x08u) iwdg->rlr = value & 0x0FFFu;
    else return false;
    return true;
}

bool py32_iwdg_tick(Py32Iwdg *iwdg, unsigned cpu_cycles,
                    uint32_t cpu_clock_hz, bool lsi_ready)
{
    uint32_t divider;
    uint64_t threshold;
    if (!iwdg->enabled || !lsi_ready || cpu_clock_hz == 0u) return false;
    divider = iwdg->pr == 0u ? 4u : 1u << (iwdg->pr + 2u);
    threshold = (uint64_t)cpu_clock_hz * divider;
    iwdg->clock_accumulator += (uint64_t)cpu_cycles * LSI_HZ;
    while (iwdg->clock_accumulator >= threshold) {
        iwdg->clock_accumulator -= threshold;
        if (iwdg->counter == 0u) return true;
        --iwdg->counter;
    }
    return false;
}
