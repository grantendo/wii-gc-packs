"""Build the Star Wars: The Force Unleashed II GameCube-controller pack: a
main.dol with one more text section (gcpad.c + entry.S at 0x8071DDA0) and
five of the game's calls pointed into it, plus a Riivolution XML that swaps
in that main.dol and offers tap jump per controller port.

  - 0x8043A888 OSInit's OSSetArenaLo          -> arena_fix (heap above the section)
  - 0x802641DC the per-frame routine's KPADRead -> kpad_hook
  - 0x8026431C, 0x802645B0 its WPADProbe calls  -> probe_hook
  - 0x8026453C its WPAD sync-device call        -> sync_hook

Usage: build.py [out dir]   (default: swtfu2-sfue64/build-pad)
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
BASE = 0x8071DDA0
STACK_END = 0x8071DD90
ENTRY = {"arena_fix": BASE, "kpad_hook": BASE + 4, "probe_hook": BASE + 8, "sync_hook": BASE + 12}
TAP_JUMP = BASE + 0x10  # one byte per port
SITES = [  # (address, what it calls now, our entry)
    (0x8043A888, 0x8043C100, "arena_fix"),
    (0x802641DC, 0x804312E0, "kpad_hook"),
    (0x8026431C, 0x804574D0, "probe_hook"),
    (0x802645B0, 0x804574D0, "probe_hook"),
    (0x8026453C, 0x80457850, "sync_hook"),
]


def run(*args):
    subprocess.run(args, check=True, cwd=HERE)


def build_blob(tmp):
    cflags = ["-O2", "-mcpu=750", "-meabi", "-mhard-float", "-ffreestanding", "-fno-builtin",
              "-fno-tree-loop-distribute-patterns", "-nostdlib", "-fno-pic", "-G0", "-msdata=none",
              "-fno-exceptions", "-Wall", "-Werror"]
    gcc = os.path.join(DKP, "powerpc-eabi-gcc.exe")
    run(gcc, *cflags, "-c", "gcpad.c", "-o", os.path.join(tmp, "gcpad.o"))
    run(gcc, "-c", "entry.S", "-o", os.path.join(tmp, "entry.o"))
    elf = os.path.join(tmp, "gcpad.elf")
    run(os.path.join(DKP, "powerpc-eabi-ld.exe"), "-T", "gcpad.ld", "-o", elf,
        os.path.join(tmp, "entry.o"), os.path.join(tmp, "gcpad.o"))
    nm = subprocess.run([os.path.join(DKP, "powerpc-eabi-nm.exe"), elf], capture_output=True, text=True,
                        check=True).stdout
    syms = {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}
    binf = os.path.join(tmp, "gcpad.bin")
    run(os.path.join(DKP, "powerpc-eabi-objcopy.exe"), "-O", "binary", "-j", ".image", elf, binf)
    blob = open(binf, "rb").read()
    assert syms["_entry"] == BASE, hex(syms["_entry"])
    assert syms["tap_jump"] == TAP_JUMP, hex(syms["tap_jump"])
    dis = subprocess.run([os.path.join(DKP, "powerpc-eabi-objdump.exe"), "-d", elf], capture_output=True,
                         text=True).stdout
    assert "(r13)" not in dis and "(r2)" not in dis, "the section uses the game's small-data registers"
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


def xml():
    out = ['<wiidisc version="1">',
           '  <!-- Built by src/build.py: main.dol with a GameCube pad section.',
           '       P1-P4 are the controller ports (adapter or console). -->',
           '  <id game="SFUE"/>', '  <options>', '    <section name="Star Wars: The Force Unleashed II">',
           '      <option name="GameCube controllers (Multiplayer, test)">',
           '        <choice name="Enabled"><patch id="swtfu2_pad"/></choice>', '      </option>', '    </section>']
    # One section per port, as in the Mario Kart Wii trick pack (vwii-gc-tricks.xml): an id so RiftWii remembers
    # each choice by name across pack versions, and off unless turned on. Each choice brings the pack's main.dol
    # too: the byte it writes is in the new section, which only that main.dol has.
    for port in range(1, 5):
        out += [f'    <section name="GameCube controllers: P{port} (controller port {port})">',
                f'      <option name="P{port}: Tap jump" id="swtfu2_p{port}_tap" default="0">',
                f'        <choice name="On"><patch id="swtfu2_pad"/><patch id="swtfu2_tap{port}"/></choice>',
                '      </option>', '    </section>']
    out += ['  </options>', '  <patch id="swtfu2_pad" root="/swtfu2_pad">',
            '    <file disc="main.dol" external="main.dol"/>', '  </patch>']
    for port in range(1, 5):
        out += [f'  <patch id="swtfu2_tap{port}">',
                f'    <memory offset="0x{TAP_JUMP + port - 1:08X}" value="01" original="00"/>', '  </patch>']
    out += ['</wiidisc>', '']
    return "\n".join(out)


def main(out):
    tmp = os.path.join(out, "_obj")
    os.makedirs(tmp, exist_ok=True)
    blob, syms, dis = build_blob(tmp)
    dol, slot = make_dol(blob)
    pack = os.path.join(out, "swtfu2_pad")
    os.makedirs(pack, exist_ok=True)
    os.makedirs(os.path.join(out, "riivolution"), exist_ok=True)
    open(os.path.join(pack, "main.dol"), "wb").write(dol)
    open(os.path.join(tmp, "gcpad.dis"), "w").write(dis)
    open(os.path.join(out, "riivolution", "swtfu2_pad.xml"), "w", newline="\n").write(xml())
    print(f"section {len(blob)} bytes at {BASE:#x}-{BASE + len(blob):#x} in DOL text slot {slot}; "
          f"heap from {(syms['__image_end'] + 31) & ~31:#x}")
    for k in ("arena_fix", "kpad_hook", "probe_hook", "sync_hook", "tap_jump"):
        print(f"  {k} {syms[k]:#x}")


if __name__ == "__main__":
    main(os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "build")))
