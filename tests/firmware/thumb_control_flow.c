#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))
#define RCC_IOPENR REG32(0x40021034u)
#define GPIOA_MODER REG32(0x50000000u)
#define GPIOA_BSRR REG32(0x50000018u)
#define SYST_CSR REG32(0xE000E010u)
#define SYST_RVR REG32(0xE000E014u)
#define SYST_CVR REG32(0xE000E018u)

volatile uint32_t svc_seen;
volatile uint32_t systick_seen;
extern int thumb_control_flow_test(void);

void SystemInit(void) { }
void SVC_Handler(void) { ++svc_seen; }
void SysTick_Handler(void) { ++systick_seen; }

int main(void)
{
    RCC_IOPENR |= 1u;
    GPIOA_MODER = (GPIOA_MODER & ~(3u << 10)) | (1u << 10);
    if (thumb_control_flow_test()) {
        SYST_RVR = 10u;
        SYST_CVR = 0u;
        SYST_CSR = 7u;
        __asm volatile ("wfi");
        SYST_CSR = 0u;
        if (systick_seen != 0u) GPIOA_BSRR = 1u << 5;
    }
    __asm volatile ("bkpt #0");
    for (;;) { }
}
