/* The blob (a Riivolution memory patch) sits just above the main thread's stack (which ends at
   0x809D2508), at what was the MEM1 arena's bottom; arena_fix moves the
   game's heap above it. Everything, bss included, is in the one image. */
OUTPUT_ARCH(powerpc)
ENTRY(_entry)
SECTIONS
{
    . = 0x809D2520;
    .image : {
        KEEP(*(.entry))
        *(.text .text.*)
        *(.rodata .rodata.* .rodata.cst*)
        *(.data .data.*)
        *(.sdata .sdata.* .sdata2 .sdata2.* .sbss .sbss.* .sbss2 .sbss2.*)
        *(.bss .bss.* COMMON)
        . = ALIGN(32);
    }
    __image_end = .;
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.gnu.attributes) }
}
