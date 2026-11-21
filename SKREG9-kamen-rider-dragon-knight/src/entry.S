# Fixed entry points at the start of the blob; main.dol's patched branches
# land here, so their targets never move when the C code changes.
    .section .entry, "ax"
    .global _entry
_entry:
    b   arena_fix          # +0x0  OSInit's OSSetArenaLo call
    b   kpad_read_main     # +0x4  the game's per-frame KPADRead call
    b   kpad_read_retry    # +0x8  its retry after KPADReset
    b   wpad_probe         # +0xC  the HOME Menu's WPADProbe call
