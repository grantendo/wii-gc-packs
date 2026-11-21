/* The blob sits at the bottom of the MEM1 arena, which arena_fix keeps the
   game's heap away from. Everything, bss included, is in the one image. */
OUTPUT_ARCH(powerpc)
ENTRY(_entry)
SECTIONS
{
    . = 0x803CFFE0;
    .image : {
        KEEP(*(.entry))
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data .data.*)
        *(.sdata .sdata.* .sbss .sbss.*)
        *(.bss .bss.* COMMON)
        . = ALIGN(32);
    }
    __image_end = .;
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.gnu.attributes) }
}
