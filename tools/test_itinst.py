#!/usr/bin/env python3
"""Checks tools/itinst.py: sample-mode IT modules become instrument-mode
modules that play the same, which snesmod needs to play them at all, and
notes snesmod would never key on (G on a silent channel) lose their G.

The render check covers the instrument conversion only. Dropping G changes
how OpenMPT carries a voice into later portamentos (it restarts the wave where
the original glides: same spectrum, different phase), so the portamento step
is checked cell by cell instead."""
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

CMD_G = 7                       # IT tone portamento
NOTE_STOPS = (254, 255, 246)    # note cut, note off, note fade

def check_pattern_codec():
    """A hand-packed pattern decodes with IT's per-channel value memory."""
    packed = bytes([0x81, 0x0F, 48, 1, 32, CMD_G, 0x90, 0,     # row 0: explicit
                    0x81, 0xF0, 0,                             # row 1: all "last"
                    0x82, 0x01, 50, 0])                        # row 2: ch 1 note
    head = struct.pack("<HH4x", len(packed), 3)
    rows = itinst.unpack_pattern(head + packed, 0)
    want = [{0: [48, 1, 32, CMD_G, 0x90]}, {0: [48, 1, 32, CMD_G, 0x90]}, {1: [50, None, None, None, None]}]
    if rows != want:
        fail("pattern decode: %r" % rows)
    again = itinst.unpack_pattern(itinst.pack_pattern(rows), 0)
    if again != want:
        fail("pattern re-encode: %r" % again)

def patterns(d):
    _, _, _, pat = header(d)
    return [itinst.unpack_pattern(d, p) if p else None for p in pat]

def silent_portamento_notes(d):
    """(pattern, row, channel) of each note carrying G on a channel that is
    not playing at that point of the song: snesmod never keys such a note
    on, while Impulse Tracker starts it normally."""
    ordnum = struct.unpack_from("<H", d, 0x20)[0]
    orders = list(d[0xC0:0xC0 + ordnum])
    pats = patterns(d)
    playing, found, seen = set(), set(), set()
    pos, row = 0, 0
    while pos < len(orders) and orders[pos] != 255:
        if orders[pos] == 254 or pats[orders[pos]] is None:
            pos, row = pos + 1, 0
            continue
        if (pos, row) in seen:
            break
        seen.add((pos, row))
        rows = pats[orders[pos]]
        nxt = (pos, row + 1) if row + 1 < len(rows) else (pos + 1, 0)
        for ch, (note, _, _, cmd, param) in sorted(rows[row].items()):
            if note is not None and note < 120:
                if cmd == CMD_G and ch not in playing:
                    found.add((orders[pos], row, ch))
                playing.add(ch)
            elif note in NOTE_STOPS:
                playing.discard(ch)
            if cmd == 3:                    # Cxx: pattern break
                nxt = (pos + 1, param)
            elif cmd == 2:                  # Bxx: position jump
                nxt = (param, 0)
        pos, row = nxt
    return found

def check_portamento(name, orig, out):
    """Notes snesmod would leave silent lose their G, and nothing else changes."""
    if silent_portamento_notes(out):
        fail("%s: G notes on silent channels: %s" % (name, sorted(silent_portamento_notes(out))))
    stripped = silent_portamento_notes(orig)
    for k, (a, b) in enumerate(zip(patterns(orig), patterns(out))):
        if a is None or b is None:
            if a != b:
                fail("%s: pattern %d presence changed" % (name, k))
            continue
        if len(a) != len(b):
            fail("%s: pattern %d row count changed" % (name, k))
        for r, (ra, rb) in enumerate(zip(a, b)):
            for ch in set(ra) | set(rb):
                ca, cb = ra.get(ch), rb.get(ch)
                if ca == cb:
                    continue
                if (k, r, ch) in stripped and ca[:3] == cb[:3] and ca[3] == CMD_G and cb[3:] == [None, None]:
                    continue
                fail("%s: pattern %d row %d channel %d changed: %r -> %r" % (name, k, r, ch, ca, cb))

def main():
    check_pattern_codec()
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
        check_portamento(name, orig, out)
        committed = os.path.join(ROOT, "data", "audio", "snes", name + ".it")
        if not os.path.exists(committed) or open(committed, "rb").read() != out:
            fail("%s: data/audio/snes/%s.it is missing or stale: run python3 tools/itinst.py" % (name, name))
        if shutil.which("openmpt123"):
            converted = os.path.join(tempfile.mkdtemp(), name + ".it")
            with open(converted, "wb") as f:
                f.write(itinst.to_instruments(orig))
            ref, diff, la, lb = render_rms_diff(src, converted)
            shutil.rmtree(os.path.dirname(converted))
            if la != lb or diff > 0.02 * ref:
                fail("%s: renders differ (rms %.1f, diff %.1f, %d/%d samples)" % (name, ref, diff, la, lb))
    print("test_itinst: %d modules ok" % len(MUSIC))

main()
