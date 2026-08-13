# Py32Emu 开发进度

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

### 正在进行

- 实现 TIM1 公共计数功能和 I2C 基础模型；继续补全特殊寄存器指令与中断优先级。

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

- CPU 已能执行基础固件，但 Thumb 指令集和特殊寄存器指令尚不完整。
- NVIC/SysTick 已有最小模型，但优先级、嵌套异常和全部 SCB 语义尚未完成。
- RCC/GPIO/USART1 已有首版；USART 波特率时序与中断发送尚未完成。
- 已完成官方 GPIO HAL 例程回归；其他官方外设例程尚未接入。

## 最近验证

```text
wsl make clean && make test
foundation tests passed
执行 18 条，18 周期，PC=0x080000E4，状态=breakpoint
GPIOA: MODER=0xFFFFF7FF ODR=0x0020
official GPIO_Toggle: 3000000 instructions, GPIOA ODR=0x0000
official USART polling: TX = FF FF FF FF FF FF FF FF FF FF FF FF
```

GCC 以 `-std=c11 -Wall -Wextra -Wpedantic` 编译，无警告。
