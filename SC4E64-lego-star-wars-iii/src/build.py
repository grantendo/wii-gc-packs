"""Build the LEGO Star Wars III GameCube-controller pack. Made from Ranger Cross's pad pack
(my Super Sentai Battle: Ranger Cross pack), with one change: LEGO III's memory is used
up to 0x809C2508 and its apploader refuses a main.dol section above 0x80900000, so the pack can't add a section.

  - the blob (gcpad.c + entry.S) at 0x809D2520, the bottom of the MEM1 arena: a Riivolution memory patch;
  - main.dol (a file patch) with PADRead (padread.S) in place of a function nothing calls or points to
    (0x8079CC70), where RiftWii's PAD search finds it, and ten calls pointed into the blob:
      0x80764278 OSInit's OSSetArenaLo                 -> arena_fix (heap above the blob)
      0x80293A3C/54/68/7C the four KPADRead calls       -> kpad_hook
      0x80292B88, 0x80292ED4, 0x802937D0, 0x80293CB8 the game's WPADProbe calls -> probe_hook
      0x80292CCC the game's KPADSetConnectCallback call      -> setcb_hook (a real Wii Remote shares the slot)

Usage: build.py [out dir]   (default: ../../build-pad)
"""
import os
import struct
import subprocess
import sys


HERE = os.path.dirname(os.path.abspath(__file__))
# The game's own main.dol, from your disc: set MAIN_DOL, or put it next to this script as main.dol.
DOL = os.environ.get("MAIN_DOL", os.path.join(os.path.dirname(os.path.abspath(__file__)), "main.dol"))
# devkitPPC: DEVKITPPC if its bin folder is there, else C:\devkitPro\devkitPPC (on Windows the installer sets
# DEVKITPPC to "/opt/devkitpro/devkitPPC", an MSYS path Python can't open). Tools are called with ".exe".
_DKP_ENV = os.environ.get("DEVKITPPC", "")
if not os.path.isdir(os.path.join(_DKP_ENV, "bin")):
    _DKP_ENV = r"C:\devkitPro\devkitPPC"
DKP = os.path.join(_DKP_ENV, "bin")
BASE = 0x809D2520          # the MEM1 arena's bottom: OSInit (0x80764260) rounds 0x809D2508 up
STACK_END = 0x809D2508
PADREAD = 0x8079CC70       # an unused game function (see find_unused below), 0x140 bytes
PADREAD_ROOM = 0x140
PADREAD_FIRST = 0x9421FF40  # its first instruction, stwu r1,-192(r1): the right game and revision
ENTRY = {"arena_fix": BASE, "kpad_hook": BASE + 4, "probe_hook": BASE + 8, "setcb_hook": BASE + 12}
SITES = [  # (address, what it calls now, our entry)
    (0x80764278, 0x80765740, "arena_fix"),
    (0x80293A3C, 0x807BBAB0, "kpad_hook"),
    (0x80293A54, 0x807BBAB0, "kpad_hook"),
    (0x80293A68, 0x807BBAB0, "kpad_hook"),
    (0x80293A7C, 0x807BBAB0, "kpad_hook"),
    (0x80292B88, 0x807C3A10, "probe_hook"),
    (0x80292ED4, 0x807C3A10, "probe_hook"),
    (0x802937D0, 0x807C3A10, "probe_hook"),
    (0x80293CB8, 0x807C3A10, "probe_hook"),
    (0x80292CCC, 0x807BC950, "setcb_hook"),  # KPADSetConnectCallback(chan, 0x80292B5C)
]
CFLAGS = ["-O2", "-mcpu=750", "-meabi", "-mhard-float", "-ffreestanding", "-fno-builtin",
          "-fno-tree-loop-distribute-patterns", "-nostdlib", "-fno-pic", "-G0", "-msdata=none",
          "-fno-exceptions", "-Wall", "-Werror"]


def tool(name):
    return os.path.join(DKP, "powerpc-eabi-" + name + ".exe")


def run(*args):
    subprocess.run(args, check=True, cwd=HERE)


def symbols(elf):
    nm = subprocess.run([tool("nm"), elf], capture_output=True, text=True, check=True).stdout
    return {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}


def build(tmp):
    run(tool("gcc"), *CFLAGS, "-c", "gcpad.c", "-o", os.path.join(tmp, "gcpad.o"))
    run(tool("gcc"), "-c", "entry.S", "-o", os.path.join(tmp, "entry.o"))
    run(tool("gcc"), "-c", "padread.S", "-o", os.path.join(tmp, "padread.o"))
    elf = os.path.join(tmp, "gcpad.elf")
    run(tool("ld"), "-T", "gcpad.ld", "-o", elf, os.path.join(tmp, "entry.o"), os.path.join(tmp, "gcpad.o"))
    syms = symbols(elf)
    assert syms["_entry"] == BASE, hex(syms["_entry"])
    pelf = os.path.join(tmp, "padread.elf")
    run(tool("ld"), "-T", "padread.ld", "--just-symbols=" + elf, "-o", pelf, os.path.join(tmp, "padread.o"))
    psyms = symbols(pelf)
    assert psyms["PADRead"] == PADREAD, hex(psyms["PADRead"])
    blob_f, pad_f = os.path.join(tmp, "gcpad.bin"), os.path.join(tmp, "padread.bin")
    run(tool("objcopy"), "-O", "binary", "-j", ".image", elf, blob_f)
    run(tool("objcopy"), "-O", "binary", "-j", ".padread", pelf, pad_f)
    blob, padread = open(blob_f, "rb").read(), open(pad_f, "rb").read()
    assert len(padread) <= PADREAD_ROOM, f"PADRead is {len(padread):#x} bytes, room {PADREAD_ROOM:#x}"
    dis = subprocess.run([tool("objdump"), "-d", elf], capture_output=True, text=True).stdout
    dis += subprocess.run([tool("objdump"), "-d", pelf], capture_output=True, text=True).stdout
    assert "(r13)" not in dis and "(r2)" not in dis, "the pack uses the game's small-data registers"
    open(os.path.join(tmp, "gcpad.dis"), "w").write(dis)
    return blob, padread, syms


def bl(site, target):
    off = target - site
    assert -0x2000000 <= off < 0x2000000
    return 0x48000001 | (off & 0x03FFFFFC)


def make_dol(padread):
    d = bytearray(open(DOL, "rb").read())
    offs = struct.unpack_from(">18I", d, 0)
    addrs = struct.unpack_from(">18I", d, 0x48)
    sizes = struct.unpack_from(">18I", d, 0x90)
    bss, bss_size = struct.unpack_from(">II", d, 0xD8)
    assert BASE >= STACK_END >= bss + bss_size, "the blob must sit above bss and the stack"

    def fo(addr):
        i = next(i for i in range(7) if sizes[i] and addrs[i] <= addr < addrs[i] + sizes[i])
        return offs[i] + addr - addrs[i]
    for site, old, entry in SITES:
        cur = struct.unpack_from(">I", d, fo(site))[0]
        assert cur == bl(site, old), f"{site:#x} holds {cur:08X}"
        struct.pack_into(">I", d, fo(site), bl(site, ENTRY[entry]))
    start = fo(PADREAD)
    assert struct.unpack_from(">I", d, start)[0] == PADREAD_FIRST, "0x8079CC70 isn't the unused function"
    end = start
    while struct.unpack_from(">I", d, end)[0] != 0x4E800020:  # its blr
        end += 4
    assert end + 4 - start == PADREAD_ROOM
    d[start:start + PADREAD_ROOM] = padread + b"\0" * (PADREAD_ROOM - len(padread))
    return bytes(d)


def find_unused():
    """Re-check that nothing calls, branches to or points to 0x8079CC70 (bl/b, a data word, lis+addi/ori)."""
    d = open(DOL, "rb").read()
    offs = struct.unpack_from(">18I", d, 0)
    addrs = struct.unpack_from(">18I", d, 0x48)
    sizes = struct.unpack_from(">18I", d, 0x90)
    hi16, lo16 = PADREAD >> 16, PADREAD & 0xFFFF
    for i in range(18):
        if not sizes[i]:
            continue
        raw = d[offs[i]:offs[i] + sizes[i]]
        for k in range(0, len(raw) - 3, 4):
            w = struct.unpack_from(">I", raw, k)[0]
            a = addrs[i] + k
            if w == PADREAD:
                return f"a data word at {a:#x}"
            if i < 7 and (w >> 26) == 18:
                li = w & 0x03FFFFFC
                li = li - 0x04000000 if li & 0x02000000 else li
                if ((li if w & 2 else a + li) & 0xFFFFFFFF) == PADREAD:
                    return f"a branch at {a:#x}"
            if i < 7 and (w >> 26) in (14, 24) and (w & 0xFFFF) == lo16:
                # a lis of the same register up to 200 instructions back (0x806BC338, the first choice, was
                # formed this way 0x3F00 bytes after its lis: a callback table)
                ra = (w >> 16) & 31
                hi = (hi16 + 1) & 0xFFFF if (w >> 26) == 14 and lo16 & 0x8000 else hi16
                for b in range(1, 200):
                    if k < 4 * b:
                        break
                    p = struct.unpack_from(">I", raw, k - 4 * b)[0]
                    if p == 0x4E800020:  # blr: another function
                        break
                    if (p >> 26) == 15 and ((p >> 21) & 31) == ra and ((p >> 16) & 31) == 0:
                        if (p & 0xFFFF) == hi:
                            return f"lis/addi at {a:#x}"
                        break
    return None


def main(out):
    tmp = os.path.join(out, "_obj")
    os.makedirs(tmp, exist_ok=True)
    ref = find_unused()
    assert ref is None, f"0x8079CC70 is referenced: {ref}"
    blob, padread, syms = build(tmp)
    dol = make_dol(padread)
    pack = os.path.join(out, "lsw3_pad")
    os.makedirs(pack, exist_ok=True)
    os.makedirs(os.path.join(out, "riivolution"), exist_ok=True)
    open(os.path.join(pack, "main.dol"), "wb").write(dol)
    open(os.path.join(pack, "gcpad.bin"), "wb").write(blob)
    xml = ('<wiidisc version="1">\n'
           '  <!-- Built by src/build.py: main.dol with PADRead and '
           'ten calls into the blob; the blob at 0x809D2520 -->\n'
           '  <id game="SC4E"/>\n  <options>\n'
           '    <section name="LEGO Star Wars III: The Clone Wars">\n'
           '      <option name="GameCube controllers (WUP-028)">\n'
           '        <choice name="On (ports 1-2 = players 1-2)"><patch id="lsw3_pad"/></choice>\n'
           '      </option>\n    </section>\n  </options>\n'
           '  <patch id="lsw3_pad" root="/lsw3_pad">\n'
           '    <file disc="main.dol" external="main.dol"/>\n'
           f'    <memory offset="{BASE:#010x}" valuefile="gcpad.bin"/>\n'
           '  </patch>\n</wiidisc>\n')
    open(os.path.join(out, "riivolution", "lsw3_pad.xml"), "w", newline="\n").write(xml)
    print(f"blob {len(blob)} bytes at {BASE:#x}-{BASE + len(blob):#x} (heap from {(syms['__image_end'] + 31) & ~31:#x}); "
          f"PADRead {len(padread):#x} of {PADREAD_ROOM:#x} bytes at {PADREAD:#x}")
    for k in ("arena_fix", "kpad_hook", "probe_hook", "si_read", "pad_memset"):
        print(f"  {k} {syms[k]:#x}")


if __name__ == "__main__":
    main(os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "build")))
