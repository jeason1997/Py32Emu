#ifndef PY32EMU_PERIPHERALS_EXTI_H
#define PY32EMU_PERIPHERALS_EXTI_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t rtsr, ftsr, swier, pr;
    uint32_t exticr[3];
    uint32_t imr, emr;
} Py32Exti;

void py32_exti_reset(Py32Exti *exti);
bool py32_exti_read(void *context, uint32_t offset, unsigned size,
                    uint32_t *value);
bool py32_exti_write(void *context, uint32_t offset, unsigned size,
                     uint32_t value);
void py32_exti_input(Py32Exti *exti, unsigned port, unsigned pin,
                     bool old_high, bool new_high);
void py32_exti_signal_line(Py32Exti *exti, unsigned line,
                           bool old_high, bool new_high);
uint32_t py32_exti_irq_mask(const Py32Exti *exti);

#endif
