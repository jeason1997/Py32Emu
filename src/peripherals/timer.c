#include "py32emu/peripherals/timer.h"

#include <string.h>

enum { TIM_CR1 = 0, TIM_DIER = 3, TIM_SR = 4, TIM_EGR = 5,
       TIM_CNT = 9, TIM_PSC = 10, TIM_ARR = 11,
       TIM_CR1_CEN = 1u, TIM_DIER_UIE = 1u, TIM_SR_UIF = 1u };

static bool clocked(const Py32Timer *timer)
{
    return timer->clock_enable_register == NULL ||
           (*timer->clock_enable_register & timer->clock_enable_mask) != 0u;
}

void py32_timer_reset(Py32Timer *timer, const uint32_t *clock_register,
                      uint32_t clock_mask)
{
    memset(timer, 0, sizeof(*timer));
    timer->reg[TIM_ARR] = 0xFFFFu;
    timer->clock_enable_register = clock_register;
    timer->clock_enable_mask = clock_mask;
}

bool py32_timer_read(void *context, uint32_t offset, unsigned size,
                     uint32_t *value)
{
    Py32Timer *timer = context;
    if (size != 2u && size != 4u) return false;
    if ((offset & 3u) != 0u || offset > 0x50u) return false;
    *value = clocked(timer) ? timer->reg[offset / 4u] : 0u;
    if (size == 2u) *value &= 0xFFFFu;
    return true;
}

bool py32_timer_write(void *context, uint32_t offset, unsigned size,
                      uint32_t value)
{
    Py32Timer *timer = context;
    unsigned index;
    if (size != 2u && size != 4u) return false;
    if ((offset & 3u) != 0u || offset > 0x50u) return false;
    if (!clocked(timer)) return true;
    index = offset / 4u;
    if (index == TIM_SR) timer->reg[index] &= value;
    else if (index == TIM_EGR) {
        timer->reg[index] = value;
        if (value & 1u) {
            timer->reg[TIM_CNT] = 0;
            timer->prescaler_counter = 0;
            timer->reg[TIM_SR] |= TIM_SR_UIF;
        }
    } else {
        timer->reg[index] = size == 2u ? value & 0xFFFFu : value;
    }
    return true;
}

bool py32_timer_tick(Py32Timer *timer, unsigned cycles)
{
    bool request = false;
    if (!clocked(timer) || (timer->reg[TIM_CR1] & TIM_CR1_CEN) == 0u)
        return false;
    while (cycles-- > 0u) {
        if (++timer->prescaler_counter > (timer->reg[TIM_PSC] & 0xFFFFu)) {
            timer->prescaler_counter = 0;
            if (timer->reg[TIM_CNT] >= (timer->reg[TIM_ARR] & 0xFFFFu)) {
                timer->reg[TIM_CNT] = 0;
                timer->reg[TIM_SR] |= TIM_SR_UIF;
                if (timer->reg[TIM_DIER] & TIM_DIER_UIE) request = true;
            } else ++timer->reg[TIM_CNT];
        }
    }
    return request;
}
