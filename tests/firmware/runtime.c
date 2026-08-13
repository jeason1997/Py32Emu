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

/* Freestanding SDK regression images do not link a hosted stdio library. */
int puts(const char *text)
{
    (void)text;
    return 0;
}

int printf(const char *format, ...)
{
    (void)format;
    return 0;
}
