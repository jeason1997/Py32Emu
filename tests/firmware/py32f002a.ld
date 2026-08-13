MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 20K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 3K
}

_stack_top = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .vectors : { KEEP(*(.vectors)) } > FLASH
    .text : { *(.text*) *(.rodata*) } > FLASH
    .data : { *(.data*) } > RAM AT > FLASH
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
