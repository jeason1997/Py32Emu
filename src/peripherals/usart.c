#include "py32emu/peripherals/usart.h"

#include <string.h>

enum { USART_SR_RXNE = 1u << 5, USART_SR_TC = 1u << 6,
       USART_SR_TXE = 1u << 7, USART_CR1_RE = 1u << 2,
       USART_CR1_TE = 1u << 3, USART_CR1_UE = 1u << 13 };

static bool clocked(const Py32Usart *usart)
{
    return usart->clock_enable_register == NULL ||
           (*usart->clock_enable_register & usart->clock_enable_mask) != 0u;
}

void py32_usart_reset(Py32Usart *usart, const uint32_t *clock_register,
                      uint32_t clock_mask)
{
    memset(usart, 0, sizeof(*usart));
    usart->sr = USART_SR_TXE | USART_SR_TC;
    usart->clock_enable_register = clock_register;
    usart->clock_enable_mask = clock_mask;
}

bool py32_usart_read(void *context, uint32_t offset, unsigned size,
                     uint32_t *value)
{
    Py32Usart *u = context;
    if (size != 1u && size != 2u && size != 4u) return false;
    if (!clocked(u)) { *value = 0; return true; }
    switch (offset) {
    case 0x00: *value = u->sr; break;
    case 0x04:
        if (u->rx_count > 0u) {
            *value = u->rx[u->rx_head];
            u->rx_head = (u->rx_head + 1u) % PY32_USART_BUFFER_SIZE;
            --u->rx_count;
            if (u->rx_count == 0u) u->sr &= ~USART_SR_RXNE;
        } else *value = u->dr;
        break;
    case 0x08: *value = u->brr; break; case 0x0C: *value = u->cr1; break;
    case 0x10: *value = u->cr2; break; case 0x14: *value = u->cr3; break;
    case 0x18: *value = u->gtpr; break; default: return false;
    }
    if (size == 1u) *value &= 0xFFu;
    else if (size == 2u) *value &= 0xFFFFu;
    return true;
}

bool py32_usart_write(void *context, uint32_t offset, unsigned size,
                      uint32_t value)
{
    Py32Usart *u = context;
    if (size != 1u && size != 2u && size != 4u) return false;
    if (!clocked(u)) return true;
    switch (offset) {
    case 0x00:
        /* SR 标志采用写 0 清除语义，硬件状态 TXE 随即重新置位。 */
        u->sr &= value;
        u->sr |= USART_SR_TXE;
        break;
    case 0x04:
        u->dr = value & 0x1FFu;
        if ((u->cr1 & (USART_CR1_UE | USART_CR1_TE)) ==
                      (USART_CR1_UE | USART_CR1_TE)) {
            if (u->tx_count < PY32_USART_BUFFER_SIZE)
                u->tx[u->tx_count++] = (uint8_t)value;
            u->sr |= USART_SR_TXE | USART_SR_TC;
        }
        break;
    case 0x08: u->brr = value & 0xFFFFu; break;
    case 0x0C: u->cr1 = value; break;
    case 0x10: u->cr2 = value; break;
    case 0x14: u->cr3 = value; break;
    case 0x18: u->gtpr = value; break;
    default: return false;
    }
    return true;
}

size_t py32_usart_receive(Py32Usart *u, const uint8_t *data, size_t size)
{
    size_t accepted = 0;
    if (u == NULL || data == NULL ||
        (u->cr1 & (USART_CR1_UE | USART_CR1_RE)) !=
                  (USART_CR1_UE | USART_CR1_RE)) return 0;
    while (accepted < size && u->rx_count < PY32_USART_BUFFER_SIZE) {
        size_t tail = (u->rx_head + u->rx_count) % PY32_USART_BUFFER_SIZE;
        u->rx[tail] = data[accepted++];
        ++u->rx_count;
    }
    if (u->rx_count > 0u) u->sr |= USART_SR_RXNE;
    return accepted;
}
