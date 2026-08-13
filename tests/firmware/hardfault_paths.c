#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))
#define RCC_IOPENR REG32(0x40021034u)
#define GPIOA_MODER REG32(0x50000000u)
#define GPIOA_BSRR REG32(0x50000018u)

volatile uint32_t hardfault_count;
extern void trigger_undefined_instruction(void);
extern void trigger_unaligned_load(void);

void SystemInit(void) { }
void HardFault_Handler(void) { ++hardfault_count; }

int main(void)
{
    volatile uint32_t ignored;

    RCC_IOPENR |= 1u;
    GPIOA_MODER = (GPIOA_MODER & ~(3u << 10)) | (1u << 10);

    __asm volatile ("svc #0");
    trigger_undefined_instruction();
    ignored = REG32(0x60000000u);
    trigger_unaligned_load();
    (void)ignored;
    if (hardfault_count == 4u) GPIOA_BSRR = 1u << 5;

    __asm volatile ("bkpt #0");
    for (;;) { }
}
