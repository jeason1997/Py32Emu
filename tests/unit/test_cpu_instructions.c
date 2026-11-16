#include "py32emu/core/cortex_m0.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CODE 0x08000000u
#define RAM 0x20000000u

static Py32Bus bus;
static CortexM0 cpu;
static uint8_t code[16], memory[256];

static void put32(uint8_t *p, uint32_t value)
{
    unsigned i;
    for (i = 0u; i < 4u; ++i) p[i] = (uint8_t)(value >> (8u * i));
}

static void prepare(uint16_t instruction)
{
    unsigned i;
    memset(code, 0, sizeof(code));
    memset(memory, 0, sizeof(memory));
    code[0] = (uint8_t)instruction;
    code[1] = (uint8_t)(instruction >> 8);
    py32_bus_init(&bus);
    assert(py32_bus_add_memory(&bus, "code", CODE, code, sizeof(code), true));
    assert(py32_bus_add_memory(&bus, "ram", RAM, memory, sizeof(memory), false));
    cortex_m0_init(&cpu, &bus);
    cpu.vector_table = CODE;
    cpu.r[CORTEX_M0_PC] = CODE;
    cpu.r[CORTEX_M0_SP] = RAM + 128u;
    cpu.msp = cpu.r[CORTEX_M0_SP];
    cpu.xpsr = 0xB1000000u;
    for (i = 0u; i < 8u; ++i) cpu.r[i] = 0x10203040u + i;
}

static void test_multiple_transfer(void)
{
    unsigned base, mask, reg, count;
    for (base = 0u; base < 8u; ++base) {
        for (mask = 1u; mask < 256u; ++mask) {
            prepare((uint16_t)(0xC800u | (base << 8) | mask));
            cpu.r[base] = RAM + 32u;
            for (reg = 0u; reg < 8u; ++reg)
                put32(memory + 32u + reg * 4u, 0xAABB0000u + reg);
            cortex_m0_step(&cpu);
            count = 0u;
            for (reg = 0u; reg < 8u; ++reg) {
                if (mask & (1u << reg))
                    assert(cpu.r[reg] == 0xAABB0000u + count++);
                else if (reg != base)
                    assert(cpu.r[reg] == 0x10203040u + reg);
            }
            if ((mask & (1u << base)) == 0u)
                assert(cpu.r[base] == RAM + 32u + count * 4u);
            assert(cpu.xpsr == 0xB1000000u && !cpu.stopped);

            /* STM 的基址入列表只有它为最低编号时才是确定的架构行为。 */
            if ((mask & (1u << base)) && (mask & ((1u << base) - 1u)))
                continue;
            prepare((uint16_t)(0xC000u | (base << 8) | mask));
            cpu.r[base] = RAM + 32u;
            cortex_m0_step(&cpu);
            count = 0u;
            for (reg = 0u; reg < 8u; ++reg) if (mask & (1u << reg)) {
                uint32_t actual;
                assert(py32_bus_read(&bus, RAM + 32u + count++ * 4u, 4u, &actual));
                assert(actual == (reg == base ? RAM + 32u : 0x10203040u + reg));
            }
            assert(cpu.r[base] == RAM + 32u + count * 4u);
            assert(cpu.xpsr == 0xB1000000u && !cpu.stopped);
        }
    }
}

static void test_pc_operands(void)
{
    prepare(0x4678u); /* MOV r0, pc */
    cortex_m0_step(&cpu);
    assert(cpu.r[0] == CODE + 4u);
    assert(cpu.xpsr == 0xB1000000u);
    prepare(0x44F8u); /* ADD r8, pc */
    cpu.r[8] = 100u;
    cortex_m0_step(&cpu);
    assert(cpu.r[8] == CODE + 104u);
    prepare(0x4487u); /* ADD pc, r0 */
    cpu.r[0] = 8u;
    cortex_m0_step(&cpu);
    assert(cpu.r[15] == CODE + 12u);
    prepare(0x4685u); /* MOV sp, r0 */
    cpu.r[0] = RAM + 43u;
    cortex_m0_step(&cpu);
    assert(cpu.r[13] == RAM + 40u);
}

static void system_instruction(uint16_t first, uint16_t second)
{
    prepare(first);
    code[2] = (uint8_t)second;
    code[3] = (uint8_t)(second >> 8);
}

static void test_special_registers(void)
{
    unsigned special;
    for (special = 0u; special < 8u; ++special) {
        uint32_t expected;
        if (special == 4u) continue;
        system_instruction(0xF3EFu, (uint16_t)(0x8000u | special));
        cpu.exception_number = 16u;
        cpu.xpsr |= 16u;
        expected = (special < 4u ? 0xB0000000u : 0u) |
                   ((special & 1u) ? 16u : 0u);
        cortex_m0_step(&cpu);
        assert(cpu.r[0] == expected); /* EPSR.T 通过 MRS 读取必须为零。 */
        system_instruction(0xF380u, (uint16_t)(0x8800u | special));
        cpu.r[0] = 0x410001FFu;
        cortex_m0_step(&cpu);
        assert(cpu.xpsr == (special < 4u ? 0x41000000u : 0xB1000000u));
        assert(!cpu.stopped && cpu.exception_number == 0u);
    }
    system_instruction(0xF3EFu, 0x8008u); /* MRS r0, MSP，非特权读取为零。 */
    cpu.control = 1u;
    cortex_m0_step(&cpu);
    assert(cpu.r[0] == 0u);
    system_instruction(0xF380u, 0x8814u); /* 非特权线程不能清除 nPRIV。 */
    cpu.control = 1u;
    cpu.r[0] = 0u;
    cortex_m0_step(&cpu);
    assert(cpu.control == 1u);
    prepare(0xB672u); /* CPSID i，非特权写入被忽略。 */
    cpu.control = 1u;
    cortex_m0_step(&cpu);
    assert(!cpu.primask);
    system_instruction(0xF380u, 0x8814u); /* Handler 仅忽略 SPSEL，不忽略 nPRIV。 */
    cpu.exception_number = 16u;
    cpu.r[0] = 3u;
    cortex_m0_step(&cpu);
    assert(cpu.control == 1u && cpu.r[13] == RAM + 128u);
}

static void test_fault_aborts_transfer(void)
{
    prepare(0xCA03u); /* LDMIA r2!, {r0,r1}，第二次读取失败。 */
    put32(code + 12u, CODE + 9u);
    cpu.r[2] = RAM + 252u;
    cortex_m0_step(&cpu);
    assert(cpu.exception_number == 3u);
    assert(cpu.r[2] == RAM + 252u);
    prepare(0xBD03u); /* POP {r0,r1,pc}，读取 PC 时失败。 */
    put32(code + 12u, CODE + 9u);
    cpu.r[13] = cpu.msp = RAM + 248u;
    cortex_m0_step(&cpu);
    assert(cpu.exception_number == 3u);
    assert(cpu.r[13] == RAM + 216u);
    assert(cpu.r[15] == CODE + 8u);
}

static void test_literal_and_backward_branch(void)
{
    prepare(0x4800u); /* LDR r0, [pc,#0] */
    put32(code + 4u, 0x12345678u);
    cortex_m0_step(&cpu);
    assert(cpu.r[0] == 0x12345678u);
    prepare(0xE7FEu); /* B . */
    cortex_m0_step(&cpu);
    assert(cpu.r[15] == CODE);
    prepare(0xD1FEu); /* BNE . */
    cortex_m0_step(&cpu);
    assert(cpu.r[15] == CODE);
    prepare(0xD0FEu); /* BEQ . 不跳转 */
    cortex_m0_step(&cpu);
    assert(cpu.r[15] == CODE + 2u);
}

static void test_exception_stack_alignment(void)
{
    uint32_t stacked;
    prepare(0xBF00u);
    put32(code + 12u, CODE + 9u);
    code[8] = 0x70u; code[9] = 0x47u; /* BX LR */
    cpu.r[13] = cpu.msp = RAM + 124u;
    assert(cortex_m0_enter_exception(&cpu, 3u));
    assert(cpu.r[13] == RAM + 88u); /* 栈帧必须 8 字节对齐。 */
    assert(py32_bus_read(&bus, RAM + 116u, 4u, &stacked));
    assert((stacked & (1u << 9)) != 0u);
    cortex_m0_step(&cpu);
    assert(cpu.exception_number == 0u && cpu.r[13] == RAM + 124u);
    assert(cpu.xpsr == 0xB1000000u && cpu.event_register);

    prepare(0xBF00u);
    put32(code + 12u, CODE + 9u);
    code[8] = 0x70u; code[9] = 0x47u;
    cpu.control = 2u;
    cpu.psp = cpu.r[13] = RAM + 124u;
    assert(cortex_m0_enter_exception(&cpu, 3u));
    assert(cpu.psp == RAM + 88u && cpu.r[13] == RAM + 128u);
    assert((cpu.control & 2u) == 0u && cpu.r[14] == 0xFFFFFFFDu);
    cortex_m0_step(&cpu);
    assert(cpu.control == 2u && cpu.r[13] == RAM + 124u);

    prepare(0xBF00u);
    put32(code + 12u, CODE + 9u);
    code[8] = 0x70u; code[9] = 0x47u;
    assert(cortex_m0_enter_exception(&cpu, 3u));
    put32(memory + 124u, 0u); /* 非法 T 状态不能被模拟器强制改成合法。 */
    cortex_m0_step(&cpu);
    assert((cpu.xpsr & (1u << 24)) == 0u);
    cortex_m0_step(&cpu);
    assert(cpu.exception_number == 3u);
}

static unsigned write_count;
static uint32_t write_offsets[3];

static bool record_write(void *context, uint32_t offset, unsigned size, uint32_t value)
{
    (void)context;
    (void)value;
    assert(size == 4u && write_count < 3u);
    write_offsets[write_count++] = offset;
    return true;
}

static void test_push_access_order(void)
{
    prepare(0xB503u); /* PUSH {r0,r1,lr} 的访问顺序为地址递增。 */
    assert(py32_bus_add_device(&bus, "trace", 0x40000000u, 32u,
                               NULL, record_write, NULL));
    cpu.r[13] = cpu.msp = 0x40000010u;
    write_count = 0u;
    cortex_m0_step(&cpu);
    assert(write_count == 3u && write_offsets[0] == 4u &&
           write_offsets[1] == 8u && write_offsets[2] == 12u);
}

int main(void)
{
    test_multiple_transfer();
    test_pc_operands();
    test_special_registers();
    test_fault_aborts_transfer();
    test_literal_and_backward_branch();
    test_exception_stack_alignment();
    test_push_access_order();
    puts("CPU instruction regression tests passed.");
    return 0;
}
