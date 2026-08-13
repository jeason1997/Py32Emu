#ifndef PY32EMU_CHIPS_CHIP_H
#define PY32EMU_CHIPS_CHIP_H

#include <stdint.h>

typedef struct {
    const char *name;
    uint32_t flash_base;
    uint32_t flash_size;
    uint32_t flash_page_size;
    uint32_t sram_base;
    uint32_t sram_size;
    unsigned external_irq_count;
    uint32_t reset_clock_hz;
} Py32ChipDescription;

const Py32ChipDescription *py32_chip_by_name(const char *name);
const Py32ChipDescription *py32f002ax5_description(void);
const Py32ChipDescription *py32f002a_32k_description(void);

#endif
