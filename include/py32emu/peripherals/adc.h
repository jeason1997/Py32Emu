#ifndef PY32EMU_PERIPHERALS_ADC_H
#define PY32EMU_PERIPHERALS_ADC_H

#include <stdbool.h>
#include <stdint.h>

#define PY32_ADC_CHANNELS 13u

typedef struct {
    uint32_t isr, ier, cr, cfgr1, cfgr2, smpr, tr, chselr, dr, ccsr;
    uint16_t channel_value[PY32_ADC_CHANNELS];
    uint64_t conversion_count;
    const uint32_t *clock_enable_register;
    uint32_t clock_enable_mask;
} Py32Adc;

void py32_adc_reset(Py32Adc *adc, const uint32_t *clock_register,
                    uint32_t clock_mask);
void py32_adc_set_channel(Py32Adc *adc, unsigned channel, uint16_t value);
bool py32_adc_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value);
bool py32_adc_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value);
bool py32_adc_irq_pending(const Py32Adc *adc);

#endif
