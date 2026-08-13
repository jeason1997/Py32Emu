#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))
#define RCC_APBENR2 REG32(0x40021040u)
#define RCC_IOPENR REG32(0x40021034u)
#define GPIOA_MODER REG32(0x50000000u)
#define GPIOA_BSRR REG32(0x50000018u)
#define NVIC_ISER REG32(0xE000E100u)
#define NVIC_ISPR REG32(0xE000E200u)
#define NVIC_IPR1 REG32(0xE000E404u)

static volatile uint8_t sequence[3];
static volatile unsigned position;

void SystemInit(void) { }

void EXTI0_1_IRQHandler(void)
{
    sequence[position++] = 2u;
}

void EXTI2_3_IRQHandler(void)
{
    sequence[position++] = 1u;
    NVIC_ISPR = 1u << 5;
    sequence[position++] = 3u;
}

int main(void)
{
    RCC_IOPENR |= 1u;
    RCC_APBENR2 |= 0u;
    GPIOA_MODER = (GPIOA_MODER & ~(3u << 10)) | (1u << 10);
    NVIC_IPR1 = 0x00C04000u; /* IRQ5 priority 1, IRQ6 priority 3. */
    NVIC_ISER = (1u << 5) | (1u << 6);
    NVIC_ISPR = 1u << 6;
    while (position != 3u) { }
    if (sequence[0] == 1u && sequence[1] == 2u && sequence[2] == 3u)
        GPIOA_BSRR = 1u << 5;
    __asm volatile ("bkpt #0");
    for (;;) { }
}
