# Py32Emu

Py32Emu 是一个使用 C11 编写的 PY32 Cortex-M0+ 微控制器模拟器。当前首先支持
PY32F002A，并通过芯片描述层为 PY32F003 等后续型号保留扩展入口。

项目正在持续开发，当前实现范围和下一步见 [开发进度](docs/PROGRESS.md)。

## 结构

```text
include/py32emu/       公共接口
src/core/              Cortex-M0+ CPU 与内存总线
src/chips/             各 PY32 型号描述与片上系统装配
src/peripherals/       可复用外设模型
src/firmware/          BIN、HEX、ELF 固件加载
src/cli/               命令行调试器
tests/                 单元与固件回归测试
docs/                  架构、资料索引与开发进度
```

## 构建

```sh
make
make test
./build/py32emu firmware.bin
```

运行官方 HAL 编译的串口示例：

```sh
make -C examples/hello_world
./build/py32emu examples/hello_world/build/hello_world.elf
```

默认型号 `py32f002ax5` 使用官方标称 20 KiB Flash/3 KiB SRAM。对于已通过
实机确认具有隐藏容量的器件，可以指定：

```sh
./build/py32emu firmware.elf --chip py32f002a-32k
```
