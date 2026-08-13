#ifndef PY32EMU_CORE_DISASSEMBLER_H
#define PY32EMU_CORE_DISASSEMBLER_H

#include <stddef.h>
#include <stdint.h>

unsigned py32_thumb_disassemble(uint32_t pc, uint16_t op, uint16_t next,
                                char *text, size_t text_size);

#endif
