#ifndef PY32EMU_PERIPHERALS_FLASH_H
#define PY32EMU_PERIPHERALS_FLASH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *memory;
    uint32_t size;
    uint32_t page_size;
    uint32_t acr;
    uint32_t sr;
    uint32_t cr;
    uint32_t optr;
    uint32_t sdkr;
    uint32_t wrpr;
    unsigned key_stage;
    uint64_t erase_count;
    uint64_t program_word_count;
    uint32_t last_address;
} Py32Flash;

void py32_flash_reset(Py32Flash *flash, uint8_t *memory, uint32_t size,
                      uint32_t page_size);
bool py32_flash_memory_read(void *context, uint32_t offset, unsigned size,
                            uint32_t *value);
bool py32_flash_memory_write(void *context, uint32_t offset, unsigned size,
                             uint32_t value);
bool py32_flash_control_read(void *context, uint32_t offset, unsigned size,
                             uint32_t *value);
bool py32_flash_control_write(void *context, uint32_t offset, unsigned size,
                              uint32_t value);

#endif
