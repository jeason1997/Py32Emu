#include <stddef.h>

void *memset(void *destination, int value, size_t size)
{
    unsigned char *out = destination;
    while (size-- > 0u) *out++ = (unsigned char)value;
    return destination;
}
void *memcpy(void *destination, const void *source, size_t size)
{
    unsigned char *out = destination;
    const unsigned char *in = source;
    while (size-- > 0u) *out++ = *in++;
    return destination;
}
