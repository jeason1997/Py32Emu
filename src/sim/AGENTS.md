# 板级仿真层规则

本规则适用于 `src/sim/` 与 `include/py32emu/sim/`。

- 本层负责板级电路，不实现芯片内部寄存器；SoC 外设仍留在 `src/peripherals/`。
- `SimDevice` 只通过引脚 drive/observed 和 tick 工作，不得直接访问 `Py32Soc`。
- `SimMcu` 适配器包装具体 SoC；`SimBoard` 只依赖 `SimMcu` 接口。
- 新增器件使用独立头文件和实现文件，不要把具体器件函数堆到 `device.h`。
- 前端音频/显示 sink 不实现器件协议；器件模型产生的 PCM/频率/状态由前端消费。
- 器件时序必须由传入的周期数和 `cycles_per_second` 决定，不能依赖宿主帧率。
