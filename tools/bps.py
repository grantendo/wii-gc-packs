"""BPS patches: make one from two files, or apply one (checking both files' CRC32s).

  python bps.py make <original> <patched> <out.bps>
  python bps.py apply <original> <patch.bps> <out>

The format is byuu's BPS (the "BPS1" header, then source/target/metadata sizes, then actions, then the source,
target and patch CRC32s). make writes only two kinds of action: copy a run from the original at the same offset
(SourceRead), or literal bytes (TargetRead). That is plenty for a main.dol whose patches change bytes in place or
append a section. apply handles all four kinds, so patches made by other tools apply too, and refuses an original
whose CRC32 doesn't match the patch's (the wrong game, region or revision).
"""
import sys
import zlib


def _enc(n):
    out = bytearray()
    while True:
        x = n & 0x7F
        n >>= 7
        if n == 0:
            out.append(0x80 | x)
            return bytes(out)
        out.append(x)
        n -= 1


def _dec(data, pos):
    n, shift = 0, 1
    while True:
        x = data[pos]
        pos += 1
        n += (x & 0x7F) * shift
        if x & 0x80:
            return n, pos
        shift <<= 7
        n += shift


def make(source, target):
    out = bytearray(b"BPS1")
    out += _enc(len(source)) + _enc(len(target)) + _enc(0)
    i, n = 0, len(target)
    while i < n:
        j = i
        if i < len(source) and target[i] == source[i]:
            while j < n and j < len(source) and target[j] == source[j]:
                j += 1
            out += _enc(((j - i - 1) << 2) | 0)
        else:
            while j < n and not (j < len(source) and target[j] == source[j]):
                j += 1
            out += _enc(((j - i - 1) << 2) | 1)
            out += target[i:j]
        i = j
    out += zlib.crc32(source).to_bytes(4, "little")
    out += zlib.crc32(target).to_bytes(4, "little")
    out += zlib.crc32(bytes(out)).to_bytes(4, "little")
    return bytes(out)


def apply(source, patch):
    if patch[:4] != b"BPS1":
        raise ValueError("not a BPS patch")
    if zlib.crc32(patch[:-4]) != int.from_bytes(patch[-4:], "little"):
        raise ValueError("the patch file is damaged (its CRC32 doesn't match)")
    want_src = int.from_bytes(patch[-12:-8], "little")
    want_dst = int.from_bytes(patch[-8:-4], "little")
    if zlib.crc32(source) != want_src:
        raise ValueError("this isn't the file the patch is for (CRC32 %08X, the patch wants %08X): "
                         "check the game, its region and its revision" % (zlib.crc32(source), want_src))
    pos = 4
    src_size, pos = _dec(patch, pos)
    dst_size, pos = _dec(patch, pos)
    meta, pos = _dec(patch, pos)
    pos += meta
    out = bytearray()
    src_rel = dst_rel = 0
    end = len(patch) - 12
    while pos < end:
        d, pos = _dec(patch, pos)
        kind, length = d & 3, (d >> 2) + 1
        if kind == 0:
            out += source[len(out):len(out) + length]
        elif kind == 1:
            out += patch[pos:pos + length]
            pos += length
        else:
            off, pos = _dec(patch, pos)
            off = -(off >> 1) if off & 1 else off >> 1
            if kind == 2:
                src_rel += off
                out += source[src_rel:src_rel + length]
                src_rel += length
            else:
                dst_rel += off
                for _ in range(length):
                    out.append(out[dst_rel])
                    dst_rel += 1
    if len(out) != dst_size or zlib.crc32(out) != want_dst:
        raise ValueError("the result doesn't match the patch's CRC32")
    return bytes(out)


def main(argv):
    if len(argv) != 5 or argv[1] not in ("make", "apply"):
        sys.exit(__doc__)
    a = open(argv[2], "rb").read()
    b = open(argv[3], "rb").read()
    try:
        out = make(a, b) if argv[1] == "make" else apply(a, b)
    except ValueError as e:
        sys.exit("bps: " + str(e))
    open(argv[4], "wb").write(out)
    print("bps: wrote %s (%d bytes)" % (argv[4], len(out)))


if __name__ == "__main__":
    main(sys.argv)
