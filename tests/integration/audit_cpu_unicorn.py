"""可选 ARMv6-M 单步差分审查；Unicorn 仅为辅助对照，规范以 Arm 手册为准。"""

import ctypes
import json
from pathlib import Path
import random
import subprocess

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn import arm_const as arm

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build/cpu-audit"
CODE, RAM = 0x08000000, 0x20000000
REGS = [getattr(arm, f"UC_ARM_REG_R{i}") for i in range(13)] + [
    arm.UC_ARM_REG_SP, arm.UC_ARM_REG_LR, arm.UC_ARM_REG_PC]
BOUNDARIES = [0, 1, 31, 32, 33, 255, 256, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    library = BUILD / "cpu.so"
    subprocess.run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-O2",
                    "-shared", "-fPIC", "-Iinclude", "src/core/cortex_m0.c",
                    "src/core/bus.c", "tests/unit/cpu_audit_bridge.c", "-o", str(library)],
                   cwd=ROOT, check=True)
    bridge = ctypes.CDLL(str(library)).cpu_audit_step
    bridge.argtypes = [ctypes.POINTER(ctypes.c_uint16), ctypes.POINTER(ctypes.c_uint32),
                       ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint8)]
    bridge.restype = ctypes.c_int
    oracle = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    oracle.ctl_set_cpu_model(arm.UC_CPU_ARM_CORTEX_M0)
    oracle.mem_map(CODE - 4096, 3 * 4096)
    oracle.mem_map(RAM, 4096)
    rng = random.Random(0x32F002A)
    failures, counts = {}, {}

    def check(group, op, registers=None, data=None, second=0):
        regs = registers if registers is not None else [
            rng.choice(BOUNDARIES) if rng.randrange(2) else rng.getrandbits(32)
            for _ in range(16)]
        regs = list(regs)
        regs[13], regs[15] = RAM + 2048, CODE
        initial_flags = rng.randrange(16) << 28 | 1 << 24
        initial_memory = data if data is not None else bytes(4096)
        c_regs = (ctypes.c_uint32 * 16)(*regs)
        c_flags = ctypes.c_uint32(initial_flags)
        c_memory = (ctypes.c_uint8 * 4096).from_buffer_copy(initial_memory)
        result = bridge((ctypes.c_uint16 * 2)(op, second), c_regs,
                        ctypes.byref(c_flags), c_memory)
        oracle.mem_write(CODE, op.to_bytes(2, "little") + second.to_bytes(2, "little"))
        oracle.ctl_remove_cache(CODE, CODE + 16)
        oracle.mem_write(RAM, initial_memory)
        oracle.reg_write(arm.UC_ARM_REG_CONTROL, 0)
        oracle.reg_write(arm.UC_ARM_REG_PSP, 0)
        oracle.reg_write(arm.UC_ARM_REG_PRIMASK, 0)
        for register, value in zip(REGS, regs):
            oracle.reg_write(register, value)
        oracle.reg_write(arm.UC_ARM_REG_XPSR, initial_flags)
        oracle.emu_start(CODE | 1, CODE + 4096, count=1)
        expected = [oracle.reg_read(register) for register in REGS]
        expected_flags = oracle.reg_read(arm.UC_ARM_REG_XPSR) & 0xF0000000
        actual = list(c_regs)
        counts[group] = counts.get(group, 0) + 1
        if (result or actual != expected or c_flags.value & 0xF0000000 != expected_flags
                or bytes(c_memory) != bytes(oracle.mem_read(RAM, 4096))):
            failure = failures.setdefault(group, {"count": 0, "first": {
                "op": hex(op), "second": hex(second), "input": regs,
                "flags": hex(initial_flags), "actual": actual, "expected": expected,
                "actual_flags": hex(c_flags.value), "expected_flags": hex(expected_flags)}})
            failure["count"] += 1

    # 全部低寄存器 ALU 编码，每种再换一次输入；含移位、ADC/SBC、比较和乘法。
    for op in range(0x4400):
        for _ in range(2):
            check("alu-low", op)
    for kind in range(3):
        for destination in list(range(13)) + [14]:
            for source in list(range(13)) + [14, 15]:
                if kind == 1 and destination < 8 and source < 8:
                    continue  # 高寄存器 CMP 中该编码组合不可预测。
                if kind == 1 and source == 15:
                    continue
                op = 0x4400 | (kind << 8) | ((destination & 8) << 4) | (source << 3) | (destination & 7)
                check("high-register-pc", op)
    for op in list(range(0xB200, 0xB300)) + list(range(0xBA00, 0xBA80)) + list(range(0xBAC0, 0xBB00)):
        check("extend-reverse", op)
    for base in range(8):
        for mask in range(1, 256):
            regs = [rng.getrandbits(32) for _ in range(16)]
            regs[base] = RAM + 128
            data = rng.randbytes(4096)
            check("ldm", 0xC800 | base << 8 | mask, regs, data)
            if not mask & 1 << base or not mask & ((1 << base) - 1):
                check("stm-defined", 0xC000 | base << 8 | mask, regs, data)
    for op in range(0x5000, 0x6000):
        base, offset = (op >> 3) & 7, (op >> 6) & 7
        if base == offset:
            continue
        regs = [rng.getrandbits(32) for _ in range(16)]
        regs[base], regs[offset] = RAM + 128, rng.randrange(16) * 4
        check("memory-register", op, regs, rng.randbytes(4096))
    for op in range(0x6000, 0xA000):
        regs = [rng.getrandbits(32) for _ in range(16)]
        if op < 0x9000:
            regs[(op >> 3) & 7] = RAM + 128
        check("memory-immediate-sp", op, regs, rng.randbytes(4096))
    for op in range(0xA000, 0xB100):
        check("address-sp", op)
    for op in list(range(0xD000, 0xDE00)) + list(range(0xE000, 0xE800)):
        check("branch-16", op)
    for mask in range(1, 512):
        check("push", 0xB400 | mask)
        data = bytearray(rng.randbytes(4096))
        if mask & 256:
            offset = 2048 + (mask & 255).bit_count() * 4
            data[offset:offset + 4] = (CODE + 0x101).to_bytes(4, "little")
        check("pop", 0xBC00 | mask, data=bytes(data))
    for displacement in range(-2048, 2048, 2):
        immediate = displacement & 0x1FFFFFF
        sign = immediate >> 24
        j1 = 1 ^ ((immediate >> 23 & 1) ^ sign)
        j2 = 1 ^ ((immediate >> 22 & 1) ^ sign)
        first = 0xF000 | sign << 10 | (immediate >> 12 & 1023)
        second = 0xD000 | j1 << 13 | j2 << 11 | (immediate >> 1 & 2047)
        check("bl-32", first, second=second)
    for source in list(range(13)) + [14]:
        for link in [0, 1]:
            regs = [rng.getrandbits(32) for _ in range(16)]
            regs[source] = CODE + 0x101
            check("bx-blx", 0x4700 | link << 7 | source << 3, regs)
    for second in [0x8F4F, 0x8F5F, 0x8F6F]:
        check("barrier", 0xF3BF, second=second)
    for special in [0, 1, 2, 3, 5, 6, 7, 8, 9, 16, 20]:
        check("mrs", 0xF3EF, second=0x8000 | special)
        check("msr", 0xF380, second=0x8800 | special)
    report = {"seed": "0x32f002a", "counts": counts, "failures": failures}
    (BUILD / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
