"""Build the Batman: The Brave and the Bold – The Videogame GameCube-controller pack:
a main.dol with one more text section (gcpad.c + entry.S at 0x8064F3E0, just above the main thread's stack) and
three of the game's calls pointed into it, plus a Riivolution XML that swaps in that main.dol. Made the Ranger
Cross way (my Super Sentai Battle: Ranger Cross pack): the game's memory ends at
0x8064F3C8, far below the apploader's 0x80900000 limit that made LEGO Star Wars III use an unused function.

  - 0x80404968 OSInit's OSSetArenaLo          -> arena_fix (heap above the section)
  - 0x8026EED8 the controller update's KPADRead -> kpad_hook
  - 0x8026EF0C the controller update's WPADProbe -> probe_hook

Usage: build.py [out dir]   (default: ../build)
"""
import os
import struct
import subprocess
import sys


HERE = os.path.dirname(os.path.abspath(__file__))
# The game's own main.dol, from your disc: set MAIN_DOL, or put it next to this script as main.dol. It's checked
# first: the pack patches fixed addresses, so only the supported disc's main.dol will do.
DOL = os.environ.get("MAIN_DOL", os.path.join(os.path.dirname(os.path.abspath(__file__)), "main.dol"))
MAIN_DOL_CRC32 = 0xEA4C0FD6  # S3BEWR
import zlib as _zlib  # noqa: E402
if not os.path.isfile(DOL):
    raise SystemExit("no main.dol: set MAIN_DOL to the game's own main.dol, or put it next to this script")
if _zlib.crc32(open(DOL, "rb").read()) != MAIN_DOL_CRC32:
    raise SystemExit("%s isn't the main.dol this pack is for (S3BEWR, CRC32 %08X): check the game, its region and "
                     "its revision" % (DOL, MAIN_DOL_CRC32))
# devkitPPC: DEVKITPPC if its bin folder is there, else C:\devkitPro\devkitPPC (on Windows the installer sets
# DEVKITPPC to "/opt/devkitpro/devkitPPC", an MSYS path Python can't open). Tools are called with ".exe".
_DKP_ENV = os.environ.get("DEVKITPPC", "")
if not os.path.isdir(os.path.join(_DKP_ENV, "bin")):
    _DKP_ENV = r"C:\devkitPro\devkitPPC"
DKP = os.path.join(_DKP_ENV, "bin")
BASE = 0x8064F3E0       # OSInit (0x80404950) rounds the arena's bottom 0x8064F3C8 up to this
STACK_END = 0x8064F3C8
ENTRY = {"arena_fix": BASE, "kpad_hook": BASE + 4, "probe_hook": BASE + 8}
SITES = [  # (address, what it calls now, our entry)
    (0x80404968, 0x80405EA0, "arena_fix"),
    (0x8026EED8, 0x80457FC0, "kpad_hook"),
    (0x8026EF0C, 0x804414F0, "probe_hook"),
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


def build_blob(tmp):
    run(tool("gcc"), *CFLAGS, "-c", "gcpad.c", "-o", os.path.join(tmp, "gcpad.o"))
    run(tool("gcc"), "-c", "entry.S", "-o", os.path.join(tmp, "entry.o"))
    elf = os.path.join(tmp, "gcpad.elf")
    run(tool("ld"), "-T", "gcpad.ld", "-o", elf, os.path.join(tmp, "entry.o"), os.path.join(tmp, "gcpad.o"))
    syms = symbols(elf)
    assert syms["_entry"] == BASE, hex(syms["_entry"])
    blob_f = os.path.join(tmp, "gcpad.bin")
    run(tool("objcopy"), "-O", "binary", "-j", ".image", elf, blob_f)
    blob = open(blob_f, "rb").read()
    dis = subprocess.run([tool("objdump"), "-d", elf], capture_output=True, text=True).stdout
    assert "(r13)" not in dis and "(r2)" not in dis, "the pack uses the game's small-data registers"
    return blob, syms, dis


def bl(site, target):
    off = target - site
    assert -0x2000000 <= off < 0x2000000
    return 0x48000001 | (off & 0x03FFFFFC)


def make_dol(blob):
    d = bytearray(open(DOL, "rb").read())
    offs = list(struct.unpack_from(">18I", d, 0))
    addrs = list(struct.unpack_from(">18I", d, 0x48))
    sizes = list(struct.unpack_from(">18I", d, 0x90))
    bss, bss_size = struct.unpack_from(">II", d, 0xD8)
    assert BASE >= STACK_END >= bss + bss_size, "the section must sit above bss and the stack"
    slot = next(i for i in range(7) if sizes[i] == 0)
    for site, old, entry in SITES:
        i = next(i for i in range(18) if sizes[i] and addrs[i] <= site < addrs[i] + sizes[i])
        fo = offs[i] + site - addrs[i]
        cur = struct.unpack_from(">I", d, fo)[0]
        assert cur == bl(site, old), f"{site:#x} holds {cur:08X}"
        struct.pack_into(">I", d, fo, bl(site, ENTRY[entry]))
    while len(d) % 32:
        d.append(0)
    data = blob + b"\0" * (-len(blob) % 32)
    offs[slot], addrs[slot], sizes[slot] = len(d), BASE, len(data)
    d += data
    struct.pack_into(">18I", d, 0, *offs)
    struct.pack_into(">18I", d, 0x48, *addrs)
    struct.pack_into(">18I", d, 0x90, *sizes)
    return bytes(d), slot


def main(out):
    tmp = os.path.join(out, "_obj")
    os.makedirs(tmp, exist_ok=True)
    blob, syms, dis = build_blob(tmp)
    dol, slot = make_dol(blob)
    pack = os.path.join(out, "batman_pad")
    os.makedirs(pack, exist_ok=True)
    os.makedirs(os.path.join(out, "riivolution"), exist_ok=True)
    open(os.path.join(pack, "main.dol"), "wb").write(dol)
    open(os.path.join(tmp, "gcpad.dis"), "w").write(dis)
    xml = ('<wiidisc version="1">\n'
           '  <!-- Built by src/build.py: main.dol with a GameCube '
           'pad section -->\n'
           '  <id game="S3BE"/>\n  <options>\n'
           '    <section name="Batman: The Brave and the Bold">\n'
           '      <option name="GameCube controllers (WUP-028)">\n'
           '        <choice name="On (ports 1-2 = players 1-2)"><patch id="batman_pad"/></choice>\n'
           '      </option>\n    </section>\n  </options>\n'
           '  <patch id="batman_pad" root="/batman_pad">\n'
           '    <file disc="main.dol" external="main.dol"/>\n'
           '  </patch>\n</wiidisc>\n')
    open(os.path.join(out, "riivolution", "batman_pad.xml"), "w", newline="\n").write(xml)
    print(f"section {len(blob)} bytes at {BASE:#x}-{BASE + len(blob):#x} in DOL text slot {slot}; "
          f"heap from {(syms['__image_end'] + 31) & ~31:#x}")
    for k in ("arena_fix", "kpad_hook", "probe_hook", "PADRead", "si_read", "pad_memset"):
        print(f"  {k} {syms[k]:#x}")


if __name__ == "__main__":
    main(os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "build")))
