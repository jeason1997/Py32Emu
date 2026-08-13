#include "py32emu/peripherals/crc.h"

enum { CRC_POLYNOMIAL = 0x04C11DB7u };

static bool enabled(const Py32Crc *crc)
{
    return crc->clock_enable_register != 0 &&
        (*crc->clock_enable_register & crc->clock_enable_mask) != 0u;
}

static uint32_t feed_word(uint32_t crc, uint32_t data)
{
    unsigned bit;
    crc ^= data;
    for (bit = 0; bit < 32u; ++bit)
        crc = (crc & 0x80000000u) != 0u
            ? (crc << 1) ^ CRC_POLYNOMIAL : crc << 1;
    return crc;
}

void py32_crc_reset(Py32Crc *crc, const uint32_t *clock_register,
                    uint32_t clock_mask)
{
    crc->dr = 0xFFFFFFFFu;
    crc->idr = 0u;
    crc->clock_enable_register = clock_register;
    crc->clock_enable_mask = clock_mask;
}

bool py32_crc_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value)
{
    Py32Crc *crc = context;
    if (!enabled(crc) || value == 0) return false;
    if (offset == 0u && size == 4u) *value = crc->dr;
    else if (offset == 4u && (size == 1u || size == 4u)) *value = crc->idr;
    else if (offset == 8u && size == 4u) *value = 0u;
    else return false;
    return true;
}

bool py32_crc_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value)
{
    Py32Crc *crc = context;
    if (!enabled(crc)) return false;
    if (offset == 0u && size == 4u) crc->dr = feed_word(crc->dr, value);
    else if (offset == 4u && (size == 1u || size == 4u))
        crc->idr = (uint8_t)value;
    else if (offset == 8u && size == 4u) {
        if ((value & 1u) != 0u) crc->dr = 0xFFFFFFFFu;
    } else return false;
    return true;
}
