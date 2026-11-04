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
    volatile uint32_t *const rcc_apbenr2 = (uint32_t *)0x40021040u;
    volatile uint32_t *const tim1_cr1 = (uint32_t *)0x40012C00u;
    volatile uint32_t *const tim1_ccr4 = (uint32_t *)0x40012C40u;
    volatile uint32_t *const tim16_cr1 = (uint32_t *)0x40014400u;

    *rcc_apbenr2 |= (1u << 11) | (1u << 17);
    *tim1_cr1 = 1u;
    *tim16_cr1 = 1u;
    for (uint32_t i = 0u; i < 256u; ++i) *tim1_ccr4 = (i * 37u) & 0xFFu;
    *tim16_cr1 = 0u;
    __asm volatile ("bkpt #0");
    for (;;) {}
}

static void default_handler(void)
{
    for (;;) {}
}
