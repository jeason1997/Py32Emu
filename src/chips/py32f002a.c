#include "py32emu/chips/chip.h"

static const Py32ChipDescription py32f002ax5 = {
    "py32f002ax5",
    0x08000000u,
    20u * 1024u,
    128u,
    0x20000000u,
    3u * 1024u,
    32u,
    24000000u
};

static const Py32ChipDescription py32f002a_32k = {
    "py32f002a-32k",
    0x08000000u,
    32u * 1024u,
    128u,
    0x20000000u,
    4u * 1024u,
    32u,
    24000000u
};

const Py32ChipDescription *py32f002ax5_description(void)
{
    return &py32f002ax5;
}

const Py32ChipDescription *py32f002a_32k_description(void)
{
    return &py32f002a_32k;
}
