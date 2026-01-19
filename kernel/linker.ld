ENTRY(_start)

SECTIONS
{
    . = 0x1000;

    .text : {
        build/entry.o(.text)
        *(.text)
    }

    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(COMMON) *(.bss) }

    _end = .;
}
