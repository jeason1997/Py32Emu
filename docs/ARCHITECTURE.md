# 架构

## 型号描述与装配

芯片描述中的 `peripherals` 位图决定 GPIO、EXTI、USART、定时器、SPI、I2C 和 ADC
实例是否进入总线；`external_irq_count` 同时限制 NVIC 可见和可服务的 IRQ 位。地址布局目前
仍由 PY32F0 SoC 装配层集中保存，只有第二个型号证明确有地址差异时才提取布局表。

CPU 核心只认识 ARMv6-M 状态和 `Py32Bus`，不包含任何 PY32 型号地址。总线把
Flash、SRAM、系统控制空间和外设窗口分派给内存或回调。`Py32ChipDescription`
保存型号容量与硬件能力，SoC 装配层据此创建实际区域和外设。

这一边界足以支持 PY32F002A 与未来 PY32F003 的差异，同时避免先建立复杂的
插件框架。只有第二个型号真正接入时，才提取已证实存在的共同外设配置。
