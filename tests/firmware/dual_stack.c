#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))
#define RCC_IOPENR REG32(0x40021034u)
#define GPIOA_MODER REG32(0x50000000u)
#define GPIOA_BSRR REG32(0x50000018u)
#define NVIC_ISER REG32(0xE000E100u)
#define NVIC_ISPR REG32(0xE000E200u)

static uint32_t process_stack[64] __attribute__((aligned(8)));
static volatile uint32_t psp_before, psp_during, psp_after, msp_during;
static volatile unsigned handled;

void SystemInit(void) { }

static inline uint32_t read_psp(void)
{
    uint32_t value;
    __asm volatile ("mrs %0, psp" : "=r" (value));
    return value;
}

static inline uint32_t read_msp(void)
{
    uint32_t value;
    __asm volatile ("mrs %0, msp" : "=r" (value));
    return value;
}

void EXTI0_1_IRQHandler(void)
{
    psp_during = read_psp();
    msp_during = read_msp();
    handled = 1u;
}

int main(void)
{
    uint32_t top = (uint32_t)(uintptr_t)(process_stack + 64);
    RCC_IOPENR |= 1u;
    GPIOA_MODER = (GPIOA_MODER & ~(3u << 10)) | (1u << 10);
    NVIC_ISER = 1u << 5;
    __asm volatile ("msr psp, %0\n"
                    "movs r0, #2\n"
                    "msr control, r0\n"
                    "isb" :: "r" (top) : "r0", "memory");
    psp_before = read_psp();
    NVIC_ISPR = 1u << 5;
    while (handled == 0u) { }
    psp_after = read_psp();
    __asm volatile ("movs r0, #0\n"
                    "msr control, r0\n"
                    "isb" ::: "r0", "memory");
    if (psp_during + 32u == psp_before && psp_after == psp_before &&
        msp_during != psp_during) GPIOA_BSRR = 1u << 5;
    __asm volatile ("bkpt #0");
    for (;;) { }
}
