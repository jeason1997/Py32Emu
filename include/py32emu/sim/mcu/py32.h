#ifndef PY32EMU_SIM_MCU_PY32_H
#define PY32EMU_SIM_MCU_PY32_H

#include "py32emu/chips/soc.h"
#include "py32emu/sim/mcu.h"

typedef struct {
    SimMcu base;
    Py32Soc *soc;
    SimLevel last_pa0, last_pa1;
} Py32Mcu;

/* 把 Py32Soc 接到通用电路层；clock_hz 是 step() 返回周期的时基。 */
void py32_mcu_init(Py32Mcu *adapter, Py32Soc *soc, uint32_t clock_hz);

#endif
