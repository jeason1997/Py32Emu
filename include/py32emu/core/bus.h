#ifndef PY32EMU_CORE_BUS_H
#define PY32EMU_CORE_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PY32_BUS_MAX_REGIONS 32u

typedef bool (*Py32BusReadFn)(void *context, uint32_t offset,
                              unsigned size, uint32_t *value);
typedef bool (*Py32BusWriteFn)(void *context, uint32_t offset,
                               unsigned size, uint32_t value);

typedef struct {
    uint32_t base;
    uint32_t size;
    uint8_t *memory;
    bool read_only;
    Py32BusReadFn read;
    Py32BusWriteFn write;
    void *context;
    const char *name;
} Py32BusRegion;

typedef struct {
    Py32BusRegion regions[PY32_BUS_MAX_REGIONS];
    size_t region_count;
    /* 两路最近访问区域缓存：取指在 Flash、数据在 SRAM 时避免反复线性扫描。 */
    Py32BusRegion *cache[2];
    unsigned cache_next;
    /* 取指直读窗口：Flash 内容只由总线写入修改，可直接读宿主内存。 */
    const uint8_t *fetch_memory;
    uint32_t fetch_base;
    uint32_t fetch_size;
    uint8_t *data_memory;
    uint32_t data_base;
    uint32_t data_size;
    uint32_t fault_address;
    bool faulted;
} Py32Bus;

void py32_bus_init(Py32Bus *bus);
void py32_bus_set_fetch(Py32Bus *bus, const uint8_t *memory,
                        uint32_t base, uint32_t size);
void py32_bus_set_data(Py32Bus *bus, uint8_t *memory,
                       uint32_t base, uint32_t size);
bool py32_bus_add_memory(Py32Bus *bus, const char *name, uint32_t base,
                         uint8_t *memory, uint32_t size, bool read_only);
bool py32_bus_add_device(Py32Bus *bus, const char *name, uint32_t base,
                         uint32_t size, Py32BusReadFn read,
                         Py32BusWriteFn write, void *context);
bool py32_bus_read(Py32Bus *bus, uint32_t address, unsigned size,
                   uint32_t *value);
bool py32_bus_write(Py32Bus *bus, uint32_t address, unsigned size,
                    uint32_t value);

static inline bool py32_bus_fetch16(Py32Bus *bus, uint32_t address,
                                    uint32_t *value)
{
    if (bus != NULL && bus->fetch_memory != NULL &&
        bus->fetch_size >= 2u && address >= bus->fetch_base &&
        address - bus->fetch_base <= bus->fetch_size - 2u) {
        const uint8_t *p = bus->fetch_memory + (address - bus->fetch_base);
        *value = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
        return true;
    }
    return py32_bus_read(bus, address, 2u, value);
}

#endif

