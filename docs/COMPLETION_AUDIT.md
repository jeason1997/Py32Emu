# PY32F002A 模拟器完成审计

审计日期：2026-08-13

本审计按照最初目标逐项检查当前工作树、自动测试和实机记录。这里的“完成”指首个可用的
PY32F002A 功能级模拟器版本完成，不表示芯片所有保留位、全部外设高级模式或逐周期时序均已
达到门级仿真精度。

## 要求与证据

| 要求 | 当前证据 | 结论 |
|---|---|---|
| 模仿 PicEmu 工程结构 | `include/src/chips/peripherals/firmware/frontends/tests/docs/examples` 分层；CLI 与 Web 共用同一个 C11 SoC | 已满足 |
| 可扩展到 PY32F003 | `Py32ChipDescription` 集中容量、页大小、IRQ 数和外设能力位；SoC 按能力位装配设备 | 已满足扩展接口要求，尚未声称已支持 F003 |
| CPU 与内存 | ARMv6-M Thumb 覆盖表、Flash 双映射、SRAM、系统存储器、Flash 自编程 | 已满足本版本范围 |
| 异常与中断 | SysTick、NVIC 优先级、嵌套抢占、MSP/PSP、SVC、HardFault、WFI/WFE 固件回归 | 已满足常用固件路径 |
| 关键外设 | RCC、GPIO、EXTI、USART、TIM1/16、LPTIM、SPI、I2C、ADC、COMP、CRC、IWDG、FLASH | 已满足 |
| 固件加载 | BIN、Intel HEX、ELF32 ARM 段加载及 ELF 符号解析 | 已满足 |
| 调试界面 | CLI 跟踪；Web 加载/复位/单步/运行/多断点/寄存器/内存/符号反汇编/串口/GPIO/模拟输入 | 已满足 |
| 官方 SDK HelloWorld | 输出 13 字节 `Hello World\r\n` 并由 BKPT 正常停止 | 已满足 |
| 真实例程验证 | 官方 GPIO、USART、TIM1、TIM16、SPI、I2C、ADC、EXTI、CRC、IWDG、FLASH、LPTIM、COMP 例程进入预期成功路径 | 已满足 |
| 实机对照 | DAPLink/OpenOCD 烧录 TIM16 ELF，读取 GPIOA/TIM16 状态与模拟结果一致；确认样片隐藏容量 | 已满足已有对照范围 |
| 持续进度与计划 | `docs/PROGRESS.md` 保留阶段记录、验证结果、后续精化项 | 已满足 |
| 构建与回归 | WSL 下 `make test` 覆盖单元、真实 Cortex-M0+ 固件、官方 SDK、Web 后端和 HTTP 服务 | 已满足 |

## 官方例程验证矩阵

| 模块 | 例程/固件 | 可观察结果 |
|---|---|---|
| GPIO | `GPIO_Toggle` | SysTick/HAL_Delay 驱动 PA5 翻转 |
| USART | `USART_HyperTerminal_Polling` | 捕获 12 个 `FF` |
| HelloWorld | 本仓库官方 HAL 示例 | 捕获 `Hello World\r\n` |
| TIM1/TIM16 | `TIMx_ARR` | 更新 IRQ、PA5 翻转，TIM1 动态改 ARR=6399 |
| SPI | `TwoBoards_FullDuplexMaster_IT` | IRQ25，发送 `01..0F` |
| I2C | `TwoBoard_CommunicationMaster_Polling` | 发送/接收各 15 字节 |
| ADC | `ContinuousConversion...Vrefint` | DR=1489，完成 120 次转换 |
| EXTI | `EXTI_IT` | PB2 下降沿进入 IRQ6 并翻转 PA5 |
| CRC | `CRC_Computing_Results` | DR=`0x379E9F06` |
| IWDG | `IWDG_RESET` + 自定义复位固件 | 官方喂狗无复位；超时触发 SoC 复位和 IWDGRSTF |
| FLASH | `FLASH_PageEraseAndWrite` | 擦 2 页、写 64 字，目标首字=`0x01010101` |
| LPTIM | `LPTIM_WakeUp` | LSI/128、ARR=51，两次 WFI→IRQ17 唤醒 |
| COMP | Polling 与 IT | PA1 1800mV 跨越 VREF，EXTI17→共享 IRQ12 |

## 已知边界与非阻塞精化项

- NVIC 尾链、晚到异常及全部 SCB 保留/调试语义未做逐周期实现。
- 外设模型覆盖常见 HAL 路径，不覆盖每种 DMA、高级定时器波形、总线争用和模拟噪声。
- 时钟切换影响寄存器和所需外设时钟源，但 CPU/外设每条指令的周期数不是硅级校准值。
- 官方 `PWR_SLEEP_WFI`/`PWR_STOP_WFI` 源码操作 GPIOB/PB2 却只使能 GPIOA 时钟，原版例程自身不能形成 PB2 EXTI；低功耗唤醒由自定义 WFI/SysTick 固件和官方 LPTIM 例程覆盖。
- `py32f002a-32k` 是经当前样片确认的独立描述，不代表所有 PY32F002A 都具有隐藏容量。

这些边界不会阻止已列出的官方固件和调试工作流运行，但未来扩展 PY32F003 或新增复杂固件时，
应继续以“官方资料/实机行为 + 对应回归测试”的方式补充模型。
