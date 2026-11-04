# ARMv6-M Thumb 指令覆盖

最后更新：2026-11-04

PY32F002A 使用 Cortex-M0+，模拟器以 ARMv6-M Thumb 指令集为边界。覆盖审计以芯片目标
和 `arm-none-eabi` 对 `-mcpu=cortex-m0plus` 的实际汇编结果为准，不引入更高架构指令。

## 已实现

- 算术和逻辑：`ADC`、`ADD`、`AND`、`ASR`、`BIC`、`CMN`、`CMP`、`EOR`、
  `LSL`、`LSR`、`MOV`、`MUL`、`MVN`、`NEG/RSB`、`ORR`、`ROR`、`SBC`、
  `SUB`、`TST`。
- 数据搬运和扩展：`LDR/LDRB/LDRH/LDRSB/LDRSH`、`STR/STRB/STRH`、
  `LDM/STM`、`PUSH/POP`、`REV/REV16/REVSH`、`SXTB/SXTH/UXTB/UXTH`。
- 地址和控制流：`ADR`、`B`、条件分支、`BL`、`BLX`、`BX`。
- 系统和异常：`BKPT`、`CPSID/CPSIE`、`MRS/MSR`、`SVC`、`DMB/DSB/ISB`、
  `NOP`、`YIELD`、`WFE`、`WFI`、`SEV`。

`tests/firmware/thumb_control_flow.*` 使用真实 Cortex-M0+ 工具链验证 `ROR`、`SVC`、
事件提示以及由 SysTick 中断唤醒 `WFI`。其他指令由基础测试和各官方 SDK 固件回归共同覆盖。

2026-11-04 第三方固件联调发现，寄存器形式 `STRH/LDRH`（`0x5000` 编码空间）曾被
`0xF200` 掩码误排除而进入未定义指令；已改为按 `0xF000` 分类，并由
`tests/unit/test_foundation.c` 使用真实编码覆盖 `STRH/LDRH` 寄存器与立即数偏移形式。

## 架构边界

`CBZ/CBNZ` 和 `IT/ITE` 不属于本项目采用的 ARMv6-M Cortex-M0+ 汇编目标；GNU ARM
工具链会在 `-mcpu=cortex-m0plus` 下拒绝这些指令，因此不作为 PY32F002A 缺失项。

## 后续精化

- 增加逐类的边界值和 APSR 标志测试，而不仅是固件路径覆盖。
- 非法异常返回、未定义指令、未对齐访问和未映射总线访问已进入可由固件接管的 HardFault；HardFault
  处理期间再次故障会进入终止状态，避免异常递归。
- 补充更多异常入栈失败测试。
- 校准不同指令、分支和异常路径的周期数。
