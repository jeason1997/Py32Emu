/* 可选差分审查适配器；只调用共享 C CPU/总线，不复制指令实现。 */
#include "py32emu/core/cortex_m0.h"

#include <string.h>

int cpu_audit_step(const uint16_t instruction[2], uint32_t registers[16],
                   uint32_t *xpsr, uint8_t memory[4096])
{
    Py32Bus bus;
    CortexM0 cpu;
    uint8_t code[16] = {0};
    unsigned i;
    for (i = 0u; i < 2u; ++i) {
        code[2u * i] = (uint8_t)instruction[i];
        code[2u * i + 1u] = (uint8_t)(instruction[i] >> 8);
    }
    py32_bus_init(&bus);
    if (!py32_bus_add_memory(&bus, "code", 0x08000000u, code, sizeof(code), true) ||
        !py32_bus_add_memory(&bus, "ram", 0x20000000u, memory, 4096u, false))
        return -1;
    py32_bus_set_fetch(&bus, code, 0x08000000u, sizeof(code));
    py32_bus_set_data(&bus, memory, 0x20000000u, 4096u);
    cortex_m0_init(&cpu, &bus);
    memcpy(cpu.r, registers, sizeof(cpu.r));
    cpu.msp = registers[CORTEX_M0_SP];
    cpu.xpsr = *xpsr;
    cortex_m0_step(&cpu);
    memcpy(registers, cpu.r, sizeof(cpu.r));
    *xpsr = cpu.xpsr;
    return cpu.stopped || cpu.exception_number != 0u;
}
