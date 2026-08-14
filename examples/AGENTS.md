# 示例固件规则

- 每个示例位于 `examples/<名称>/`，至少包含源代码和可重复构建的 Makefile。
- 示例使用公开 SDK/HAL 或正常裸机接口，不依赖模拟器内部结构、固定 PC 或测试专用后门。
- WSL 中使用 `arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb` 构建，产物进入示例的 `build/`。
- 芯片宏、链接脚本、Flash/SRAM 容量必须与所选 `Py32ChipDescription` 一致。
- 示例成功条件必须可观察且可自动判断；仅运行到步数上限不能证明业务成功。
- 新增或修改示例后，实际编译并在模拟器运行；若用于回归，加入根 Makefile 的测试入口。
- HelloWorld 等串口示例要验证精确字节，不只搜索近似文本。
