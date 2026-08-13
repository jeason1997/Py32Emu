#ifndef PY32EMU_PERIPHERALS_SPI_H
#define PY32EMU_PERIPHERALS_SPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PY32_SPI_CAPTURE_SIZE 4096u

typedef uint16_t (*Py32SpiExchangeFn)(void *context, uint16_t output,
                                      unsigned bits);

typedef struct {
    uint32_t cr1, cr2, sr, dr;
    uint8_t tx[PY32_SPI_CAPTURE_SIZE];
    size_t tx_count;
    Py32SpiExchangeFn exchange;
    void *exchange_context;
    const uint32_t *clock_enable_register;
    uint32_t clock_enable_mask;
} Py32Spi;

void py32_spi_reset(Py32Spi *spi, const uint32_t *clock_register,
                    uint32_t clock_mask);
void py32_spi_connect(Py32Spi *spi, Py32SpiExchangeFn exchange, void *context);
bool py32_spi_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value);
bool py32_spi_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value);
bool py32_spi_irq_pending(const Py32Spi *spi);

#endif

