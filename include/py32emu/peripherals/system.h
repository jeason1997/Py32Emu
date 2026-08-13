#ifndef PY32EMU_PERIPHERALS_SYSTEM_H
#define PY32EMU_PERIPHERALS_SYSTEM_H

#include "py32emu/core/cortex_m0.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    CortexM0 *cpu;
    uint32_t syst_csr, syst_rvr, syst_cvr, syst_calib;
    uint32_t nvic_enable, nvic_pending;
    uint8_t nvic_priority[32];
    uint32_t icsr;
    uint32_t scb_scr, scb_ccr, scb_shp[2], scb_shcsr;
    bool systick_pending;
    unsigned external_irq_count;
} Py32System;

void py32_system_reset(Py32System *system, CortexM0 *cpu,
                       uint32_t clock_hz, unsigned external_irq_count);
bool py32_system_read(void *context, uint32_t offset, unsigned size,
                      uint32_t *value);
bool py32_system_write(void *context, uint32_t offset, unsigned size,
                       uint32_t value);
void py32_system_tick(Py32System *system, unsigned cpu_cycles);
bool py32_system_service_exception(Py32System *system);

#endif
