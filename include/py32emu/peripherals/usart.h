#ifndef PY32EMU_PERIPHERALS_USART_H
#define PY32EMU_PERIPHERALS_USART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PY32_USART_BUFFER_SIZE 4096u

typedef struct {
    uint32_t sr, dr, brr, cr1, cr2, cr3, gtpr;
    uint8_t tx[PY32_USART_BUFFER_SIZE];
    size_t tx_count;
    uint8_t rx[PY32_USART_BUFFER_SIZE];
    size_t rx_head, rx_count;
    const uint32_t *clock_enable_register;
    uint32_t clock_enable_mask;
} Py32Usart;

void py32_usart_reset(Py32Usart *usart, const uint32_t *clock_register,
                      uint32_t clock_mask);
bool py32_usart_read(void *context, uint32_t offset, unsigned size,
                     uint32_t *value);
bool py32_usart_write(void *context, uint32_t offset, unsigned size,
                      uint32_t value);
size_t py32_usart_receive(Py32Usart *usart, const uint8_t *data, size_t size);

#endif

