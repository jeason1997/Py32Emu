#ifndef PY32EMU_PERIPHERALS_CRC_H
#define PY32EMU_PERIPHERALS_CRC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t dr;
    uint8_t idr;
    const uint32_t *clock_enable_register;
    uint32_t clock_enable_mask;
} Py32Crc;

void py32_crc_reset(Py32Crc *crc, const uint32_t *clock_register,
                    uint32_t clock_mask);
bool py32_crc_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value);
bool py32_crc_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value);

#endif
