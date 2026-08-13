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
    uint32_t fault_address;
    bool faulted;
} Py32Bus;

void py32_bus_init(Py32Bus *bus);
bool py32_bus_add_memory(Py32Bus *bus, const char *name, uint32_t base,
                         uint8_t *memory, uint32_t size, bool read_only);
bool py32_bus_add_device(Py32Bus *bus, const char *name, uint32_t base,
                         uint32_t size, Py32BusReadFn read,
                         Py32BusWriteFn write, void *context);
bool py32_bus_read(Py32Bus *bus, uint32_t address, unsigned size,
                   uint32_t *value);
bool py32_bus_write(Py32Bus *bus, uint32_t address, unsigned size,
                    uint32_t value);

#endif

