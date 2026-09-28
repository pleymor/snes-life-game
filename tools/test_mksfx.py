#!/usr/bin/env python3
"""Checks tools/mksfx.py: a valid IT module holding the seven effects."""
import os, struct, sys, wave
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mksfx  # noqa: E402

ROOT = os.path.dirname(HERE)
NAMES = ["place", "refuse", "undo", "tick", "menu", "select", "win"]
WAVS = [os.path.join(ROOT, "data", "audio", "sfx", n + ".wav") for n in NAMES]

def fail(msg):
    print("FAIL test_mksfx:", msg)
    sys.exit(1)

def main():
    data = mksfx.build_it(WAVS)
    if data != mksfx.build_it(WAVS):
        fail("two builds differ")
    if data[:4] != b"IMPM":
        fail("no IMPM header")
    ordnum, insnum, smpnum, patnum = struct.unpack_from("<HHHH", data, 0x20)
    if (insnum, smpnum, patnum) != (0, 7, 1):
        fail("counts %r" % ((insnum, smpnum, patnum),))
    ptrs = struct.unpack_from("<7I", data, 0xC0 + ordnum)
    for i, p in enumerate(ptrs):
        if data[p:p + 4] != b"IMPS":
            fail("sample %d header" % i)
        name = data[p + 0x14:p + 0x2E].rstrip(b"\0").decode()
        flags = data[p + 0x12]
        length, _, _, c5 = struct.unpack_from("<IIII", data, p + 0x30)
        with wave.open(WAVS[i], "rb") as w:
            frames = w.getnframes()
        if name != NAMES[i]:
            fail("sample %d named %r" % (i, name))
        if c5 != 8000:
            fail("sample %d at %d Hz" % (i, c5))
        if not (flags & 0x01 and flags & 0x02):
            fail("sample %d flags %#x" % (i, flags))
        if length != frames:
            fail("sample %d length %d != %d" % (i, length, frames))
    committed = os.path.join(ROOT, "data", "audio", "sfx.it")
    if not os.path.exists(committed) or open(committed, "rb").read() != data:
        fail("data/audio/sfx.it is missing or stale: run python3 tools/mksfx.py")
    print("test_mksfx: 7 effects ok")

main()
