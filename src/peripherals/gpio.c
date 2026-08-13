#include "py32emu/peripherals/gpio.h"

#include <string.h>

static bool clocked(const Py32Gpio *gpio)
{
    return gpio->clock_enable_register == NULL ||
           (*gpio->clock_enable_register & gpio->clock_enable_mask) != 0u;
}

void py32_gpio_reset(Py32Gpio *gpio, const uint32_t *clock_register,
                     uint32_t clock_mask)
{
    memset(gpio, 0, sizeof(*gpio));
    /* 通用引脚复位为模拟模式；SWD 等封装差异稍后由芯片描述覆盖。 */
    gpio->moder = 0xFFFFFFFFu;
    gpio->clock_enable_register = clock_register;
    gpio->clock_enable_mask = clock_mask;
}

uint32_t py32_gpio_input_data(const Py32Gpio *gpio)
{
    uint32_t value = 0;
    unsigned pin;
    for (pin = 0; pin < 16u; ++pin) {
        uint32_t mode = (gpio->moder >> (pin * 2u)) & 3u;
        uint32_t mask = 1u << pin;
        if (mode == 1u) {
            if (gpio->odr & mask) value |= mask;
        } else if (gpio->input_driven & mask) {
            if (gpio->input_values & mask) value |= mask;
        } else {
            uint32_t pull = (gpio->pupdr >> (pin * 2u)) & 3u;
            if (pull == 1u) value |= mask;
        }
    }
    return value;
}

bool py32_gpio_read(void *context, uint32_t offset, unsigned size,
                    uint32_t *value)
{
    Py32Gpio *gpio = context;
    if (size != 4u || (offset & 3u) != 0u) return false;
    if (!clocked(gpio)) { *value = 0; return true; }
    switch (offset) {
    case 0x00: *value = gpio->moder; break;
    case 0x04: *value = gpio->otyper; break;
    case 0x08: *value = gpio->ospeedr; break;
    case 0x0C: *value = gpio->pupdr; break;
    case 0x10: *value = py32_gpio_input_data(gpio); break;
    case 0x14: *value = gpio->odr; break;
    case 0x18: *value = 0; break;
    case 0x1C: *value = gpio->lckr; break;
    case 0x20: *value = gpio->afr[0]; break;
    case 0x24: *value = gpio->afr[1]; break;
    case 0x28: *value = 0; break;
    default: return false;
    }
    return true;
}

bool py32_gpio_write(void *context, uint32_t offset, unsigned size,
                     uint32_t value)
{
    Py32Gpio *gpio = context;
    if (size != 4u || (offset & 3u) != 0u) return false;
    if (!clocked(gpio)) return true;
    switch (offset) {
    case 0x00: gpio->moder = value; break;
    case 0x04: gpio->otyper = value & 0xFFFFu; break;
    case 0x08: gpio->ospeedr = value; break;
    case 0x0C: gpio->pupdr = value; break;
    case 0x14: gpio->odr = (uint16_t)value; break;
    case 0x18:
        gpio->odr = (uint16_t)((gpio->odr | (value & 0xFFFFu)) &
                               ~(value >> 16));
        break;
    case 0x1C: gpio->lckr = value; break;
    case 0x20: gpio->afr[0] = value; break;
    case 0x24: gpio->afr[1] = value; break;
    case 0x28: gpio->odr &= (uint16_t)~value; break;
    default: return false;
    }
    return true;
}

void py32_gpio_set_input(Py32Gpio *gpio, unsigned pin, bool driven, bool high)
{
    uint16_t mask;
    if (gpio == NULL || pin >= 16u) return;
    mask = (uint16_t)(1u << pin);
    if (driven) gpio->input_driven |= mask; else gpio->input_driven &= ~mask;
    if (high) gpio->input_values |= mask; else gpio->input_values &= ~mask;
}

bool py32_gpio_pin_output(const Py32Gpio *gpio, unsigned pin,
                          bool *driven, bool *high)
{
    uint32_t mode;
    if (gpio == NULL || pin >= 16u || driven == NULL || high == NULL)
        return false;
    mode = (gpio->moder >> (pin * 2u)) & 3u;
    *driven = clocked(gpio) && mode == 1u;
    *high = (gpio->odr & (1u << pin)) != 0u;
    return true;
}
