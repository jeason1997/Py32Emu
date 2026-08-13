# Py32Emu 开发进度

## 2026-08-13：首个可交付版本完成审计

- 新增 `docs/COMPLETION_AUDIT.md`，按原始目标核对结构、扩展接口、CPU、内存、异常/中断、关键外设、加载器、调试界面、官方例程和实机证据。
- README 已更新为当前构建方式、功能范围、CLI/Web 用法、例程结果和精度边界。
- 审计确认官方 PWR 两份例程存在 GPIOB/PB2 与 GPIOA 时钟使能不一致，未向模拟器加入违反时钟门控的兼容特例；WFI 唤醒由真实自测固件和官方 LPTIM 覆盖。
- `--pulse` 现在明确先驱动高电平再在指定步数拉低，保证语义是确定的下降沿，不依赖固件内部上拉。

当前总体完成度：100%（首个目标版本）。WSL `make clean && make test` 已从零重建并通过；后续精化项保留在完成审计中，不影响当前已验证的固件与调试工作流。

## 2026-08-13：COMP 模拟比较与共享中断阶段

- 新增 COMP1/COMP2 成对模型和芯片能力位，支持 CSR/FR、正负输入选择、内部 VREF 分压、VCC、极性、输出与锁定语义。
- EXTI 从 GPIO 0–15 扩展到内部线 17/18，COMP 输出跨越阈值时可按上升/下降沿产生 ADC_COMP 共享 IRQ12。
- SoC、CLI 和 Web 调试台增加 PA1/PA3 毫伏输入接口；CLI 支持 `--analog PA1=1800 --analog-at N`。
- 官方 `COMP_CompareGpioVsVrefint_Polling` 和 `..._IT` 均已在 WSL 运行；PA1 从 0mV 注入 1800mV 后 COMP1 输出变高，中断例程进入官方回调并翻转 PA5。
- 单元测试覆盖 VREFINT 比较、输出转换、EXTI17 pending/W1C 和 IRQ12；Web 后端协议测试覆盖模拟输入注入。

当前总体完成度：约 96%。下一阶段进行目标完成审计，补齐影响典型官方固件运行的剩余缺口，并整理可交付文档。

## 2026-08-13：LPTIM 低功耗唤醒阶段

- 新增可复用 LPTIM1 模型和芯片能力位，支持 ISR/ICR/IER/CFGR/CR/ARR/CNT、单次启动、预分频和 ARRM 中断。
- LPTIM 时钟依据 RCC CCIPR 选择 PCLK、LSI 或 LSE；独立低速时钟会在 CPU 执行 WFI 时继续推进。
- 补充最小 PWR 寄存器窗口，使官方 HAL 可配置 STOP 模式；实际睡眠与唤醒仍由 Cortex-M0+ 和 NVIC 统一处理。
- 官方 `LPTIM_WakeUp` 已在 WSL 编译运行，LSI 32768 Hz 经 128 分频、ARR=51，完成两次 IRQ17 唤醒及 PA5 回调翻转。
- 单元测试覆盖计数时序、匹配标志、W1C 清除和中断请求；官方例程已加入 `make test`。

当前总体完成度：约 94%。下一阶段处理 COMP 输入比较与 ADC_COMP 共享中断，并继续完成最终范围审计。

## 2026-08-13：FLASH 自编程阶段

- 实现 PY32 FLASH 控制器模型，主存储区与 `0x00000000` 别名共享同一份 Flash 数据。
- 支持官方双密钥解锁、重新上锁、SR 写 1 清零、128 字节页擦除、扇区/整片擦除基础语义及 32 位字编程。
- 编程遵循 Flash 的 1→0 物理约束；锁定或模式错误时不修改数据并置位 `WRPERR`。
- 官方 `FLASH_PageEraseAndWrite` 已在 WSL 编译并运行：擦除 2 页、写入 64 个字，`0x08002000` 校验为 `0x01010101`。
- CLI 会报告擦除次数、编程字数、最后操作地址及测试区首字，相关单元/集成测试已加入 `make test`。
- 全量 WSL 串行回归通过；并行执行整个 `make test` 会触发既有 HelloWorld 子 Makefile 的 clean/build 竞态，暂不作为功能故障。

当前总体完成度：约 92%。下一阶段优先接入 LPTIM 或 COMP 官方例程，并继续核对尚未覆盖的芯片系统行为。

最后更新：2026-08-13

## 总目标

加载并执行 PY32F002A 的真实 BIN/HEX/ELF 固件，支持 Cortex-M0+ 指令、异常与
中断，以及官方例程常用片上外设；提供类似 PicEmu 的命令行和 Web 调试体验。

## 当前阶段：阶段 1 - 最小系统基础

### 已完成

- 盘点 PicEmu 分层、构建、测试和文档结构。
- 定位 PY32F002A 中文参考手册 v1.2、数据手册及官方 V1.1.3 固件库。
- 确认官方 CMSIS 头文件中的主要内存映射和中断编号。
- 建立型号描述、内存总线和原始 BIN 固件映像的公共接口。
- 建立首批总线、设备描述与固件加载单元测试。
- 实现 Cortex-M0+ 复位向量、寄存器/xPSR 状态和故障停止机制。
- 实现启动代码所需的首批 Thumb 指令：移位、算术/逻辑、加载存储、栈、
  多寄存器传送、条件/无条件分支、BX、BL、BKPT 和 WFI。
- 建立 `Py32Soc` 装配层，提供 Flash 的 0 地址别名、主 Flash、SRAM 与 CPU。
- 命令行现可真实执行 BIN 固件，支持步数上限和逐指令跟踪。
- 已找到 WSL GCC，并通过无警告构建和基础执行测试。
- 实现 RCC 基础寄存器及 HSI 就绪/时钟选择语义。
- 实现 GPIOA/GPIOB/GPIOF 的模式、上下拉、输入、ODR、BSRR、BRR、复用功能
  和 RCC 时钟门控，并提供外部输入与输出引脚观察接口。
- 实现 SysTick、NVIC 基础寄存器、向量表重定位、异常自动压栈和
  `EXC_RETURN` 自动恢复。
- 使用 `arm-none-eabi-gcc` 生成真实 Cortex-M0+ BIN，已验证该固件可配置
  RCC/GPIOA5 并以 BKPT 正常停止。
- 已直接构建并运行官方 V1.1.3 `GPIO/GPIO_Toggle` HAL 例程；SysTick 中断
  能推进 `HAL_Delay(250)`，主循环已真实翻转 PA5。
- 官方 HAL GPIO 例程已加入 `make test`，测试构建使用项目自带 GCC 启动文件，
  不依赖相邻 PicEmu 工作区。
- 加入 Intel HEX 加载器，支持数据、EOF、扩展线性地址和入口记录，严格检查
  校验和与 Flash 地址范围；CLI 根据 `.hex` 扩展名自动选择加载器。
- 实现 USART1 基础模型：SR/DR/BRR/CR、RCC 门控、即时发送状态、接收 FIFO、
  宿主注入与发送捕获接口。
- 补充 Thumb `SXTB/SXTH/UXTB/UXTH/REV/REV16/REVSH` 指令。
- 已构建并运行官方 `USART_HyperTerminal_Polling`；固件真实发送了十二个
  `0xFF`，随后进入轮询接收等待，结果已纳入自动回归。
- 实现 ELF32 ARM 加载器，按 `PT_LOAD` 物理地址装载 Flash，并解析函数/对象
  符号；CLI 跟踪可显示 `<函数名+偏移>`。官方 GPIO ELF 已加入符号回归。
- 实现 TIM16 基础计数器、预分频、自动重装、更新标志和 IRQ21 请求，首批
  寄存器/计数单元测试通过。
- 官方 `TIM16_ARR` 已在模拟器中产生更新中断并翻转 PA5，纳入自动回归。
- 通过 DAPLink/OpenOCD 在真实 PY32F002A 上烧录同一 ELF；200 ms 后读取到
  `GPIOA_ODR=0`、`TIM16_CNT=0x847`，与模拟器首次更新后 PA5 为低一致。
- 实机 OpenOCD 报告物理 Flash 为 32 KiB，原有固件 MSP 为 `0x20001000`，表明
  当前样片存在 32 KiB/4 KiB 隐藏容量；官方 x5 的 20 KiB/3 KiB 描述保持独立。
- 增加独立的 `py32f002a-32k` 芯片描述和 CLI `--chip` 选择，不改变官方
  `py32f002ax5` 容量边界。
- 新增可直接构建的官方 HAL `examples/hello_world`，模拟器已真实捕获
  `Hello World\r\n` 十三个串口字节并以 BKPT 正常结束，纳入自动回归。
- 实现 SPI1 基础模型：CR1/CR2/SR/DR、RCC 门控、8/16 位传输、发送捕获、
  虚拟从机交换回调和 IRQ25 请求；同时加入 SYSCFG/EXTI 基础寄存器窗口。
- 官方 `SPI_TwoBoards_FullDuplexMaster_IT` 中断主机例程已运行，完整捕获
  `01` 至 `0F`；其暴露的 `PUSH {lr}` 和寄存器形式 `BLX` CPU 缺陷已修复。
- 实现 I2C1 基础模型：传统 CR/SR/DR 寄存器、START/STOP/地址阶段、主机收发、
  RCC 门控、IRQ23 请求、发送/接收捕获和可替换虚拟从机回调接口。
- 官方 `I2C_TwoBoard_CommunicationMaster_Polling` 已完成 15 字节发送与 15 字节
  接收；例程所需的 `CPSID i`/`CPSIE i` 已加入 CPU 并纳入回归。
- 增加 TIM1 独立实例、RCC APB2 门控和 IRQ13 更新请求，复用通用定时器的
  计数、预分频、自动重装和更新标志语义。
- 官方 `TIM1_ARR` 已完成更新中断与 PA5 翻转，并在第三次回调中把 ARR 从
  1599 动态修改为 6399；CLI 可观察 TIM1 的 CNT/PSC/ARR/SR。
- 实现 ADC1 基础模型：13 路宿主可注入采样值、分辨率/对齐、通道选择、校准、
  EOC/EOS/OVR、模拟看门狗、IRQ12 和 ADC common CCR 寄存器窗口。
- 官方 `ADC_ContinuousConversion_TriggerSW_Vrefint` 已完成 HAL 校准、软件启动、
  轮询和取值闭环；默认 VREFINT=1489，十万条指令完成 120 次转换。
- 将 EXTI 从无语义寄存器窗口升级为控制器模型，支持 RTSR/FTSR、SWIER、PR 的
  W1C、EXTICR 端口选择、IMR/EMR，以及 EXTI0_1/2_3/4_15 中断分组。
- GPIO 宿主输入现在会按配置产生边沿；CLI 增加 `--pulse PB2 --pulse-at N` 注入。
  官方 `EXTI_IT` 已验证 PB2 单次下降沿进入 IRQ6、清除 PR 并翻转 PA5 一次。
- NVIC 现按 Cortex-M0+ 实现的高 2 位优先级仲裁，同优先级按异常号决定顺序；
  SysTick 使用 SHPR 优先级，并允许更高优先级异常抢占当前 ISR。
- CPU 增加八层活动异常栈和 Handler/Thread 两种 `EXC_RETURN`（F1/F9），嵌套返回
  可恢复先前异常号；NVIC IABR 与 ICSR VECTACTIVE 可观察当前活动状态。
- 新增真实 Cortex-M0+ 抢占固件，验证 IRQ5 抢占 IRQ6 后依次返回 IRQ6 与线程，
  同时修正 EXTI 源锁存，避免覆盖软件写入的 NVIC ISPR。
- 实现 ARMv6-M `MRS/MSR` 对 APSR/xPSR/IPSR/MSP/PSP/PRIMASK/CONTROL 的基础
  访问，以及 DSB/DMB/ISB；CONTROL.SPSEL 可在 Thread 模式切换可见栈。
- 异常入口会按线程栈选择将硬件帧压入 MSP 或 PSP，Handler 固定使用 MSP；新增
  `EXC_RETURN FD` 返回 PSP。真实双栈固件验证 PSP 入栈 32 字节、ISR 使用 MSP，
  返回线程后 PSP 完整恢复。

- 完成 ARMv6-M Thumb 指令覆盖审计并建立 `docs/THUMB_COVERAGE.md`；补齐 `ROR`，修正
  寄存器移位量为零时的 N/Z 标志更新。
- 实现同步 `SVC` 异常以及 `YIELD/SEV/WFE/WFI` 事件与等待语义；真实 Cortex-M0+ 自测固件
  已验证 SVC handler 和 SysTick 唤醒 WFI。`CBZ/CBNZ`、`IT/ITE` 经工具链确认不属于本目标。

- 将非法异常返回、未定义指令、未对齐访问和未映射总线访问从宿主直接停止改为 Cortex-M0+ HardFault；
  固件可由异常号 3 的 handler 处理并返回，HardFault 内再次故障会按锁死路径终止。
- 总线现在会为未对齐、只读写入、未映射地址和设备拒绝访问统一记录故障地址；新增真实
  Cortex-M0+ 固件连续验证四类 HardFault 后继续运行。

- 新增独立 `py32emu-web-core` 调试后端，直接复用 C11 SoC；支持固件加载、芯片选择、
  复位、单步、批量运行、地址断点、核心状态和内存读取。
- 新增本地 Node 服务与响应式浏览器调试台，可查看 R0-R15、xPSR、CONTROL、异常状态、
  GPIOA/B/F 和 SRAM；后端协议与 HTTP API 均已有 WSL 自动回归。

- Web 调试台新增可复用 Thumb 反汇编器、ELF 符号化 PC 窗口、多地址断点列表、USART1
  收发终端，以及 PA/PB/PF 外部输入的高、低、释放交互。

- `Py32ChipDescription` 新增外设能力位图，SoC 总线装配、外设 IRQ 推进、EXTI 和 GPIO
  输入注入均服从型号描述；Web 状态也会报告能力并隐藏不存在的 GPIO。
- `external_irq_count` 现在真实约束 NVIC 可见、可写和可服务的 IRQ；单测用仅含 GPIOA、
  8 路 IRQ 的裁剪描述验证 GPIOB/USART1/ADC1 不会映射，越界 NVIC 位不会生效。

- 新增可复用 CRC 外设模型：AHB 时钟门控、`DR/IDR/CR.RESET` 和标准
  `0x04C11DB7` 32 位累积；能力位可供未来 PY32F003 描述复用。
- 原版官方 `CRC_Computing_Results` 已在模拟器中处理 114 个 32 位字，得到手册例程期望的
  `0x379E9F06`，并进入板载 LED 低有效的成功分支。

- 新增 IWDG 模型：LSI 32768 Hz、`KR` 启动/解锁/喂狗键、PR 4-256 分频、12 位 RLR、
  写保护和按 CPU 周期换算的独立计数器。
- IWDG 超时现在请求完整 SoC 复位并设置 RCC `IWDGRSTF`；自定义真实固件已验证第二次启动
  观察到复位标志，官方 `IWDG_RESET` 则运行超过一次超时窗口并在 900 ms 处正确喂狗、无复位。

### 正在进行

- 继续接入 LPTIM、COMP、FLASH 擦写等官方 SDK 示例。

### 下一步

1. 完成 Thumb-1 基础数据处理、加载存储和分支指令。
2. 加入 xPSR、MSP/PSP、PRIMASK、异常入栈/返回。
3. 实现 PPB、NVIC 和 SysTick。
4. 实现 RCC 与 GPIO，跑通官方 GPIO 翻转例程。
5. 依次加入 USART、TIM1/TIM16、EXTI、SPI、I2C、ADC 等外设。
6. 增加 Intel HEX/ELF、反汇编、断点、寄存器和内存查看。
7. 仿照 PicEmu 建立 Web 电路实验台并运行官方例程回归测试。

## 资料基线

- `PY32F002A Reference manual v1.2.pdf`
- `PY32F002A 系列数据手册 Rev1.0.pdf`
- `PY32F0xx_Firmware_V1.1.3`
- CMSIS 设备头文件 `py32f002ax5.h`

当前按 CMSIS 的 PY32F002Ax5 描述采用 20 KiB Flash（`0x08000000`）和 3 KiB
SRAM（`0x20000000`）。其他容量后缀将在获得对应器件目标后作为独立描述加入。

## 尚未完成/未验证

- CPU 已支持常用特殊寄存器和双栈异常，但 Thumb 指令覆盖仍需系统审计。
- NVIC/SysTick 已支持优先级和嵌套异常；尾链、晚到中断和全部 SCB 语义尚未完成。
- RCC/GPIO/EXTI/USART1/SPI1/I2C1/TIM1/TIM16/ADC1 已有首版；总线时序、错误注入与部分中断
  语义仍需完善。
- 已接入官方 GPIO、EXTI、USART、TIM1、TIM16、SPI、I2C、ADC HAL 例程。

## 最近验证

```text
wsl make clean && make test
foundation tests passed
执行 18 条，18 周期，PC=0x080000E4，状态=breakpoint
GPIOA: MODER=0xFFFFF7FF ODR=0x0020
official GPIO_Toggle: 3000000 instructions, GPIOA ODR=0x0000
official USART polling: TX = FF FF FF FF FF FF FF FF FF FF FF FF
official SPI interrupt master: TX = 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
official I2C polling master: TX/RX = 15/15 bytes
official TIM1_ARR: PA5 low, ARR changed to 6399
official ADC VREFINT polling: DR=1489, conversions=120
official EXTI_IT: PB2 falling edge -> IRQ6 -> PA5 low
NVIC preemption firmware: IRQ6 -> IRQ5 -> IRQ6 -> thread, breakpoint
dual-stack firmware: Thread PSP -> IRQ on MSP -> EXC_RETURN FD -> PSP restored
```

GCC 以 `-std=c11 -Wall -Wextra -Wpedantic` 编译，无警告。
