#include "py32emu/peripherals/adc.h"

#include <string.h>

#define CR_ADCAL (UINT32_C(1) << 31)

enum {
    ISR_EOSMP = 1u << 1, ISR_EOC = 1u << 2, ISR_EOS = 1u << 3,
    ISR_OVR = 1u << 4, ISR_AWD = 1u << 7,
    CR_ADEN = 1u, CR_ADSTART = 1u << 2, CR_ADSTP = 1u << 4,
    CFGR1_SCANDIR = 1u << 2, CFGR1_RESSEL_MASK = 3u << 3,
    CFGR1_ALIGN = 1u << 5, CFGR1_AWDSGL = 1u << 22,
    CFGR1_AWDEN = 1u << 23
};

static bool clocked(const Py32Adc *adc)
{
    return adc->clock_enable_register == NULL ||
           (*adc->clock_enable_register & adc->clock_enable_mask) != 0u;
}

static unsigned selected_channel(const Py32Adc *adc)
{
    int channel;
    if ((adc->cfgr1 & CFGR1_SCANDIR) != 0u) {
        for (channel = (int)PY32_ADC_CHANNELS - 1; channel >= 0; --channel)
            if ((adc->chselr & (1u << channel)) != 0u) return (unsigned)channel;
    } else {
        for (channel = 0; channel < (int)PY32_ADC_CHANNELS; ++channel)
            if ((adc->chselr & (1u << channel)) != 0u) return (unsigned)channel;
    }
    return 0u;
}

static void convert(Py32Adc *adc)
{
    unsigned channel = selected_channel(adc);
    unsigned resolution = (adc->cfgr1 & CFGR1_RESSEL_MASK) >> 3;
    unsigned bits = resolution == 0u ? 12u : resolution == 1u ? 10u
                    : resolution == 2u ? 8u : 6u;
    uint32_t result = adc->channel_value[channel] >> (12u - bits);
    uint32_t low = adc->tr & 0xFFFu;
    uint32_t high = (adc->tr >> 16) & 0xFFFu;
    if ((adc->isr & ISR_EOC) != 0u) adc->isr |= ISR_OVR;
    adc->dr = (adc->cfgr1 & CFGR1_ALIGN) != 0u ? result << (16u - bits) : result;
    adc->cr &= ~CR_ADSTART;
    adc->isr |= ISR_EOSMP | ISR_EOC | ISR_EOS;
    if ((adc->cfgr1 & CFGR1_AWDEN) != 0u &&
        (result < low || result > high) &&
        ((adc->cfgr1 & CFGR1_AWDSGL) == 0u ||
         channel == ((adc->cfgr1 >> 26) & 0xFu))) adc->isr |= ISR_AWD;
    ++adc->conversion_count;
}

void py32_adc_reset(Py32Adc *adc, const uint32_t *clock_register,
                    uint32_t clock_mask)
{
    unsigned channel;
    memset(adc, 0, sizeof(*adc));
    for (channel = 0; channel < PY32_ADC_CHANNELS; ++channel)
        adc->channel_value[channel] = 0u;
    adc->channel_value[11] = 1500u; /* nominal temperature-sensor sample */
    adc->channel_value[12] = 1489u; /* 1.2 V VREFINT at a nominal 3.3 V VDDA */
    adc->tr = 0x0FFF0000u;
    adc->clock_enable_register = clock_register;
    adc->clock_enable_mask = clock_mask;
}

void py32_adc_set_channel(Py32Adc *adc, unsigned channel, uint16_t value)
{
    if (adc == NULL || channel >= PY32_ADC_CHANNELS) return;
    adc->channel_value[channel] = value > 4095u ? 4095u : value;
}

bool py32_adc_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value)
{
    Py32Adc *adc = context;
    if (size != 4u || value == NULL || (offset & 3u) != 0u) return false;
    if (!clocked(adc)) { *value = 0; return true; }
    switch (offset) {
    case 0x00: *value = adc->isr; break;
    case 0x04: *value = adc->ier; break;
    case 0x08: *value = adc->cr; break;
    case 0x0C: *value = adc->cfgr1; break;
    case 0x10: *value = adc->cfgr2; break;
    case 0x14: *value = adc->smpr; break;
    case 0x20: *value = adc->tr; break;
    case 0x28: *value = adc->chselr; break;
    case 0x40:
        *value = adc->dr;
        adc->isr &= ~(ISR_EOC | ISR_EOS | ISR_OVR);
        break;
    case 0x44: *value = adc->ccsr; break;
    default: return false;
    }
    return true;
}

bool py32_adc_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value)
{
    Py32Adc *adc = context;
    if (size != 4u || (offset & 3u) != 0u) return false;
    if (!clocked(adc)) return true;
    switch (offset) {
    case 0x00: adc->isr &= ~value; break;
    case 0x04: adc->ier = value; break;
    case 0x08:
        if ((value & CR_ADCAL) != 0u) {
            adc->cr = value & ~CR_ADCAL;
            adc->dr = 0u;
        } else {
            adc->cr = value;
            if ((value & CR_ADSTP) != 0u) adc->cr &= ~(CR_ADSTART | CR_ADSTP);
            if ((value & (CR_ADEN | CR_ADSTART)) == (CR_ADEN | CR_ADSTART))
                convert(adc);
        }
        break;
    case 0x0C: adc->cfgr1 = value; break;
    case 0x10: adc->cfgr2 = value; break;
    case 0x14: adc->smpr = value; break;
    case 0x20: adc->tr = value; break;
    case 0x28: adc->chselr = value & 0x1BFFu; break;
    case 0x40: adc->dr = value; break;
    case 0x44: adc->ccsr = value & ~(1u << 31); break;
    default: return false;
    }
    return true;
}

bool py32_adc_irq_pending(const Py32Adc *adc)
{
    const uint32_t flags = ISR_EOSMP | ISR_EOC | ISR_EOS | ISR_OVR | ISR_AWD;
    return adc != NULL && clocked(adc) && (adc->ier & adc->isr & flags) != 0u;
}
