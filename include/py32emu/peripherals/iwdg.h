#ifndef PY32EMU_PERIPHERALS_IWDG_H
#define PY32EMU_PERIPHERALS_IWDG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t pr, rlr, counter;
    uint64_t clock_accumulator;
    bool enabled, write_access;
} Py32Iwdg;

void py32_iwdg_reset(Py32Iwdg *iwdg);
bool py32_iwdg_read(void *context, uint32_t offset, unsigned size,
                    uint32_t *value);
bool py32_iwdg_write(void *context, uint32_t offset, unsigned size,
                     uint32_t value);
bool py32_iwdg_tick(Py32Iwdg *iwdg, unsigned cpu_cycles,
                    uint32_t cpu_clock_hz, bool lsi_ready);

#endif
