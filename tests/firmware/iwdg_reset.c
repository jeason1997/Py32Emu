#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))
#define RCC_CSR REG32(0x40021060u)
#define RCC_IOPENR REG32(0x40021034u)
#define GPIOA_MODER REG32(0x50000000u)
#define GPIOA_BSRR REG32(0x50000018u)
#define IWDG_KR REG32(0x40003000u)
#define IWDG_PR REG32(0x40003004u)
#define IWDG_RLR REG32(0x40003008u)

void SystemInit(void) { }

int main(void)
{
    RCC_IOPENR |= 1u;
    GPIOA_MODER = (GPIOA_MODER & ~(3u << 10)) | (1u << 10);
    if ((RCC_CSR & (1u << 29)) != 0u) {
        GPIOA_BSRR = 1u << 5;
        __asm volatile ("bkpt #0");
    }
    RCC_CSR |= 1u;
    IWDG_KR = 0xCCCCu;
    IWDG_KR = 0x5555u;
    IWDG_PR = 0u;
    IWDG_RLR = 1u;
    IWDG_KR = 0xAAAAu;
    for (;;) { __asm volatile ("nop"); }
}
