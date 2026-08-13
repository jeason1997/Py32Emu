#ifndef PY32EMU_FIRMWARE_IMAGE_H
#define PY32EMU_FIRMWARE_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t address;
    uint32_t size;
    char *name;
} Py32FirmwareSymbol;

typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t load_address;
    Py32FirmwareSymbol *symbols;
    size_t symbol_count;
} Py32FirmwareImage;

void py32_firmware_image_init(Py32FirmwareImage *image);
void py32_firmware_image_free(Py32FirmwareImage *image);
bool py32_firmware_load_bin(Py32FirmwareImage *image, const char *path,
                            uint32_t load_address, size_t maximum_size,
                            char *error, size_t error_size);
bool py32_firmware_load_hex(Py32FirmwareImage *image, const char *path,
                            uint32_t flash_base, size_t flash_size,
                            char *error, size_t error_size);
bool py32_firmware_load_elf(Py32FirmwareImage *image, const char *path,
                            uint32_t flash_base, size_t flash_size,
                            char *error, size_t error_size);
const Py32FirmwareSymbol *py32_firmware_find_symbol(
    const Py32FirmwareImage *image, uint32_t address);

#endif
