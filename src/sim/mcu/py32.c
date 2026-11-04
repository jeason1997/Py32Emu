#include "py32emu/sim/mcu/py32.h"

#include <stddef.h>

enum {
    PY32_PIN_PA0 = 0u,
    PY32_PIN_PA1 = 1u,
    PY32_PIN_COUNT = 48u,
    TIM1_CR1 = 0, TIM1_CCER = 8, TIM1_CNT = 9, TIM1_ARR = 11,
    TIM1_CCR3 = 15, TIM1_CCR4 = 16, TIM1_BDTR = 17
};

static SimLevel py32_mcu_pin_drive(const SimMcu *mcu, unsigned pin);

static const Py32Gpio *gpio_for_pin(const Py32Soc *soc, unsigned port)
{
    if (port == 0u) return &soc->gpioa;
    if (port == 1u) return &soc->gpiob;
    if (port == 2u) return &soc->gpiof;
    return NULL;
}

static SimLevel pwm_level(const Py32Soc *soc, unsigned channel)
{
    const Py32Timer *tim = &soc->tim1;
    uint32_t polarity_bit = channel == 3u ? (1u << 9) : (1u << 13);
    uint32_t compare = tim->reg[channel == 3u ? TIM1_CCR3 : TIM1_CCR4];
    bool high = tim->reg[TIM1_CNT] < compare;
    if ((tim->reg[TIM1_CCER] & polarity_bit) != 0u) high = !high;
    return high ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW;
}

static bool pwm_active(const Py32Soc *soc)
{
    const Py32Timer *tim = &soc->tim1;
    const Py32Gpio *gpio = &soc->gpioa;
    return (soc->rcc.apbenr2 & (1u << 11)) != 0u &&
           (tim->reg[TIM1_CR1] & 1u) != 0u &&
           (tim->reg[TIM1_CCER] & ((1u << 8) | (1u << 12))) ==
               ((1u << 8) | (1u << 12)) &&
           (tim->reg[TIM1_BDTR] & (1u << 15)) != 0u &&
           (gpio->moder & 0xFu) == 0xAu &&
           (gpio->afr[0] & 0xFFu) == 0xDDu;
}

static SimLevel tim1_channel_level(const Py32Soc *soc, unsigned channel)
{
    if (!pwm_active(soc)) return SIM_LEVEL_Z;
    return pwm_level(soc, channel);
}

static void py32_mcu_reset(SimMcu *mcu)
{
    Py32Mcu *adapter = mcu->state;
    char error[32];
    adapter->last_pa0 = SIM_LEVEL_Z;
    adapter->last_pa1 = SIM_LEVEL_Z;
    mcu->pin_dirty = true;
    (void)py32_soc_reset(adapter->soc, error, sizeof(error));
}

static unsigned py32_mcu_step(SimMcu *mcu)
{
    Py32Mcu *adapter = mcu->state;
    SimLevel pa0 = SIM_LEVEL_Z, pa1 = SIM_LEVEL_Z;
    unsigned cycles = py32_soc_step(adapter->soc).cycles;
    if (pwm_active(adapter->soc)) {
        pa0 = pwm_level(adapter->soc, 3u);
        pa1 = pwm_level(adapter->soc, 4u);
    }
    if (pa0 != adapter->last_pa0 || pa1 != adapter->last_pa1) {
        adapter->last_pa0 = pa0;
        adapter->last_pa1 = pa1;
        mcu->pin_dirty = true;
    }
    return cycles;
}

static unsigned py32_mcu_pin_count(const SimMcu *mcu)
{
    (void)mcu;
    return PY32_PIN_COUNT;
}

static SimLevel py32_mcu_pin_drive(const SimMcu *mcu, unsigned pin)
{
    const Py32Mcu *adapter = mcu->state;
    const Py32Soc *soc = adapter->soc;
    unsigned port = pin / 16u, number = pin % 16u;
    const Py32Gpio *gpio = gpio_for_pin(soc, port);
    bool driven = false, high = false;

    if (gpio == NULL) return SIM_LEVEL_Z;
    /* PA0/PA1 在 AF13 时由 TIM1 CH3/CH4 驱动，是扬声器的差分输入。 */
    if (port == 0u && (number == PY32_PIN_PA0 || number == PY32_PIN_PA1)) {
        uint32_t mode = (gpio->moder >> (number * 2u)) & 3u;
        uint32_t af = (gpio->afr[number / 8u] >> ((number % 8u) * 4u)) & 0xFu;
        if (mode == 2u && af == 13u)
            return tim1_channel_level(soc, number == 0u ? 3u : 4u);
    }
    if (py32_gpio_pin_output(gpio, number, &driven, &high))
        return driven ? (high ? SIM_LEVEL_HIGH : SIM_LEVEL_LOW)
                      : SIM_LEVEL_Z;
    return SIM_LEVEL_Z;
}

static void py32_mcu_set_pin_input(SimMcu *mcu, unsigned pin,
                                   SimLevel level)
{
    Py32Mcu *adapter = mcu->state;
    bool driven = level == SIM_LEVEL_LOW || level == SIM_LEVEL_HIGH;
    if (pin >= PY32_PIN_COUNT) return;
    py32_soc_set_gpio_input(adapter->soc, pin / 16u, pin % 16u,
                            driven, level == SIM_LEVEL_HIGH);
}

static bool py32_mcu_stopped(const SimMcu *mcu)
{
    const Py32Mcu *adapter = mcu->state;
    return adapter->soc->cpu.stopped;
}

static const SimMcuOps PY32_MCU_OPS = {
    py32_mcu_reset,
    py32_mcu_step,
    py32_mcu_pin_count,
    py32_mcu_pin_drive,
    py32_mcu_set_pin_input,
    py32_mcu_stopped
};

void py32_mcu_init(Py32Mcu *adapter, Py32Soc *soc, uint32_t clock_hz)
{
    adapter->base.ops = &PY32_MCU_OPS;
    adapter->base.state = adapter;
    adapter->base.cycles_per_second = clock_hz != 0u ? clock_hz : 1u;
    adapter->base.pin_dirty = true;
    adapter->soc = soc;
    adapter->last_pa0 = SIM_LEVEL_Z;
    adapter->last_pa1 = SIM_LEVEL_Z;
}
