# Fixed entry points at the start of the section; main.dol's patched
# branches land here, so their targets never move when the C code changes.
    .section .entry, "ax"
    .global _entry
_entry:
    b   arena_fix          # +0x0  OSInit's OSSetArenaLo call (0x80764278)
    b   kpad_hook          # +0x4  the controller update's four KPADRead calls (0x80293A3C-0x80293A7C)
    b   probe_hook         # +0x8  the game's four WPADProbe calls
    b   setcb_hook         # +0xC  the game's KPADSetConnectCallback call (0x80292CCC)
                           # (PADRead lives in main.dol at 0x8079CC70: padread.S)

    .text
# void* pad_memset(void* p, int c, u32 n): bytes only, leaf.
    .global pad_memset
pad_memset:
    cmpwi   5, 0
    beqlr
    mtctr   5
    addi    9, 3, -1
5:  stbu    4, 1(9)
    bdnz    5b
    blr
