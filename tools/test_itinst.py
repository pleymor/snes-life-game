#!/usr/bin/env python3
"""Checks tools/itinst.py: sample-mode IT modules become instrument-mode
modules that play the same, which snesmod needs to play them at all."""
import math, os, shutil, struct, subprocess, sys, tempfile, wave
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import itinst  # noqa: E402

ROOT = os.path.dirname(HERE)
MUSIC = ["alonely", "offerthelight", "purity"]

def fail(msg):
    print("FAIL test_itinst:", msg)
    sys.exit(1)

def header(d):
    ordnum, insnum, smpnum, patnum = struct.unpack_from("<HHHH", d, 0x20)
    flags = struct.unpack_from("<H", d, 0x2C)[0]
    base = 0xC0 + ordnum
    ins = struct.unpack_from("<%dI" % insnum, d, base)
    smp = struct.unpack_from("<%dI" % smpnum, d, base + 4 * insnum)
    pat = struct.unpack_from("<%dI" % patnum, d, base + 4 * insnum + 4 * smpnum)
    return flags, ins, smp, pat

def sample_data(d, ptr):
    flags = d[ptr + 0x12]
    length, _, _, _, _, _, data_ptr = struct.unpack_from("<7I", d, ptr + 0x30)
    width = 2 if flags & 0x02 else 1
    return d[data_ptr:data_ptr + length * width]

def render_rms_diff(a_path, b_path):
    tmp = tempfile.mkdtemp()
    outs = []
    for i, src in enumerate((a_path, b_path)):
        dst = os.path.join(tmp, "m%d.it" % i)
        shutil.copy(src, dst)
        subprocess.run(["openmpt123", "--render", "--force", "--no-float", dst],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        with wave.open(dst + ".wav", "rb") as w:
            outs.append(struct.unpack("<%dh" % (w.getnframes() * w.getnchannels()),
                                      w.readframes(w.getnframes())))
    shutil.rmtree(tmp)
    a, b = outs
    n = min(len(a), len(b))
    ref = math.sqrt(sum(x * x for x in a[:n]) / n)
    diff = math.sqrt(sum((x - y) ** 2 for x, y in zip(a[:n], b[:n])) / n)
    return ref, diff, len(a), len(b)

def main():
    for name in MUSIC:
        src = os.path.join(ROOT, "data", "audio", name + ".it")
        orig = open(src, "rb").read()
        out = itinst.convert(orig)
        if out != itinst.convert(orig):
            fail("%s: two conversions differ" % name)
        oflags, oins, osmp, _ = header(orig)
        flags, ins, smp, pat = header(out)
        if not flags & 0x04:
            fail("%s: not in instrument mode" % name)
        if oflags & 0x04:
            if out != orig:
                fail("%s: instrument-mode module was modified" % name)
        else:
            if len(ins) != len(osmp):
                fail("%s: %d instruments for %d samples" % (name, len(ins), len(osmp)))
            for k, p in enumerate(ins):
                if out[p:p + 4] != b"IMPI":
                    fail("%s: instrument %d header" % (name, k))
                table = out[p + 0x40:p + 0x40 + 240]
                for note in range(120):
                    if table[2 * note] != note or table[2 * note + 1] != k + 1:
                        fail("%s: instrument %d note %d" % (name, k, note))
        for k, p in enumerate(smp):
            if out[p:p + 4] != b"IMPS":
                fail("%s: sample %d header" % (name, k))
            if sample_data(out, p) != sample_data(orig, osmp[k]):
                fail("%s: sample %d data changed" % (name, k))
        for p in pat:
            if p and not (0 < p < len(out)):
                fail("%s: pattern pointer out of file" % name)
        committed = os.path.join(ROOT, "data", "audio", "snes", name + ".it")
        if not os.path.exists(committed) or open(committed, "rb").read() != out:
            fail("%s: data/audio/snes/%s.it is missing or stale: run python3 tools/itinst.py" % (name, name))
        if shutil.which("openmpt123"):
            ref, diff, la, lb = render_rms_diff(src, committed)
            if la != lb or diff > 0.02 * ref:
                fail("%s: renders differ (rms %.1f, diff %.1f, %d/%d samples)" % (name, ref, diff, la, lb))
    print("test_itinst: %d modules ok" % len(MUSIC))

main()
