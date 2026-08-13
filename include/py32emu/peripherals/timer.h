#ifndef PY32EMU_PERIPHERALS_TIMER_H
#define PY32EMU_PERIPHERALS_TIMER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t reg[21];
    uint32_t prescaler_counter;
    const uint32_t *clock_enable_register;
    uint32_t clock_enable_mask;
} Py32Timer;

void py32_timer_reset(Py32Timer *timer, const uint32_t *clock_register,
                      uint32_t clock_mask);
bool py32_timer_read(void *context, uint32_t offset, unsigned size,
                     uint32_t *value);
bool py32_timer_write(void *context, uint32_t offset, unsigned size,
                      uint32_t value);
bool py32_timer_tick(Py32Timer *timer, unsigned cycles);

#endif

