# Web 调试界面

调试台同时支持 GPIO 数字电平和 COMP 模拟输入。COMP 面板可向 PA1/COMP1 或
PA3/COMP2 注入 0–5000mV，状态协议会返回两个 CSR 和当前注入值；这与 CLI 的
`--analog PA1=1800 --analog-at N` 使用同一个 SoC 接口。

Web 调试台复用 C11 编写的 `Py32Soc`，浏览器和 Node 服务不包含另一份 CPU 实现。
因此 CLI、自动化测试和 Web 执行同一套 Cortex-M0+、总线与外设逻辑。

## 启动

在 WSL/Linux 中：

```sh
./frontends/web/scripts/start.sh --port 4174
```

在 Windows PowerShell 中：

```powershell
.\frontends\web\scripts\start.ps1 -Port 4174
```

然后打开 `http://127.0.0.1:4174`。固件路径必须位于项目目录中，可以使用 ELF、HEX
或 BIN；默认示例为 `examples/hello_world/build/hello_world.elf`。

## 当前能力

- 选择 `py32f002ax5` 或实机确认过的 `py32f002a-32k` 描述。
- 加载、复位、单步和按步数运行。
- 设置和查看一个或多个地址断点，继续运行时自动越过当前断点一次。
- 查看 R0-R15、xPSR、CONTROL、当前异常、周期和停止原因。
- 查看 GPIOA/B/F 的 MODER、IDR、ODR，并主动驱动或释放任意输入引脚。
- 查看 ELF 符号化的 PC 附近 Thumb 反汇编，并按地址读取最多 256 字节内存。
- 查看 USART1 输出，并向 USART1 接收 FIFO 发送 UTF-8 文本。

后续会增加源码行映射、可编辑寄存器/内存和更完整的运行控制。
