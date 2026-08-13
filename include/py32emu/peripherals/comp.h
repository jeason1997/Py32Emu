#ifndef PY32EMU_PERIPHERALS_COMP_H
#define PY32EMU_PERIPHERALS_COMP_H

#include "py32emu/peripherals/exti.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t csr[2], fr[2];
    uint16_t plus_mv[2][4];
    uint16_t vref_mv, vcc_mv, temperature_mv;
    uint64_t transition_count[2];
    const uint32_t *clock_enable_register;
    Py32Exti *exti;
} Py32Comp;

void py32_comp_reset(Py32Comp *comp, const uint32_t *clock_register,
                     Py32Exti *exti);
bool py32_comp_read(void *context, uint32_t offset, unsigned size,
                    uint32_t *value);
bool py32_comp_write(void *context, uint32_t offset, unsigned size,
                     uint32_t value);
void py32_comp_set_plus_input(Py32Comp *comp, unsigned instance,
                              unsigned selection, uint16_t millivolts);
bool py32_comp_output(const Py32Comp *comp, unsigned instance);

#endif
