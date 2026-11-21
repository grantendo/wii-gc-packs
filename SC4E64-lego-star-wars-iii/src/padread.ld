/* PADRead in place of an unused game function (padread.S). */
OUTPUT_ARCH(powerpc)
SECTIONS
{
    . = 0x8079CC70;
    .padread : { KEEP(*(.padread)) }
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.gnu.attributes) *(.text) *(.data) *(.bss) }
}
