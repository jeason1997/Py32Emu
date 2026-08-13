#include "py32emu/peripherals/spi.h"

#include <string.h>

enum { SPI_CR1_SPE = 1u << 6, SPI_CR1_DFF = 1u << 11,
       SPI_CR2_RXNEIE = 1u << 6, SPI_CR2_TXEIE = 1u << 7,
       SPI_SR_RXNE = 1u, SPI_SR_TXE = 1u << 1 };

static bool clocked(const Py32Spi *spi)
{
    return spi->clock_enable_register == NULL ||
           (*spi->clock_enable_register & spi->clock_enable_mask) != 0u;
}

void py32_spi_reset(Py32Spi *spi, const uint32_t *clock_register,
                    uint32_t clock_mask)
{
    memset(spi, 0, sizeof(*spi));
    spi->sr = SPI_SR_TXE;
    spi->clock_enable_register = clock_register;
    spi->clock_enable_mask = clock_mask;
}

void py32_spi_connect(Py32Spi *spi, Py32SpiExchangeFn exchange, void *context)
{
    if (spi == NULL) return;
    spi->exchange = exchange;
    spi->exchange_context = context;
}

bool py32_spi_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value)
{
    Py32Spi *spi = context;
    if (size != 1u && size != 2u && size != 4u) return false;
    if (!clocked(spi)) { *value = 0; return true; }
    switch (offset) {
    case 0x00: *value = spi->cr1; break;
    case 0x04: *value = spi->cr2; break;
    case 0x08: *value = spi->sr; break;
    case 0x0C:
        *value = spi->dr;
        spi->sr &= ~SPI_SR_RXNE;
        break;
    default: return false;
    }
    if (size == 1u) *value &= 0xFFu;
    else if (size == 2u) *value &= 0xFFFFu;
    return true;
}

bool py32_spi_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value)
{
    Py32Spi *spi = context;
    unsigned bits, frames, frame;
    uint16_t response = 0;
    if (size != 1u && size != 2u && size != 4u) return false;
    if (!clocked(spi)) return true;
    switch (offset) {
    case 0x00: spi->cr1 = value; break;
    case 0x04: spi->cr2 = value; break;
    case 0x08: spi->sr &= value; spi->sr |= SPI_SR_TXE; break;
    case 0x0C:
        if ((spi->cr1 & SPI_CR1_SPE) == 0u) return true;
        bits = (spi->cr1 & SPI_CR1_DFF) != 0u ? 16u : 8u;
        frames = bits == 8u ? size : 1u;
        if (frames > 2u) frames = 2u;
        for (frame = 0; frame < frames; ++frame) {
            uint16_t output = bits == 8u
                ? (uint16_t)((value >> (frame * 8u)) & 0xFFu)
                : (uint16_t)value;
            uint16_t input;
            if (spi->tx_count < PY32_SPI_CAPTURE_SIZE)
                spi->tx[spi->tx_count++] = (uint8_t)output;
            input = spi->exchange != NULL
                ? spi->exchange(spi->exchange_context, output, bits)
                : (uint16_t)(bits == 16u ? 0xFFFFu : 0xFFu);
            response |= (uint16_t)(input << (frame * 8u));
        }
        spi->dr = response;
        spi->sr |= SPI_SR_TXE | SPI_SR_RXNE;
        break;
    default: return false;
    }
    return true;
}

bool py32_spi_irq_pending(const Py32Spi *spi)
{
    if (spi == NULL || !clocked(spi) || (spi->cr1 & SPI_CR1_SPE) == 0u)
        return false;
    return ((spi->cr2 & SPI_CR2_RXNEIE) != 0u &&
            (spi->sr & SPI_SR_RXNE) != 0u) ||
           ((spi->cr2 & SPI_CR2_TXEIE) != 0u &&
            (spi->sr & SPI_SR_TXE) != 0u);
}
