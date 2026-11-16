#include <stdint.h>

extern int thumb_register_edges_test(void);
void SystemInit(void) { }

int main(void)
{
    volatile uint32_t *clock = (uint32_t *)0x40021034u;
    volatile uint32_t *mode = (uint32_t *)0x50000000u;
    volatile uint32_t *output = (uint32_t *)0x50000018u;
    *clock |= 1u;
    *mode = (*mode & ~(3u << 10)) | (1u << 10);
    if (thumb_register_edges_test()) *output = 1u << 5;
    __asm volatile ("bkpt #0");
    for (;;) { }
}
