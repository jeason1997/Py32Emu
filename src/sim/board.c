#include "py32emu/sim/board.h"

#include <string.h>

static SimLevel combine(SimLevel current, SimLevel driver)
{
    if (driver == SIM_LEVEL_Z) return current;
    if (driver == SIM_LEVEL_CONFLICT) return SIM_LEVEL_CONFLICT;
    if (current == SIM_LEVEL_Z) return driver;
    return current == driver ? current : SIM_LEVEL_CONFLICT;
}

void sim_board_init(SimBoard *board, SimMcu *mcu)
{
    memset(board, 0, sizeof(*board));
    if (mcu != NULL) (void)sim_board_add_mcu(board, mcu);
}

bool sim_board_add_mcu(SimBoard *board, SimMcu *mcu)
{
    unsigned i;
    if (board == NULL || mcu == NULL) return false;
    for (i = 0; i < board->mcu_count; ++i)
        if (board->mcus[i] == mcu) return true;
    if (board->mcu_count >= SIM_MAX_PARTS) return false;
    board->mcus[board->mcu_count++] = mcu;
    if (board->mcu == NULL) board->mcu = mcu;
    return true;
}

int sim_board_add_net(SimBoard *board, const char *name)
{
    SimNet *net;
    if (board->net_count >= SIM_MAX_NETS) return -1;
    net = &board->nets[board->net_count];
    memset(net, 0, sizeof(*net));
    net->name = name;
    net->level = SIM_LEVEL_Z;
    return (int)board->net_count++;
}

static bool add_endpoint(SimBoard *board, int index, SimEndpoint endpoint)
{
    SimNet *net;
    if (index < 0 || (unsigned)index >= board->net_count) return false;
    net = &board->nets[index];
    if (net->endpoint_count >= SIM_MAX_ENDPOINTS) return false;
    net->endpoints[net->endpoint_count++] = endpoint;
    return true;
}

bool sim_board_connect_mcu(SimBoard *board, int net, unsigned mcu_pin)
{
    return sim_board_connect_mcu_instance(board, net, board->mcu, mcu_pin);
}

bool sim_board_connect_mcu_instance(SimBoard *board, int net,
                                    SimMcu *mcu, unsigned mcu_pin)
{
    SimEndpoint endpoint = {.type = SIM_ENDPOINT_MCU_PIN};
    endpoint.target.mcu.mcu = mcu;
    endpoint.target.mcu.pin = mcu_pin;
    return mcu != NULL && mcu_pin < sim_mcu_pin_count(mcu) &&
           sim_board_add_mcu(board, mcu) &&
           add_endpoint(board, net, endpoint);
}

bool sim_board_connect_device(SimBoard *board, int net,
                              SimDevice *device, unsigned device_pin)
{
    SimEndpoint endpoint = {.type = SIM_ENDPOINT_DEVICE_PIN};
    unsigned i;
    bool known = false;
    if (device == NULL || device_pin >= device->pin_count) return false;
    for (i = 0; i < board->device_count; ++i)
        known |= board->devices[i] == device;
    if (!known) {
        if (board->device_count >= SIM_MAX_DEVICES) return false;
        board->devices[board->device_count++] = device;
    }
    endpoint.target.device.device = device;
    endpoint.target.device.pin = device_pin;
    return add_endpoint(board, net, endpoint);
}

bool sim_board_merge_nets(SimBoard *board, int keep, int remove)
{
    SimNet *target, *source;
    unsigned i;
    if (keep < 0 || remove < 0 || keep == remove ||
        (unsigned)keep >= board->net_count ||
        (unsigned)remove >= board->net_count) return false;
    target = &board->nets[keep];
    source = &board->nets[remove];
    if (target->endpoint_count + source->endpoint_count >
        SIM_MAX_ENDPOINTS) return false;
    for (i = 0; i < source->endpoint_count; ++i)
        target->endpoints[target->endpoint_count++] = source->endpoints[i];
    source->endpoint_count = 0;
    source->level = SIM_LEVEL_Z;
    return true;
}

void sim_board_resolve(SimBoard *board)
{
    unsigned n;
    for (n = 0; n < board->net_count; ++n) {
        SimNet *net = &board->nets[n];
        SimLevel resolved = SIM_LEVEL_Z;
        unsigned e, mcu_endpoints = 0;
        bool device_drives = false;
        for (e = 0; e < net->endpoint_count; ++e) {
            SimEndpoint *endpoint = &net->endpoints[e];
            SimLevel drive;
            if (endpoint->type == SIM_ENDPOINT_MCU_PIN) {
                ++mcu_endpoints;
                drive = sim_mcu_pin_drive(endpoint->target.mcu.mcu,
                                          endpoint->target.mcu.pin);
            } else {
                drive = endpoint->target.device.device
                            ->drive[endpoint->target.device.pin];
                if (drive != SIM_LEVEL_Z) device_drives = true;
            }
            resolved = combine(resolved, drive);
        }
        net->level = resolved;
        for (e = 0; e < net->endpoint_count; ++e) {
            SimEndpoint *endpoint = &net->endpoints[e];
            if (endpoint->type == SIM_ENDPOINT_MCU_PIN) {
                /*
                 * 只有器件或另一颗 MCU 驱动网络时才需要回灌 MCU 输入。
                 * 扬声器这类纯观察器件为高阻，跳过可避免每条指令重复
                 * 刷新 GPIO 输入和 EXTI，保持 48 MHz 实时余量。
                 */
                if (device_drives || mcu_endpoints > 1u)
                    sim_mcu_set_pin_input(endpoint->target.mcu.mcu,
                                          endpoint->target.mcu.pin, resolved);
            } else {
                SimDevice *device = endpoint->target.device.device;
                unsigned pin = endpoint->target.device.pin;
                if (device->observed[pin] != resolved) {
                    device->observed[pin] = resolved;
                    if (device->ops != NULL &&
                        device->ops->pin_changed != NULL)
                        device->ops->pin_changed(device, pin, resolved);
                }
            }
        }
    }
}

void sim_board_reset(SimBoard *board)
{
    unsigned i;
    board->device_cycles = 0u;
    for (i = 0; i < board->mcu_count; ++i) sim_mcu_reset(board->mcus[i]);
    for (i = 0; i < board->device_count; ++i) {
        SimDevice *device = board->devices[i];
        if (device->ops != NULL && device->ops->reset != NULL)
            device->ops->reset(device);
    }
    sim_board_resolve(board);
}

unsigned sim_board_step(SimBoard *board)
{
    unsigned cycles = 0;
    unsigned i;
    bool dirty = false;
    for (i = 0; i < board->mcu_count; ++i) {
        unsigned consumed = sim_mcu_step(board->mcus[i]);
        if (consumed > cycles) cycles = consumed;
        if (board->mcus[i]->pin_dirty) dirty = true;
    }
    /*
     * 器件按 16 周期批量推进，而不是每条指令调用一次。扬声器的音频
     * 采样周期为 2000+ 周期，这个粒度远小于采样间隔，但能显著降低
     * 48 MHz 模拟时的函数调用开销。
     */
    board->device_cycles += cycles;
    if (board->device_cycles >= 16u || dirty) {
        for (i = 0; i < board->device_count; ++i) {
            SimDevice *device = board->devices[i];
            if (device->ops != NULL && device->ops->tick != NULL)
                device->ops->tick(device, board->device_cycles,
                                  board->mcu->cycles_per_second);
        }
        board->device_cycles = 0u;
    }
    if (dirty) {
        for (i = 0; i < board->mcu_count; ++i)
            board->mcus[i]->pin_dirty = false;
        sim_board_resolve(board);
    }
    return cycles;
}
