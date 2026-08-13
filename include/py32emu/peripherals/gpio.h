#ifndef PY32EMU_PERIPHERALS_GPIO_H
#define PY32EMU_PERIPHERALS_GPIO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t moder, otyper, ospeedr, pupdr;
    uint16_t odr;
    uint16_t input_values;
    uint16_t input_driven;
    uint32_t lckr, afr[2];
    const uint32_t *clock_enable_register;
    uint32_t clock_enable_mask;
} Py32Gpio;

void py32_gpio_reset(Py32Gpio *gpio, const uint32_t *clock_register,
                     uint32_t clock_mask);
bool py32_gpio_read(void *context, uint32_t offset, unsigned size,
                    uint32_t *value);
bool py32_gpio_write(void *context, uint32_t offset, unsigned size,
                     uint32_t value);
void py32_gpio_set_input(Py32Gpio *gpio, unsigned pin, bool driven, bool high);
uint32_t py32_gpio_input_data(const Py32Gpio *gpio);
bool py32_gpio_pin_output(const Py32Gpio *gpio, unsigned pin,
                          bool *driven, bool *high);

#endif
