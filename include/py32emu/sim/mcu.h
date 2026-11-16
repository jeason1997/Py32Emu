#ifndef PY32EMU_SIM_MCU_H
#define PY32EMU_SIM_MCU_H

#include "py32emu/sim/device.h"
#include "py32emu/sim/limits.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct SimMcu SimMcu;

typedef struct {
    unsigned cycle, pin;
    SimLevel level;
} SimPinEvent;

typedef struct {
    void (*reset)(SimMcu *mcu);
    unsigned (*step)(SimMcu *mcu);
    unsigned (*pin_count)(const SimMcu *mcu);
    SimLevel (*pin_drive)(const SimMcu *mcu, unsigned pin);
    void (*set_pin_input)(SimMcu *mcu, unsigned pin, SimLevel level);
    bool (*stopped)(const SimMcu *mcu);
} SimMcuOps;

/*
 * 电路层只依赖这个抽象接口；具体 Py32Soc 细节留在适配器中。
 * cycles_per_second 是 step() 返回周期的时基。
 */
struct SimMcu {
    const SimMcuOps *ops;
    void *state;
    uint32_t cycles_per_second;
    bool pin_dirty;
    /* 本条指令内有序的引脚事件；cycle 为相对指令起点的周期偏移。 */
    SimPinEvent pin_events[SIM_MAX_PIN_EVENTS];
    unsigned pin_event_count, pin_events_visible;
    bool replay_pins, timed_edges;
};

void sim_mcu_reset(SimMcu *mcu);
unsigned sim_mcu_step(SimMcu *mcu);
unsigned sim_mcu_pin_count(const SimMcu *mcu);
SimLevel sim_mcu_pin_drive(const SimMcu *mcu, unsigned pin);
void sim_mcu_set_pin_input(SimMcu *mcu, unsigned pin, SimLevel level);
bool sim_mcu_stopped(const SimMcu *mcu);

#endif
