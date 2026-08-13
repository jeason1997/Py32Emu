#include "py32emu/peripherals/exti.h"

#include <string.h>

#define EXTI_LINES UINT32_C(0x7FFFF)

void py32_exti_reset(Py32Exti *exti)
{
    memset(exti, 0, sizeof(*exti));
}

bool py32_exti_read(void *context, uint32_t offset, unsigned size,
                    uint32_t *value)
{
    Py32Exti *exti = context;
    if (size != 4u || value == NULL || (offset & 3u) != 0u) return false;
    switch (offset) {
    case 0x00: *value = exti->rtsr; break;
    case 0x04: *value = exti->ftsr; break;
    case 0x08: *value = exti->swier; break;
    case 0x0C: *value = exti->pr; break;
    case 0x60: case 0x64: case 0x68:
        *value = exti->exticr[(offset - 0x60u) / 4u]; break;
    case 0x80: *value = exti->imr; break;
    case 0x84: *value = exti->emr; break;
    default: *value = 0u; break; /* documented reserved words read as zero */
    }
    return true;
}

bool py32_exti_write(void *context, uint32_t offset, unsigned size,
                     uint32_t value)
{
    Py32Exti *exti = context;
    if (size != 4u || (offset & 3u) != 0u) return false;
    switch (offset) {
    case 0x00: exti->rtsr = value & EXTI_LINES; break;
    case 0x04: exti->ftsr = value & EXTI_LINES; break;
    case 0x08:
        exti->swier = value & EXTI_LINES;
        exti->pr |= exti->swier;
        break;
    case 0x0C: exti->pr &= ~value; break;
    case 0x60: case 0x64: case 0x68:
        exti->exticr[(offset - 0x60u) / 4u] = value; break;
    case 0x80: exti->imr = value & EXTI_LINES; break;
    case 0x84: exti->emr = value & EXTI_LINES; break;
    default: break;
    }
    return true;
}

void py32_exti_input(Py32Exti *exti, unsigned port, unsigned pin,
                     bool old_high, bool new_high)
{
    uint32_t line, selection;
    if (exti == NULL || pin >= 12u || old_high == new_high) return;
    line = 1u << pin;
    selection = (exti->exticr[pin / 4u] >> (8u * (pin & 3u))) & 0xFu;
    if (selection != port) return;
    if ((!old_high && new_high && (exti->rtsr & line) != 0u) ||
        (old_high && !new_high && (exti->ftsr & line) != 0u))
        exti->pr |= line;
}

void py32_exti_signal_line(Py32Exti *exti, unsigned line_number,
                           bool old_high, bool new_high)
{
    uint32_t line;
    if (exti == NULL || line_number >= 19u || old_high == new_high) return;
    line = 1u << line_number;
    if ((!old_high && new_high && (exti->rtsr & line) != 0u) ||
        (old_high && !new_high && (exti->ftsr & line) != 0u))
        exti->pr |= line;
}

uint32_t py32_exti_irq_mask(const Py32Exti *exti)
{
    uint32_t pending;
    uint32_t irqs = 0u;
    if (exti == NULL) return 0u;
    pending = exti->pr & exti->imr;
    if ((pending & 0x0003u) != 0u) irqs |= 1u << 5;
    if ((pending & 0x000Cu) != 0u) irqs |= 1u << 6;
    if ((pending & 0xFFF0u) != 0u) irqs |= 1u << 7;
    if ((pending & ((1u << 17) | (1u << 18))) != 0u) irqs |= 1u << 12;
    return irqs;
}
