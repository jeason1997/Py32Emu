#include "py32emu/peripherals/rcc.h"

#include <string.h>

void py32_rcc_reset(Py32Rcc *rcc)
{
    memset(rcc, 0, sizeof(*rcc));
    /* PY32F002A 复位后 HSI 打开且可用，系统时钟选择 HSI。 */
    rcc->cr = (1u << 8) | (1u << 10);
}

static uint32_t *register_at(Py32Rcc *rcc, uint32_t offset)
{
    switch (offset) {
    case 0x00: return &rcc->cr; case 0x04: return &rcc->icscr;
    case 0x08: return &rcc->cfgr; case 0x10: return &rcc->ecscr;
    case 0x18: return &rcc->cier; case 0x1C: return &rcc->cifr;
    case 0x24: return &rcc->ioprstr; case 0x28: return &rcc->ahbrstr;
    case 0x2C: return &rcc->apbrstr1; case 0x30: return &rcc->apbrstr2;
    case 0x34: return &rcc->iopenr; case 0x38: return &rcc->ahbenr;
    case 0x3C: return &rcc->apbenr1; case 0x40: return &rcc->apbenr2;
    case 0x54: return &rcc->ccipr; case 0x5C: return &rcc->bdcr;
    case 0x60: return &rcc->csr; default: return NULL;
    }
}

bool py32_rcc_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value)
{
    uint32_t *reg;
    if (size != 4u || (offset & 3u) != 0u) return false;
    reg = register_at(context, offset);
    if (reg == NULL) return false;
    *value = *reg;
    return true;
}

bool py32_rcc_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value)
{
    Py32Rcc *rcc = context;
    uint32_t *reg;
    if (size != 4u || (offset & 3u) != 0u) return false;
    if (offset == 0x20u) { rcc->cifr &= ~value; return true; }
    reg = register_at(rcc, offset);
    if (reg == NULL) return false;
    *reg = value;
    if (offset == 0x00u) {
        /* 模拟器中的内部振荡器立即稳定。 */
        if (value & (1u << 8)) rcc->cr |= 1u << 10;
        if (value & 1u) rcc->cr |= 1u << 1;
    } else if (offset == 0x08u) {
        rcc->cfgr = (rcc->cfgr & ~(7u << 3)) | ((value & 7u) << 3);
    }
    return true;
}

