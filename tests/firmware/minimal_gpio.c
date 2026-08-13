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
    volatile uint32_t *const gpioa_moder = (uint32_t *)0x50000000u;
    volatile uint32_t *const gpioa_bsrr = (uint32_t *)0x50000018u;

    *rcc_iopenr |= 1u;
    *gpioa_moder = (*gpioa_moder & ~(3u << 10)) | (1u << 10);
    *gpioa_bsrr = 1u << 5;
    __asm volatile ("bkpt #0");
    for (;;) {}
}

static void default_handler(void)
{
    for (;;) {}
}

