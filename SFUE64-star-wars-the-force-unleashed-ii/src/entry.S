# Fixed entry points and options at the start of the section; main.dol's
# patched calls land here, and the Riivolution options write the option
# bytes, so none of these addresses move when the C code changes.
    .section .entry, "ax"
    .global _entry
_entry:
    b   arena_fix          # +0x00  OSInit's OSSetArenaLo call (0x8043A888)
    b   kpad_hook          # +0x04  the per-frame input routine's KPADRead (0x802641DC)
    b   probe_hook         # +0x08  its two WPADProbe calls (0x8026431C, 0x802645B0)
    b   sync_hook          # +0x0C  its WPAD sync-device call (0x8026453C)

# +0x10: tap jump, one byte per GameCube port (0 off, 1 on). The pack's
# Riivolution options "Tap jump (port N)" write 01 here.
    .global tap_jump
tap_jump:
    .byte 0, 0, 0, 0
    .balign 32
