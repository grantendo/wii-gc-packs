# PADRead for LEGO Star Wars III, in main.dol itself: RiftWii's PAD search only looks at main.dol's code
# sections, and the game's apploader refuses a new section above 0x80900000 (the game's memory is used up
# to 0x809C2508). So it replaces a function nothing calls or points to, at 0x8079CC70 (0x140 bytes;
# build.py checks it fits and that nothing references it). si_read and pad_memset are in the pack's blob
# at 0x809D2520 (gcpad.c, entry.S), loaded by a Riivolution memory patch.

# u32 PADRead(PADStatus status[4])
#
# Reads the console's own GameCube ports (si_read in gcpad.c). Every port
# without a pad gets the SDK's error path: err = -1 (PAD_ERR_NO_CONTROLLER)
# and the other 10 bytes cleared. Those four unrolled
#     stb rE,10(rS); mr r3,rS; li r4,0; li r5,10; bl memset
# sequences in one framed function are what RiftWii's PAD search
# (src/patching/symsearch.cpp, find_pad_symbols) takes for PADRead. It then
# hooks this function and fills the ports still marked missing with the
# GameCube adapter's pads. Returns 0 (no rumble motors).
    .section .padread, "ax"
    .global PADRead
PADRead:
    stwu    1, -0x20(1)
    mflr    0
    stw     0, 0x24(1)
    stw     31, 0x1C(1)
    stw     30, 0x18(1)
    mr      31, 3
    bl      si_read             # fills live ports, returns a mask of missing ones
    mr      30, 3
    li      0, -1

    andi.   9, 30, 1
    beq     1f
    addi    9, 31, 0
    stb     0, 10(9)
    mr      3, 9
    li      4, 0
    li      5, 10
    bl      pad_memset
    li      0, -1
1:  andi.   9, 30, 2
    beq     2f
    addi    9, 31, 12
    stb     0, 10(9)
    mr      3, 9
    li      4, 0
    li      5, 10
    bl      pad_memset
    li      0, -1
2:  andi.   9, 30, 4
    beq     3f
    addi    9, 31, 24
    stb     0, 10(9)
    mr      3, 9
    li      4, 0
    li      5, 10
    bl      pad_memset
    li      0, -1
3:  andi.   9, 30, 8
    beq     4f
    addi    9, 31, 36
    stb     0, 10(9)
    mr      3, 9
    li      4, 0
    li      5, 10
    bl      pad_memset
4:  li      3, 0
    lwz     30, 0x18(1)
    lwz     31, 0x1C(1)
    lwz     0, 0x24(1)
    mtlr    0
    addi    1, 1, 0x20
    blr

