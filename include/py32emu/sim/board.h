#ifndef PY32EMU_SIM_BOARD_H
#define PY32EMU_SIM_BOARD_H

#include "py32emu/sim/device.h"
#include "py32emu/sim/limits.h"
#include "py32emu/sim/mcu.h"

#include <stdbool.h>

typedef enum {
    SIM_ENDPOINT_MCU_PIN,
    SIM_ENDPOINT_DEVICE_PIN
} SimEndpointType;

typedef struct {
    SimEndpointType type;
    union {
        struct {
            SimMcu *mcu;
            unsigned pin;
        } mcu;
        struct {
            SimDevice *device;
            unsigned pin;
        } device;
    } target;
} SimEndpoint;

typedef struct {
    const char *name;
    SimEndpoint endpoints[SIM_MAX_ENDPOINTS];
    unsigned endpoint_count;
    SimLevel level;
} SimNet;

typedef struct {
    SimMcu *mcu;
    SimMcu *mcus[SIM_MAX_PARTS];
    unsigned mcu_count;
    SimNet nets[SIM_MAX_NETS];
    unsigned net_count;
    SimDevice *devices[SIM_MAX_DEVICES];
    unsigned device_count;
    uint64_t device_cycles;
} SimBoard;

void sim_board_init(SimBoard *board, SimMcu *mcu);
bool sim_board_add_mcu(SimBoard *board, SimMcu *mcu);
int sim_board_add_net(SimBoard *board, const char *name);
bool sim_board_connect_mcu(SimBoard *board, int net, unsigned mcu_pin);
bool sim_board_connect_mcu_instance(SimBoard *board, int net,
                                    SimMcu *mcu, unsigned mcu_pin);
bool sim_board_connect_device(SimBoard *board, int net,
                              SimDevice *device, unsigned device_pin);
bool sim_board_merge_nets(SimBoard *board, int keep, int remove);
void sim_board_reset(SimBoard *board);
unsigned sim_board_step(SimBoard *board);
void sim_board_resolve(SimBoard *board);

#endif
