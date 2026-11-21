"""DOL helpers for SKREG9 main.dol (copied from the Super Climax Heroes work folder): sections, reads, and a cached full
disassembly (objdump) with branch targets, for searching by hand."""
import os, re, struct, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
# The game's own main.dol, from your disc: set MAIN_DOL, or put it next to this script as main.dol.
DOL = os.environ.get("MAIN_DOL", os.path.join(os.path.dirname(os.path.abspath(__file__)), "main.dol"))
OBJDUMP = r"C:\devkitPro\devkitPPC\bin\powerpc-eabi-objdump.exe"
CACHE = os.path.join(tempfile.gettempdir(), "skreg9_dis.txt")
data = open(DOL, "rb").read()


def sections():
    offs = struct.unpack(">18I", data[0:72]); addrs = struct.unpack(">18I", data[72:144]); sizes = struct.unpack(">18I", data[144:216])
    out = [(i, o, a, s) for i, (o, a, s) in enumerate(zip(offs, addrs, sizes)) if s]
    bss = struct.unpack(">II", data[216:224])
    return out, bss


def read(addr, n):
    for i, o, a, s in sections()[0]:
        if a <= addr < a + s:
            return data[o + addr - a: o + addr - a + n]
    raise ValueError(hex(addr))


def u32(addr):
    return struct.unpack(">I", read(addr, 4))[0]


def disassemble():
    """Return {addr: (word, text)} for every text section, cached on disk."""
    if not os.path.exists(CACHE):
        lines = []
        for i, o, a, s in sections()[0]:
            if i >= 7:          # data sections
                continue
            with tempfile.NamedTemporaryFile(delete=False, suffix=".bin") as f:
                f.write(data[o:o + s])
            out = subprocess.run([OBJDUMP, "-D", "-b", "binary", "-m", "powerpc:750", "-EB",
                                  f"--adjust-vma={a:#x}", f.name], capture_output=True, text=True).stdout
            os.unlink(f.name)
            lines += [l for l in out.splitlines() if re.match(r"\s*8[0-9a-f]{7}:", l)]
        open(CACHE, "w").write("\n".join(lines))
    res = {}
    for l in open(CACHE):
        m = re.match(r"\s*(8[0-9a-f]{7}):\s+((?:[0-9a-f]{2} ){4})\s*(.*)", l)
        if m:
            res[int(m.group(1), 16)] = (int(m.group(2).replace(" ", ""), 16), m.group(3).strip())
    return res


def branch_target(addr, word):
    op = word >> 26
    if op == 18:
        li = word & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        return (li if word & 2 else addr + li) & 0xFFFFFFFF
    return None


def callers(target, dis=None):
    dis = dis or disassemble()
    return [a for a, (w, _) in dis.items() if (w >> 26) == 18 and (w & 1) and branch_target(a, w) == target]


def func_start(addr, dis=None):
    """Walk back to the instruction after the previous blr (rough)."""
    dis = dis or disassemble()
    a = addr - 4
    while a in dis and dis[a][0] != 0x4E800020:
        a -= 4
    return a + 4


def string_refs(saddr, dis=None):
    """Instructions forming saddr via lis/addi (or ori) pairs."""
    dis = dis or disassemble()
    hi, lo = (saddr + 0x8000) >> 16, saddr & 0xFFFF
    out = []
    for a, (w, t) in dis.items():
        if (w >> 26) == 14 and (w & 0xFFFF) == lo:  # addi rD,rA,lo
            ra = (w >> 16) & 31
            for back in range(1, 12):
                p = a - 4 * back
                if p in dis:
                    pw = dis[p][0]
                    if (pw >> 26) == 15 and ((pw >> 21) & 31) == ra and (pw & 0xFFFF) == hi:
                        out.append(a)
                        break
    return out
