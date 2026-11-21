"""Build the Kamen Rider: Dragon Knight GameCube-controller pack (a port of the Super Climax Heroes
one, (my Kamen Rider: Super Climax Heroes pack): compile gcpad.c into a blob at
0x803CFFE0 (the bottom of the MEM1 arena: OSInit 0x80252168 rounds 0x803CFFD8 up) and write a Riivolution XML that

  - loads the blob (memory valuefile),
  - points four main.dol calls at its entry stubs (each guarded by the
    original instruction, so another version of the game is left alone),
  - writes one settings group per option choice (see gcpad.c: CFG_BASE).

Usage: build.py [out dir]   (default: ../../build-controls)
"""
import os, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dol  # noqa: E402

# devkitPPC: DEVKITPPC if its bin folder is there, else C:\devkitPro\devkitPPC (on Windows the installer sets
# DEVKITPPC to "/opt/devkitpro/devkitPPC", an MSYS path Python can't open). Tools are called with ".exe".
_DKP_ENV = os.environ.get("DEVKITPPC", "")
if not os.path.isdir(os.path.join(_DKP_ENV, "bin")):
    _DKP_ENV = r"C:\devkitPro\devkitPPC"
DKP = os.path.join(_DKP_ENV, "bin")
BASE = 0x803CFFE0
CFG_BASE = 0x803D2000
END = 0x803D2200  # gcpad.c ARENA_END: the settings end here
ENTRY = {"arena_fix": BASE, "kpad_main": BASE + 4, "kpad_retry": BASE + 8, "wpad_probe": BASE + 12}
SITES = [  # (address in main.dol, what it calls now, our entry)
    (0x80252180, 0x80253700, "arena_fix"),   # OSInit: OSSetArenaLo(lo)
    (0x8001CD9C, 0x80248970, "kpad_main"),   # input update: KPADRead
    (0x8001CE4C, 0x80248970, "kpad_retry"),  # the same after KPADReset
    (0x800F74F0, 0x80271CB0, "wpad_probe"),  # HOME Menu list: WPADProbe
]

CL = dict(UP=0x1, LEFT=0x2, ZR=0x4, X=0x8, A=0x10, Y=0x20, B=0x40, ZL=0x80, R=0x200,
          PLUS=0x400, HOME=0x800, MINUS=0x1000, L=0x2000, DOWN=0x4000, RIGHT=0x8000)
SRC = ["A", "B", "X", "Y", "Z", "R", "L", "START", "DU", "DD", "DL", "DR", "CU", "CD", "CL", "CR"]
MAGIC = 0x47434B30


def cl(*names):
    v = 0
    for n in names:
        v |= CL[n]
    return v


# option -> (group, first source, [(choice name, [classic masks...])], default choice 1-based)
# Dragon Knight (Dolphin 16 Nov): its input display shows y, b and ZL as the attack buttons, the three Super Climax
# Heroes uses for light, heavy and Rider Art, so the layouts keep their shape: weak (W) = Classic y, strong (S) =
# Classic b, the card = ZL. What Classic x, minus and the D-pad do in Dragon Knight is still to check.
OPTIONS = [
    ("Face buttons", 0, "A", [
        # sources A, B, X; X is special (Classic a) in both. B light first:
        # combos run light > heavy > special, and B > A > X is one sweep of
        # the thumb (Eighting's own GameCube layout in the Naruto GNT games).
        # In menus A confirms and B goes back whatever is chosen (gcpad.c).
        ("Layout A: B weak, A strong", [cl("B"), cl("Y"), cl("A")]),
        ("Layout B: A weak, B strong", [cl("Y"), cl("B"), cl("A")]),
    ], 1),
    ("Jump", 1, "Y", [  # sources Y, Z, R, L
        ("R jumps, L dashes (Y cancel, Z card)", [cl("ZR"), cl("ZL"), cl("R"), cl("L")]),
        ("Y jumps, L dashes (Z cancel, R card)", [cl("R"), cl("ZR"), cl("ZL"), cl("L")]),
        ("L jumps, R dashes (Y cancel, Z card)", [cl("ZR"), cl("ZL"), cl("L"), cl("R")]),
    ], 3),
    ("Extra buttons", 2, "DU", [
        ("C-stick (D-Up = x + minus)",
         [cl("X", "MINUS"), cl("MINUS"), 0, 0, cl("UP"), cl("DOWN"), cl("LEFT"), cl("RIGHT")]),
        ("D-pad (C-Up = x + minus)",
         [cl("UP"), cl("DOWN"), cl("LEFT"), cl("RIGHT"), cl("X", "MINUS"), cl("MINUS"), 0, 0]),
    ], 1),
]
HOME_CHOICES = [("Start + Z", 0x1010), ("Off", 0)]


def run(*args):
    subprocess.run(args, check=True, cwd=HERE)


def build_blob(tmp):
    cflags = ["-O2", "-mcpu=750", "-meabi", "-mhard-float", "-ffreestanding", "-fno-builtin",
              "-nostdlib", "-fno-pic", "-G0", "-msdata=none", "-fno-exceptions", "-Wall", "-Werror"]
    run(os.path.join(DKP, "powerpc-eabi-gcc.exe"), *cflags, "-c", "gcpad.c", "-o", os.path.join(tmp, "gcpad.o"))
    run(os.path.join(DKP, "powerpc-eabi-gcc.exe"), "-c", "entry.S", "-o", os.path.join(tmp, "entry.o"))
    elf = os.path.join(tmp, "gcpad.elf")
    run(os.path.join(DKP, "powerpc-eabi-ld.exe"), "-T", "gcpad.ld", "-o", elf,
        os.path.join(tmp, "entry.o"), os.path.join(tmp, "gcpad.o"))
    nm = subprocess.run([os.path.join(DKP, "powerpc-eabi-nm.exe"), elf], capture_output=True, text=True, check=True).stdout
    syms = {l.split()[2]: int(l.split()[0], 16) for l in nm.splitlines() if len(l.split()) == 3}
    binf = os.path.join(tmp, "gcpad.bin")
    run(os.path.join(DKP, "powerpc-eabi-objcopy.exe"), "-O", "binary", "-j", ".image", elf, binf)
    blob = open(binf, "rb").read()
    assert syms["_entry"] == BASE, hex(syms["_entry"])
    assert BASE + len(blob) <= CFG_BASE, f"blob ends at {BASE + len(blob):#x}, settings start at {CFG_BASE:#x}"
    # no small-data or absolute references back into the game's r2/r13 areas
    dis = subprocess.run([os.path.join(DKP, "powerpc-eabi-objdump.exe"), "-d", elf], capture_output=True, text=True).stdout
    assert "(r13)" not in dis and "(r2)" not in dis, "blob uses the game's small-data registers"
    return blob, syms


def bl(site, target):
    off = target - site
    assert -0x2000000 <= off < 0x2000000
    return 0x48000001 | (off & 0x03FFFFFC)


def group_words(g, first, masks):
    return [MAGIC + g, SRC.index(first), len(masks)] + masks


def hexwords(ws):
    return "".join(f"{w:08X}" for w in ws)


def main(out):
    tmp = os.path.join(out, "_obj")
    os.makedirs(tmp, exist_ok=True)
    blob, syms = build_blob(tmp)
    pack = os.path.join(out, "krdk_gcpad")
    os.makedirs(pack, exist_ok=True)
    os.makedirs(os.path.join(out, "riivolution"), exist_ok=True)
    open(os.path.join(pack, "gcpad.bin"), "wb").write(blob)

    base = [f'    <memory offset="{BASE:#010x}" valuefile="gcpad.bin"/>']
    for site, old_target, entry in SITES:
        orig = dol.u32(site)
        assert orig == bl(site, old_target), f"{site:#x} holds {orig:08X}"
        base.append(f'    <memory offset="{site:#010x}" value="{bl(site, ENTRY[entry]):08X}" original="{orig:08X}"/>')

    options, patches = [], ['  <patch id="dkgcpad" root="/krdk_gcpad">\n' + "\n".join(base) + "\n  </patch>"]
    options.append('      <option name="GameCube controllers (WUP-028)" default="1">\n'
                   '        <choice name="On (ports 1-2 = players 1-2)"><patch id="dkgcpad"/></choice>\n'
                   '      </option>')
    for name, g, first, choices, default in OPTIONS:
        lines = [f'      <option name="{name}" default="{default}">']
        for i, (cname, masks) in enumerate(choices):
            pid = f"dkgcpad_g{g}_{i}"
            addr = CFG_BASE + g * 0x40
            patches.append(f'  <patch id="{pid}">\n    <memory offset="{addr:#010x}" value="{hexwords(group_words(g, first, masks))}"/>\n  </patch>')
            lines.append(f'        <choice name="{cname}"><patch id="{pid}"/></choice>')
        options.append("\n".join(lines + ["      </option>"]))
    lines = ['      <option name="HOME button" default="1">']
    for i, (cname, combo) in enumerate(HOME_CHOICES):
        pid = f"dkgcpad_home_{i}"
        patches.append(f'  <patch id="{pid}">\n    <memory offset="{CFG_BASE + 4 * 0x40:#010x}" value="{hexwords([MAGIC + 4, combo])}"/>\n  </patch>')
        lines.append(f'        <choice name="{cname}"><patch id="{pid}"/></choice>')
    options.append("\n".join(lines + ["      </option>"]))
    xml = ('<wiidisc version="1">\n'
           '  <!-- Built by src/build.py. SKREG9 only: every main.dol patch checks the original. -->\n'
           '  <id game="SKRE"/>\n  <options>\n'
           '    <section name="Kamen Rider: Dragon Knight: GameCube controllers">\n' + "\n".join(options) +
           '\n    </section>\n  </options>\n' + "\n".join(patches) + "\n</wiidisc>\n")
    open(os.path.join(out, "riivolution", "krdk_gcpad.xml"), "w", newline="\n").write(xml)
    print(f"blob {len(blob)} bytes at {BASE:#x}-{BASE + len(blob):#x} (settings at {CFG_BASE:#x}, arena from {END:#x})")
    for k in ("arena_fix", "kpad_read_main", "kpad_read_retry", "wpad_probe"):
        print(f"  {k} {syms[k]:#x}")
    return xml


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "build")
    print(main(os.path.abspath(out)))
