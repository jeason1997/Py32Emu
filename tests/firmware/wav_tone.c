#include <stdint.h>

extern uint32_t _stack_top;
static void reset_handler(void);
static void default_handler(void);

__attribute__((section(".vectors"), used))
const uintptr_t vectors[48] = {
    (uintptr_t)&_stack_top,
    (uintptr_t)reset_handler,
    [2 ... 47] = (uintptr_t)default_handler
};

static void reset_handler(void)
{
    volatile uint32_t *const rcc_iopenr = (uint32_t *)0x40021034u;
    volatile uint32_t *const rcc_apbenr2 = (uint32_t *)0x40021040u;
    volatile uint32_t *const gpioa_moder = (uint32_t *)0x50000000u;
    volatile uint32_t *const gpioa_afr = (uint32_t *)0x50000020u;
    volatile uint32_t *const tim1_cr1 = (uint32_t *)0x40012C00u;
    volatile uint32_t *const tim1_ccer = (uint32_t *)0x40012C20u;
    volatile uint32_t *const tim1_arr = (uint32_t *)0x40012C2Cu;
    volatile uint32_t *const tim1_ccr3 = (uint32_t *)0x40012C3Cu;
    volatile uint32_t *const tim1_ccr4 = (uint32_t *)0x40012C40u;
    volatile uint32_t *const tim1_bdtr = (uint32_t *)0x40012C44u;

    *rcc_iopenr |= 1u;
    *rcc_apbenr2 |= 1u << 11;
    /* PA0/PA1 设为 AF13，分别接 TIM1_CH3/CH4，模拟真实差分扬声器接线。 */
    *gpioa_moder = (*gpioa_moder & ~0xFu) | 0xAu;
    *gpioa_afr = (*gpioa_afr & ~0xFFu) | 0xDDu;
    *tim1_arr = 255u;
    *tim1_ccer |= (1u << 8) | (1u << 12);
    *tim1_bdtr |= 1u << 15;
    *tim1_cr1 = 1u;

    for (uint32_t i = 0u; i < 256u; ++i) {
        uint32_t value = (i * 37u) & 0xFFu;
        *tim1_ccr3 = 256u - value;
        *tim1_ccr4 = value;
        /* 保持一个以上 22.05 kHz 采样周期（48 MHz / 22050 ≈ 2177）。 */
        for (volatile uint32_t delay = 0u; delay < 2000u; ++delay) {}
    }
    __asm volatile ("bkpt #0");
    for (;;) {}
}

static void default_handler(void)
{
    for (;;) {}
}
