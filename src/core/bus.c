#include "py32emu/core/bus.h"

#include <string.h>

static bool valid_size(unsigned size)
{
    return size == 1u || size == 2u || size == 4u;
}

static Py32BusRegion *find_region(Py32Bus *bus, uint32_t address,
                                  unsigned size)
{
    size_t i;
    uint64_t end = (uint64_t)address + size;

    for (i = 0; i < bus->region_count; ++i) {
        Py32BusRegion *region = &bus->regions[i];
        uint64_t region_end = (uint64_t)region->base + region->size;
        if (address >= region->base && end <= region_end) return region;
    }
    return NULL;
}

void py32_bus_init(Py32Bus *bus)
{
    memset(bus, 0, sizeof(*bus));
}

static bool add_region(Py32Bus *bus, const Py32BusRegion *region)
{
    size_t i;
    uint64_t end;

    if (region->size == 0u || bus->region_count >= PY32_BUS_MAX_REGIONS)
        return false;
    end = (uint64_t)region->base + region->size;
    for (i = 0; i < bus->region_count; ++i) {
        const Py32BusRegion *other = &bus->regions[i];
        uint64_t other_end = (uint64_t)other->base + other->size;
        if (region->base < other_end && other->base < end) return false;
    }
    bus->regions[bus->region_count++] = *region;
    return true;
}

bool py32_bus_add_memory(Py32Bus *bus, const char *name, uint32_t base,
                         uint8_t *memory, uint32_t size, bool read_only)
{
    Py32BusRegion region = {base, size, memory, read_only, NULL, NULL,
                            NULL, name};
    return bus != NULL && memory != NULL && add_region(bus, &region);
}

bool py32_bus_add_device(Py32Bus *bus, const char *name, uint32_t base,
                         uint32_t size, Py32BusReadFn read,
                         Py32BusWriteFn write, void *context)
{
    Py32BusRegion region = {base, size, NULL, false, read, write,
                            context, name};
    return bus != NULL && (read != NULL || write != NULL) &&
           add_region(bus, &region);
}

bool py32_bus_read(Py32Bus *bus, uint32_t address, unsigned size,
                   uint32_t *value)
{
    Py32BusRegion *region;
    uint32_t offset;
    unsigned i;

    if (bus == NULL || value == NULL || !valid_size(size) ||
        (address & (size - 1u)) != 0u) return false;
    region = find_region(bus, address, size);
    if (region == NULL) goto fault;
    offset = address - region->base;
    if (region->read != NULL)
        return region->read(region->context, offset, size, value);
    if (region->memory == NULL) goto fault;
    *value = 0;
    for (i = 0; i < size; ++i)
        *value |= (uint32_t)region->memory[offset + i] << (8u * i);
    return true;

fault:
    bus->faulted = true;
    bus->fault_address = address;
    return false;
}

bool py32_bus_write(Py32Bus *bus, uint32_t address, unsigned size,
                    uint32_t value)
{
    Py32BusRegion *region;
    uint32_t offset;
    unsigned i;

    if (bus == NULL || !valid_size(size) ||
        (address & (size - 1u)) != 0u) return false;
    region = find_region(bus, address, size);
    if (region == NULL) goto fault;
    offset = address - region->base;
    if (region->write != NULL)
        return region->write(region->context, offset, size, value);
    if (region->memory == NULL || region->read_only) goto fault;
    for (i = 0; i < size; ++i)
        region->memory[offset + i] = (uint8_t)(value >> (8u * i));
    return true;

fault:
    bus->faulted = true;
    bus->fault_address = address;
    return false;
}

