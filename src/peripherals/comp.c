#include "py32emu/peripherals/comp.h"

#include <string.h>

#define COMP_EN        (1u << 0)
#define COMP_POLARITY  (1u << 15)
#define COMP_OUT       (1u << 30)
#define COMP_LOCK      (1u << 31)

static bool enabled(const Py32Comp *comp, unsigned instance)
{
    uint32_t mask = 1u << (21u + instance);
    return comp->clock_enable_register != 0 &&
        (*comp->clock_enable_register & mask) != 0u;
}

bool py32_comp_output(const Py32Comp *comp, unsigned instance)
{
    return comp != 0 && instance < 2u &&
           (comp->csr[instance] & COMP_OUT) != 0u;
}

static uint16_t minus_mv(const Py32Comp *comp, unsigned instance)
{
    unsigned selection = (comp->csr[instance] >> 4) & 15u;
    switch (selection) {
    case 0u: return comp->vref_mv / 4u;
    case 1u: return comp->vref_mv / 2u;
    case 2u: return (uint16_t)(comp->vref_mv * 3u / 4u);
    case 3u: return comp->vref_mv;
    case 4u: return comp->vcc_mv;
    case 5u: return comp->temperature_mv;
    default: return 0u; /* 外部负输入可在后续按引脚模型扩展。 */
    }
}

static void update_output(Py32Comp *comp, unsigned instance)
{
    bool old_high = py32_comp_output(comp, instance);
    unsigned selection = (comp->csr[instance] >> 8) & 3u;
    bool high = false;
    if (enabled(comp, instance) && (comp->csr[instance] & COMP_EN) != 0u) {
        high = comp->plus_mv[instance][selection] >
               minus_mv(comp, instance);
        if ((comp->csr[instance] & COMP_POLARITY) != 0u) high = !high;
    }
    if (high) comp->csr[instance] |= COMP_OUT;
    else comp->csr[instance] &= ~COMP_OUT;
    if (old_high != high) {
        ++comp->transition_count[instance];
        py32_exti_signal_line(comp->exti, 17u + instance, old_high, high);
    }
}

void py32_comp_reset(Py32Comp *comp, const uint32_t *clock_register,
                     Py32Exti *exti)
{
    memset(comp, 0, sizeof(*comp));
    comp->vref_mv = 1200u;
    comp->vcc_mv = 3300u;
    comp->temperature_mv = 760u;
    comp->clock_enable_register = clock_register;
    comp->exti = exti;
}

bool py32_comp_read(void *context, uint32_t offset, unsigned size,
                    uint32_t *value)
{
    Py32Comp *comp = context;
    unsigned instance;
    if (comp == 0 || value == 0 || size != 4u) return false;
    if (offset != 0u && offset != 4u && offset != 0x10u && offset != 0x14u)
        return false;
    instance = offset >= 0x10u ? 1u : 0u;
    update_output(comp, instance);
    *value = (offset & 4u) != 0u ? comp->fr[instance] : comp->csr[instance];
    return true;
}

bool py32_comp_write(void *context, uint32_t offset, unsigned size,
                     uint32_t value)
{
    Py32Comp *comp = context;
    unsigned instance;
    if (comp == 0 || size != 4u) return false;
    if (offset != 0u && offset != 4u && offset != 0x10u && offset != 0x14u)
        return false;
    instance = offset >= 0x10u ? 1u : 0u;
    if ((offset & 4u) != 0u) {
        comp->fr[instance] = value;
        return true;
    }
    if ((comp->csr[instance] & COMP_LOCK) != 0u) return true;
    comp->csr[instance] = (value & ~COMP_OUT) |
                          (comp->csr[instance] & COMP_OUT);
    update_output(comp, instance);
    return true;
}

void py32_comp_set_plus_input(Py32Comp *comp, unsigned instance,
                              unsigned selection, uint16_t millivolts)
{
    if (comp == 0 || instance >= 2u || selection >= 4u) return;
    comp->plus_mv[instance][selection] = millivolts;
    update_output(comp, instance);
}
