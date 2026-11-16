#ifndef PY32EMU_SIM_DEVICE_H
#define PY32EMU_SIM_DEVICE_H

#include <stdint.h>

#define SIM_DEVICE_MAX_PINS 24u

typedef enum {
    SIM_LEVEL_LOW,
    SIM_LEVEL_HIGH,
    SIM_LEVEL_Z,
    SIM_LEVEL_CONFLICT
} SimLevel;

typedef struct SimDevice SimDevice;

typedef struct {
    void (*reset)(SimDevice *device);
    void (*tick)(SimDevice *device, uint64_t cycles,
                 uint32_t cycles_per_second);
    void (*pin_changed)(SimDevice *device, unsigned pin, SimLevel level);
} SimDeviceOps;

/*
 * 板级虚拟器件只通过引脚驱动/观察网络，不直接访问 CPU 或 SoC 内部状态。
 * 这与 PicEmu 的 SimDevice 边界一致，便于同一器件复用于 CLI/Web/实物桥接。
 */
struct SimDevice {
    const char *name;
    const SimDeviceOps *ops;
    void *state;
    unsigned pin_count;
    SimLevel drive[SIM_DEVICE_MAX_PINS];
    SimLevel observed[SIM_DEVICE_MAX_PINS];
};

void sim_device_init(SimDevice *device, const char *name,
                     const SimDeviceOps *ops, void *state,
                     unsigned pin_count);
void sim_device_set_drive(SimDevice *device, unsigned pin, SimLevel level);

#endif
