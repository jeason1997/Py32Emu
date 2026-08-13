#include "py32emu/peripherals/flash.h"

#include <string.h>

#define FLASH_SR_EOP       (1u << 0)
#define FLASH_SR_WRPERR    (1u << 4)
#define FLASH_CR_PG        (1u << 0)
#define FLASH_CR_PER       (1u << 1)
#define FLASH_CR_MER       (1u << 2)
#define FLASH_CR_SER       (1u << 11)
#define FLASH_CR_LOCK      (1u << 31)
#define FLASH_KEY1         0x45670123u
#define FLASH_KEY2         0xCDEF89ABu

static bool valid_access(uint32_t offset, unsigned size, uint32_t limit)
{
    return (size == 1u || size == 2u || size == 4u) &&
           offset <= limit && size <= limit - offset &&
           (offset & (size - 1u)) == 0u;
}

void py32_flash_reset(Py32Flash *flash, uint8_t *memory, uint32_t size,
                      uint32_t page_size)
{
    uint64_t erase_count = flash->erase_count;
    uint64_t program_count = flash->program_word_count;
    memset(flash, 0, sizeof(*flash));
    flash->memory = memory;
    flash->size = size;
    flash->page_size = page_size;
    flash->cr = FLASH_CR_LOCK;
    flash->erase_count = erase_count;
    flash->program_word_count = program_count;
}

bool py32_flash_memory_read(void *context, uint32_t offset, unsigned size,
                            uint32_t *value)
{
    Py32Flash *flash = context;
    unsigned i;
    if (flash == 0 || value == 0 ||
        !valid_access(offset, size, flash->size)) return false;
    *value = 0u;
    for (i = 0; i < size; ++i)
        *value |= (uint32_t)flash->memory[offset + i] << (8u * i);
    return true;
}

bool py32_flash_memory_write(void *context, uint32_t offset, unsigned size,
                             uint32_t value)
{
    Py32Flash *flash = context;
    unsigned i;
    if (flash == 0 || !valid_access(offset, size, flash->size)) return false;
    if ((flash->cr & FLASH_CR_LOCK) != 0u) {
        flash->sr |= FLASH_SR_WRPERR;
        return true;
    }
    if ((flash->cr & FLASH_CR_MER) != 0u) {
        memset(flash->memory, 0xFF, flash->size);
        ++flash->erase_count;
    } else if ((flash->cr & FLASH_CR_PER) != 0u) {
        uint32_t start = offset - offset % flash->page_size;
        memset(flash->memory + start, 0xFF, flash->page_size);
        ++flash->erase_count;
    } else if ((flash->cr & FLASH_CR_SER) != 0u) {
        uint32_t sector_size = flash->page_size * 8u;
        uint32_t start = offset - offset % sector_size;
        uint32_t length = sector_size;
        if (length > flash->size - start) length = flash->size - start;
        memset(flash->memory + start, 0xFF, length);
        ++flash->erase_count;
    } else if ((flash->cr & FLASH_CR_PG) != 0u && size == 4u) {
        for (i = 0; i < size; ++i)
            flash->memory[offset + i] &= (uint8_t)(value >> (8u * i));
        ++flash->program_word_count;
    } else {
        flash->sr |= FLASH_SR_WRPERR;
        return true;
    }
    flash->last_address = 0x08000000u + offset;
    flash->sr |= FLASH_SR_EOP;
    return true;
}

bool py32_flash_control_read(void *context, uint32_t offset, unsigned size,
                             uint32_t *value)
{
    Py32Flash *flash = context;
    if (flash == 0 || value == 0 || size != 4u) return false;
    switch (offset) {
    case 0x00u: *value = flash->acr; break;
    case 0x08u: case 0x0Cu: *value = 0u; break;
    case 0x10u: *value = flash->sr; break;
    case 0x14u: *value = flash->cr; break;
    case 0x20u: *value = flash->optr; break;
    case 0x24u: *value = flash->sdkr; break;
    case 0x2Cu: *value = flash->wrpr; break;
    default: *value = 0u; break;
    }
    return true;
}

bool py32_flash_control_write(void *context, uint32_t offset, unsigned size,
                              uint32_t value)
{
    Py32Flash *flash = context;
    if (flash == 0 || size != 4u) return false;
    switch (offset) {
    case 0x00u: flash->acr = value; break;
    case 0x08u:
        if ((flash->cr & FLASH_CR_LOCK) == 0u) break;
        if (flash->key_stage == 0u && value == FLASH_KEY1)
            flash->key_stage = 1u;
        else if (flash->key_stage == 1u && value == FLASH_KEY2) {
            flash->cr &= ~FLASH_CR_LOCK;
            flash->key_stage = 0u;
        } else flash->key_stage = 0u;
        break;
    case 0x0Cu: break;
    case 0x10u: flash->sr &= ~value; break; /* 状态位写 1 清零。 */
    case 0x14u:
        if ((flash->cr & FLASH_CR_LOCK) != 0u)
            flash->cr = FLASH_CR_LOCK;
        else flash->cr = value;
        break;
    case 0x20u: flash->optr = value; break;
    case 0x24u: flash->sdkr = value; break;
    case 0x2Cu: flash->wrpr = value; break;
    default: break; /* 时序配置寄存器暂存为未实现但可访问。 */
    }
    return true;
}
