#include "py32emu/chips/chip.h"

#include <string.h>

const Py32ChipDescription *py32_chip_by_name(const char *name)
{
    const Py32ChipDescription *chip = py32f002ax5_description();
    const Py32ChipDescription *extended = py32f002a_32k_description();
    if (name != NULL && (strcmp(name, chip->name) == 0 ||
                         strcmp(name, "py32f002a") == 0)) return chip;
    if (name != NULL && strcmp(name, extended->name) == 0) return extended;
    return NULL;
}
