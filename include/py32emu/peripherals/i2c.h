#ifndef PY32EMU_PERIPHERALS_I2C_H
#define PY32EMU_PERIPHERALS_I2C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PY32_I2C_CAPTURE_SIZE 4096u

typedef struct {
    bool (*start)(void *context, uint8_t address, bool read);
    bool (*write)(void *context, uint8_t value);
    uint8_t (*read)(void *context);
    void (*stop)(void *context);
} Py32I2cTargetOps;

typedef struct {
    uint32_t cr1, cr2, oar1, oar2, dr, sr1, sr2, ccr, trise;
    uint8_t address;
    bool reading;
    bool address_phase;
    bool sr1_read;
    uint8_t tx[PY32_I2C_CAPTURE_SIZE];
    size_t tx_count;
    uint8_t rx[PY32_I2C_CAPTURE_SIZE];
    size_t rx_count;
    const Py32I2cTargetOps *target_ops;
    void *target_context;
    const uint32_t *clock_enable_register;
    uint32_t clock_enable_mask;
} Py32I2c;

void py32_i2c_reset(Py32I2c *i2c, const uint32_t *clock_register,
                    uint32_t clock_mask);
void py32_i2c_connect(Py32I2c *i2c, const Py32I2cTargetOps *ops,
                      void *context);
bool py32_i2c_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value);
bool py32_i2c_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value);
bool py32_i2c_irq_pending(const Py32I2c *i2c);

#endif
