#ifndef PY32EMU_PERIPHERALS_RCC_H
#define PY32EMU_PERIPHERALS_RCC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t cr, icscr, cfgr, ecscr;
    uint32_t cier, cifr;
    uint32_t ioprstr, ahbrstr, apbrstr1, apbrstr2;
    uint32_t iopenr, ahbenr, apbenr1, apbenr2;
    uint32_t ccipr, bdcr, csr;
} Py32Rcc;

void py32_rcc_reset(Py32Rcc *rcc);
bool py32_rcc_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value);
bool py32_rcc_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value);

#endif

