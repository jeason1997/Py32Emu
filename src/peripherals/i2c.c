#include "py32emu/peripherals/i2c.h"

#include <string.h>

enum {
    CR1_PE = 1u, CR1_START = 1u << 8, CR1_STOP = 1u << 9,
    CR1_SWRST = 1u << 15,
    CR2_ITERREN = 1u << 8, CR2_ITEVTEN = 1u << 9, CR2_ITBUFEN = 1u << 10,
    SR1_SB = 1u, SR1_ADDR = 1u << 1, SR1_BTF = 1u << 2,
    SR1_RXNE = 1u << 6, SR1_TXE = 1u << 7,
    SR1_ERRORS = 0x0F00u,
    SR2_MSL = 1u, SR2_BUSY = 1u << 1, SR2_TRA = 1u << 2
};

static bool clocked(const Py32I2c *i2c)
{
    return i2c->clock_enable_register == NULL ||
           (*i2c->clock_enable_register & i2c->clock_enable_mask) != 0u;
}

static uint8_t target_read(Py32I2c *i2c)
{
    uint8_t value = i2c->target_ops != NULL && i2c->target_ops->read != NULL
        ? i2c->target_ops->read(i2c->target_context) : 0xFFu;
    if (i2c->rx_count < PY32_I2C_CAPTURE_SIZE)
        i2c->rx[i2c->rx_count++] = value;
    return value;
}

void py32_i2c_reset(Py32I2c *i2c, const uint32_t *clock_register,
                    uint32_t clock_mask)
{
    memset(i2c, 0, sizeof(*i2c));
    i2c->clock_enable_register = clock_register;
    i2c->clock_enable_mask = clock_mask;
}

void py32_i2c_connect(Py32I2c *i2c, const Py32I2cTargetOps *ops,
                      void *context)
{
    if (i2c == NULL) return;
    i2c->target_ops = ops;
    i2c->target_context = context;
}

bool py32_i2c_read(void *context, uint32_t offset, unsigned size,
                   uint32_t *value)
{
    Py32I2c *i2c = context;
    if ((size != 1u && size != 2u && size != 4u) || value == NULL) return false;
    if (!clocked(i2c)) { *value = 0; return true; }
    switch (offset) {
    case 0x00: *value = i2c->cr1; break;
    case 0x04: *value = i2c->cr2; break;
    case 0x08: *value = i2c->oar1; break;
    case 0x0C: *value = i2c->oar2; break;
    case 0x10:
        if (i2c->reading && !i2c->address_phase) i2c->dr = target_read(i2c);
        *value = i2c->dr;
        break;
    case 0x14: *value = i2c->sr1; i2c->sr1_read = true; break;
    case 0x18:
        *value = i2c->sr2;
        if (i2c->sr1_read && (i2c->sr1 & SR1_ADDR) != 0u) {
            i2c->sr1 &= ~SR1_ADDR;
            i2c->address_phase = false;
            i2c->sr1 |= i2c->reading ? (SR1_RXNE | SR1_BTF)
                                     : (SR1_TXE | SR1_BTF);
        }
        i2c->sr1_read = false;
        break;
    case 0x1C: *value = i2c->ccr; break;
    case 0x20: *value = i2c->trise; break;
    default: return false;
    }
    if (size == 1u) *value &= 0xFFu;
    else if (size == 2u) *value &= 0xFFFFu;
    return true;
}

bool py32_i2c_write(void *context, uint32_t offset, unsigned size,
                    uint32_t value)
{
    Py32I2c *i2c = context;
    bool acknowledged;
    (void)size;
    if (size != 1u && size != 2u && size != 4u) return false;
    if (!clocked(i2c)) return true;
    switch (offset) {
    case 0x00:
        if ((value & CR1_SWRST) != 0u) {
            const uint32_t *clock = i2c->clock_enable_register;
            uint32_t mask = i2c->clock_enable_mask;
            const Py32I2cTargetOps *ops = i2c->target_ops;
            void *target = i2c->target_context;
            py32_i2c_reset(i2c, clock, mask);
            py32_i2c_connect(i2c, ops, target);
            i2c->cr1 = CR1_SWRST;
            return true;
        }
        i2c->cr1 = value & ~(CR1_START | CR1_STOP);
        if ((value & CR1_START) != 0u && (value & CR1_PE) != 0u) {
            i2c->address_phase = true;
            i2c->sr1 = SR1_SB;
            i2c->sr2 = SR2_MSL | SR2_BUSY;
        }
        if ((value & CR1_STOP) != 0u) {
            if (i2c->target_ops != NULL && i2c->target_ops->stop != NULL)
                i2c->target_ops->stop(i2c->target_context);
            i2c->sr2 &= ~(SR2_MSL | SR2_BUSY | SR2_TRA);
            i2c->sr1 &= ~(SR1_SB | SR1_ADDR | SR1_BTF | SR1_RXNE | SR1_TXE);
            i2c->address_phase = false;
        }
        break;
    case 0x04: i2c->cr2 = value; break;
    case 0x08: i2c->oar1 = value; break;
    case 0x0C: i2c->oar2 = value; break;
    case 0x10:
        i2c->dr = value & 0xFFu;
        if ((i2c->sr1 & SR1_SB) != 0u) {
            i2c->address = (uint8_t)(value >> 1);
            i2c->reading = (value & 1u) != 0u;
            acknowledged = i2c->target_ops == NULL || i2c->target_ops->start == NULL ||
                i2c->target_ops->start(i2c->target_context, i2c->address, i2c->reading);
            i2c->sr1 &= ~SR1_SB;
            if (acknowledged) i2c->sr1 |= SR1_ADDR;
            else i2c->sr1 |= 1u << 10;
            if (!i2c->reading) i2c->sr2 |= SR2_TRA;
        } else if (!i2c->reading) {
            if (i2c->tx_count < PY32_I2C_CAPTURE_SIZE)
                i2c->tx[i2c->tx_count++] = (uint8_t)value;
            if (i2c->target_ops != NULL && i2c->target_ops->write != NULL &&
                !i2c->target_ops->write(i2c->target_context, (uint8_t)value))
                i2c->sr1 |= 1u << 10;
            i2c->sr1 |= SR1_TXE | SR1_BTF;
        }
        break;
    case 0x14: i2c->sr1 &= value | ~(SR1_ERRORS); break;
    case 0x18: i2c->sr2 = value; break;
    case 0x1C: i2c->ccr = value; break;
    case 0x20: i2c->trise = value; break;
    default: return false;
    }
    return true;
}

bool py32_i2c_irq_pending(const Py32I2c *i2c)
{
    uint32_t events;
    if (i2c == NULL || !clocked(i2c) || (i2c->cr1 & CR1_PE) == 0u) return false;
    events = i2c->sr1 & (SR1_SB | SR1_ADDR | SR1_BTF | SR1_RXNE | SR1_TXE);
    return ((i2c->cr2 & CR2_ITERREN) != 0u && (i2c->sr1 & SR1_ERRORS) != 0u) ||
           ((i2c->cr2 & CR2_ITEVTEN) != 0u && events != 0u) ||
           ((i2c->cr2 & CR2_ITBUFEN) != 0u &&
            (i2c->sr1 & (SR1_RXNE | SR1_TXE)) != 0u);
}
