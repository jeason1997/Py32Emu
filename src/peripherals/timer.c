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

uint32_t py32_timer_compare(const Py32Timer *timer, unsigned channel)
{
    unsigned index, shift;
    if (channel < 1u || channel > 4u) return 0u;
    index = channel - 1u;
    shift = (index & 1u) * 8u;
    return (timer->reg[6u + index / 2u] & (1u << (shift + 3u))) != 0u
        ? timer->active_ccr[index] : timer->reg[13u + index];
}

static void update_event(Py32Timer *timer, bool software)
{
    unsigned channel;
    /* UDIS 禁止更新传送；URS 只限制软件更新产生的 UIF/中断。 */
    if ((timer->reg[TIM_CR1] & (1u << 1)) != 0u) return;
    for (channel = 0u; channel < 4u; ++channel)
        timer->active_ccr[channel] = timer->reg[13u + channel];
    if (!software || (timer->reg[TIM_CR1] & (1u << 2)) == 0u)
        timer->reg[TIM_SR] |= TIM_SR_UIF;
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
            update_event(timer, true);
        }
    } else {
        timer->reg[index] = size == 2u ? value & 0xFFFFu : value;
    }
    return true;
}

bool py32_timer_tick(Py32Timer *timer, unsigned cycles)
{
    uint32_t psc, arr;
    if (!clocked(timer) || (timer->reg[TIM_CR1] & TIM_CR1_CEN) == 0u)
        return false;
    psc = timer->reg[TIM_PSC] & 0xFFFFu;
    arr = timer->reg[TIM_ARR] & 0xFFFFu;
    if (psc == 0u && timer->prescaler_counter == 0u &&
        timer->reg[TIM_CNT] <= arr) {
        uint64_t count = (uint64_t)timer->reg[TIM_CNT] + cycles;
        if (count <= arr) {
            timer->reg[TIM_CNT] = (uint32_t)count;
            return false;
        }
        timer->reg[TIM_CNT] = (uint32_t)(count % (arr + 1u));
        update_event(timer, false);
        return (timer->reg[TIM_CR1] & (1u << 1)) == 0u &&
               (timer->reg[TIM_DIER] & TIM_DIER_UIE) != 0u;
    }
    if (cycles == 1u) {
        if (++timer->prescaler_counter > psc) {
            timer->prescaler_counter = 0;
            if (timer->reg[TIM_CNT] >= arr) {
                timer->reg[TIM_CNT] = 0;
                update_event(timer, false);
                return (timer->reg[TIM_CR1] & (1u << 1)) == 0u &&
                       (timer->reg[TIM_DIER] & TIM_DIER_UIE) != 0u;
            }
            ++timer->reg[TIM_CNT];
        }
        return false;
    }
    if (cycles != 0u) {
        uint64_t ticks;
        uint64_t first = timer->prescaler_counter > psc ? 1u :
                         (uint64_t)psc + 1u - timer->prescaler_counter;
        uint64_t until_update;
        if (cycles < first) {
            timer->prescaler_counter += cycles;
            return false;
        }
        /* 按计数事件推进，避免 MUL 等多周期指令逐周期循环。 */
        ticks = 1u + ((uint64_t)cycles - first) / (psc + 1u);
        timer->prescaler_counter = (uint32_t)(((uint64_t)cycles - first) %
                                             (psc + 1u));
        until_update = timer->reg[TIM_CNT] >= arr ? 1u :
                       (uint64_t)arr + 1u - timer->reg[TIM_CNT];
        if (ticks < until_update) {
            timer->reg[TIM_CNT] += (uint32_t)ticks;
            return false;
        }
        timer->reg[TIM_CNT] = (uint32_t)((ticks - until_update) % (arr + 1u));
        update_event(timer, false);
        return (timer->reg[TIM_CR1] & (1u << 1)) == 0u &&
               (timer->reg[TIM_DIER] & TIM_DIER_UIE) != 0u;
    }
    return false;
}
