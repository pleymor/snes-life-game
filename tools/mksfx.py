#!/usr/bin/env python3
"""Génère data/audio/sfx.it : un module Impulse Tracker sans motif musical
qui ne contient que les sept effets sonores du jeu, dans un ordre fixe.
smconv (PVSnesLib) lit les effets de snesmod dans les échantillons du
premier module de la banque ; l'indice d'un effet est son rang ici.
Déterministe : deux exécutions donnent le même fichier."""
import os, struct, wave

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
NAMES = ["place", "refuse", "undo", "tick", "menu", "select", "win"]

def read_wav(path):
    with wave.open(path, "rb") as w:
        if w.getnchannels() != 1 or w.getsampwidth() != 2:
            raise ValueError("%s: expected mono 16-bit" % path)
        return w.readframes(w.getnframes()), w.getnframes(), w.getframerate()

def build_it(wav_paths):
    samples = [read_wav(p) for p in wav_paths]
    names = [os.path.splitext(os.path.basename(p))[0] for p in wav_paths]
    ordnum, insnum, smpnum, patnum = 2, 0, len(samples), 1
    # En-tête IT (0xC0 octets) : titre, compteurs, version, drapeaux
    # (stéréo + glissements linéaires, pas d'instruments), volumes, tempo.
    hdr = struct.pack("<4s26sHHHHHHHHH", b"IMPM", b"IMMIGRATION SFX", 0x1004,
                      ordnum, insnum, smpnum, patnum, 0x0214, 0x0200, 0x0009, 0)
    hdr += struct.pack("<BBBBBBHI", 128, 48, 6, 125, 128, 0, 0, 0)
    hdr += bytes(4)
    hdr += bytes([32] * 64) + bytes([64] * 64)
    assert len(hdr) == 0xC0
    orders = bytes([0, 255])
    smp_off = 0xC0 + ordnum + 4 * smpnum + 4 * patnum
    pat_off = smp_off + 0x50 * smpnum
    pattern = struct.pack("<HH4s", 64, 64, bytes(4)) + bytes(64)   # 64 lignes vides
    data_off = pat_off + len(pattern)
    out = hdr + orders
    out += b"".join(struct.pack("<I", smp_off + 0x50 * i) for i in range(smpnum))
    out += struct.pack("<I", pat_off)
    heads, datas, cur = b"", b"", data_off
    for (raw, frames, rate), name in zip(samples, names):
        flags = 0x01 | 0x02            # échantillon présent, 16 bits
        heads += struct.pack("<4s12sBBBB26sBBIIIIIIIBBBB",
                             b"IMPS", name.encode()[:12], 0, 64, flags, 64,
                             name.encode()[:26], 0x01, 0,
                             frames, 0, 0, rate, 0, 0, cur, 0, 0, 0, 0)
        datas += raw
        cur += len(raw)
    return out + heads + pattern + datas

def main():
    wavs = [os.path.join(ROOT, "data", "audio", "sfx", n + ".wav") for n in NAMES]
    path = os.path.join(ROOT, "data", "audio", "sfx.it")
    with open(path, "wb") as f:
        f.write(build_it(wavs))
    print("%s : %d effets" % (path, len(wavs)))

if __name__ == "__main__":
    main()
