# Py32Emu

Py32Emu 是使用 C11 编写的 PY32 Cortex-M0+ 固件模拟器。当前目标型号是
PY32F002A；CPU、总线和外设模型不保存具体型号容量，芯片描述负责选择 Flash/SRAM
大小、IRQ 数量和实际装配的外设，为后续 PY32F003 等型号保留扩展入口。

当前版本可以直接加载由官方 SDK 和 `arm-none-eabi-gcc` 生成的 BIN、Intel HEX
与 ELF 固件，并已运行 HelloWorld 及多项官方 HAL 例程。详细证据见
[完成审计](docs/COMPLETION_AUDIT.md)和[开发记录](docs/PROGRESS.md)。

## 已实现能力

- ARMv6-M Thumb CPU、MSP/PSP、xPSR/CONTROL、SVC、HardFault、WFI/WFE。
- Cortex-M0+ 异常入栈/返回、嵌套抢占、NVIC、SysTick 和 SCB 常用寄存器。
- Flash/SRAM/系统存储器、Flash 页擦写、自编程和 BIN/HEX/ELF 加载。
- RCC、GPIOA/B/F、EXTI、USART1、TIM1/TIM16、LPTIM1、SPI1、I2C1、
  ADC1、COMP1/2、CRC 和 IWDG。
- CLI 单步/跟踪、GPIO 脉冲、模拟毫伏输入和外设状态输出。
- Web 调试台：寄存器、符号反汇编、多断点、内存、GPIO、COMP 模拟输入和
  USART 终端。

## 工程结构

```text
include/py32emu/       公共 C 接口
src/core/              Cortex-M0+ CPU、总线与反汇编器
src/chips/             芯片描述和 SoC 装配
src/peripherals/       可复用外设模型
src/firmware/          BIN、HEX、ELF 固件加载器
src/cli/               命令行前端
src/web/               Web 调试后端
frontends/web/         Node 服务和浏览器界面
examples/              可直接编译的示例固件
tests/                 单元、固件、官方 SDK 和 Web 回归测试
docs/                  架构、调试器、覆盖率和进度文档
```

## 在 WSL/Linux 构建和测试

需要 GCC、GNU Make、`arm-none-eabi-gcc` 和 Node.js。官方例程回归默认从
`/mnt/e/BaiduNetdiskDownload/Py32资料/PY32F0xx_Firmware_V1.1.3` 读取 SDK。

```sh
cd /mnt/e/Projects/Py32Emu
make
make test
```

运行官方 HAL 编译的 HelloWorld：

```sh
make -C examples/hello_world
./build/py32emu examples/hello_world/build/hello_world.elf
```

输出中应包含：

```text
USART1 TX (13): 48 65 6C 6C 6F 20 57 6F 72 6C 64 0D 0A
```

## CLI

```sh
./build/py32emu firmware.elf --steps 100000
./build/py32emu firmware.hex --trace
./build/py32emu firmware.elf --pulse PB2 --pulse-at 10000
./build/py32emu firmware.elf --analog PA1=1800 --analog-at 10000
./build/py32emu firmware.elf --chip py32f002a-32k
```

`py32f002ax5` 严格使用官方描述的 20 KiB Flash/3 KiB SRAM；经当前实物确认的
隐藏容量器件可以显式选择 `py32f002a-32k`（32 KiB/4 KiB），两者不会混用容量边界。

## Web 调试台

```sh
./frontends/web/scripts/start.sh --port 4174
```

浏览器打开 `http://127.0.0.1:4174`。使用方法见
[Web 调试界面](docs/WEB_DEBUGGER.md)。

## 精度边界

这是面向固件执行、学习和自动回归的功能级模拟器，不是晶体管级或逐周期完全等价模型。
已实现外设优先覆盖官方 HAL 常见路径；高级模式、模拟噪声、精确启动时间和所有未使用保留位
仍可能与实物不同。发现差异时应以官方手册和真实芯片结果为准，并为差异增加回归测试。
