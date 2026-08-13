#ifndef PY32EMU_CORE_CORTEX_M0_H
#define PY32EMU_CORE_CORTEX_M0_H

#include "py32emu/core/bus.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    CORTEX_M0_SP = 13,
    CORTEX_M0_LR = 14,
    CORTEX_M0_PC = 15
};

enum {
    CORTEX_M0_XPSR_N = 31,
    CORTEX_M0_XPSR_Z = 30,
    CORTEX_M0_XPSR_C = 29,
    CORTEX_M0_XPSR_V = 28,
    CORTEX_M0_XPSR_T = 24
};

typedef enum {
    CORTEX_M0_STOP_NONE,
    CORTEX_M0_STOP_BKPT,
    CORTEX_M0_STOP_WFI,
    CORTEX_M0_STOP_BUS_FAULT,
    CORTEX_M0_STOP_UNDEFINED
} CortexM0StopReason;

typedef struct {
    uint32_t r[16];
    uint32_t xpsr;
    uint32_t msp;
    uint32_t psp;
    uint32_t control;
    uint32_t vector_table;
    unsigned exception_number;
    bool primask;
    bool stopped;
    CortexM0StopReason stop_reason;
    uint16_t last_instruction;
    uint32_t last_pc;
    uint64_t cycles;
    Py32Bus *bus;
} CortexM0;

typedef struct {
    uint32_t executed_pc;
    uint16_t instruction;
    unsigned halfwords;
    unsigned cycles;
    CortexM0StopReason stop_reason;
} CortexM0StepResult;

void cortex_m0_init(CortexM0 *cpu, Py32Bus *bus);
bool cortex_m0_reset(CortexM0 *cpu, uint32_t vector_base);
CortexM0StepResult cortex_m0_step(CortexM0 *cpu);
bool cortex_m0_enter_exception(CortexM0 *cpu, unsigned exception_number);
const char *cortex_m0_stop_reason_name(CortexM0StopReason reason);

#endif
