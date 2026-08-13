#ifndef PY32EMU_PERIPHERALS_LPTIM_H
#define PY32EMU_PERIPHERALS_LPTIM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t isr, ier, cfgr, cr, arr, cnt;
    uint64_t clock_accumulator;
    uint64_t match_count;
    const uint32_t *clock_enable_register;
    uint32_t clock_enable_mask;
} Py32Lptim;

void py32_lptim_reset(Py32Lptim *lptim, const uint32_t *clock_register,
                      uint32_t clock_mask);
bool py32_lptim_read(void *context, uint32_t offset, unsigned size,
                     uint32_t *value);
bool py32_lptim_write(void *context, uint32_t offset, unsigned size,
                      uint32_t value);
bool py32_lptim_tick(Py32Lptim *lptim, uint32_t cpu_cycles,
                     uint32_t cpu_clock_hz, uint32_t source_clock_hz);

#endif
