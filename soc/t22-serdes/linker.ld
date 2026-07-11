OUTPUT_ARCH(riscv)
ENTRY(_start)

STACK_SIZE = 0x1000;

MEMORY
{
    ROM    (rx)  : ORIGIN = 0x00140000, LENGTH = 0x00010000
    RAM    (rwx) : ORIGIN = 0x00150000, LENGTH = 0x0000F000
    CONFIG (rw)  : ORIGIN = 0x0015F000, LENGTH = 0x00001000
}

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        KEEP(*(.text.init))
        *(.text .text.*)
        . = ALIGN(4);
        __text_end = .;
    } > ROM

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata .rodata.*)
        . = ALIGN(4);
        __rodata_end = .;
    } > ROM

    .data :
    {
        . = ALIGN(4);
        __data_start = .;
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > RAM AT > ROM

    __data_load_start = LOADADDR(.data);
    __rom_image_end = LOADADDR(.data) + SIZEOF(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    .config (NOLOAD) :
    {
        . = ALIGN(4);
        KEEP(*(.config .config.*))
        . = ALIGN(4);
    } > CONFIG

    __heap_start = ALIGN(__bss_end, 16);
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
    __stack_limit = __stack_top - STACK_SIZE;
    __heap_end = __stack_limit;
    __config_start = ORIGIN(CONFIG);
    __config_end = ORIGIN(CONFIG) + LENGTH(CONFIG);

    ASSERT(__rom_image_end <= ORIGIN(ROM) + LENGTH(ROM), "ROM overflow")
    ASSERT(__heap_start <= __stack_limit, "RAM overflow: data/heap overlaps stack")
    ASSERT(__stack_top == __config_start, "RAM/config boundary mismatch")
    ASSERT(SIZEOF(.config) <= LENGTH(CONFIG), "Configuration area overflow")
}
