#!/usr/bin/env python3
"""Convertit les modules IT du mode échantillons au mode instruments.

snesmod (PVSnesLib) ne joue que les modules en mode instruments : un module
en mode échantillons se charge mais reste muet. La conversion ajoute un
instrument par échantillon, qui renvoie toutes les notes vers cet
échantillon ; les motifs ne changent pas, le numéro d'échantillon de la
colonne d'instrument devenant le numéro d'instrument. Un module déjà en
mode instruments est recopié tel quel. Déterministe.

Lit data/audio/<nom>.it (tel que téléchargé), écrit data/audio/snes/<nom>.it.
"""
import os, struct

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
MUSIC = ["alonely", "offerthelight", "purity"]
INS_SIZE = 554                  # instrument IT « nouveau format » (IMPI)
USE_INSTRUMENTS = 0x04          # drapeau d'en-tête

def instrument(k, name):
    """Instrument k (0-based) : toutes les notes jouent l'échantillon k+1."""
    ins = bytearray(INS_SIZE)
    ins[0:4] = b"IMPI"
    ins[0x04:0x10] = name.encode()[:12].ljust(12, b"\0")
    ins[0x11] = 0                                       # NNA : coupe, comme le mode échantillons
    struct.pack_into("<H", ins, 0x14, 0)                # fadeout
    ins[0x17] = 60                                      # centre de séparation : C-5
    ins[0x18] = 128                                     # volume global
    ins[0x19] = 0x80 | 32                               # panoramique par défaut non utilisé
    struct.pack_into("<H", ins, 0x1C, 0x0214)           # version de tracker
    ins[0x1E] = 1                                       # nombre d'échantillons
    ins[0x20:0x3A] = name.encode()[:26].ljust(26, b"\0")
    struct.pack_into("<H", ins, 0x3E, 0xFFFF)           # banque MIDI : aucune
    for note in range(120):
        ins[0x40 + 2 * note] = note
        ins[0x40 + 2 * note + 1] = k + 1
    return bytes(ins)

def convert(data):
    flags = struct.unpack_from("<H", data, 0x2C)[0]
    if flags & USE_INSTRUMENTS:
        return data
    ordnum, insnum, smpnum, patnum = struct.unpack_from("<HHHH", data, 0x20)
    if insnum != 0:
        raise ValueError("sample-mode module with instruments")
    base = 0xC0 + ordnum
    shift = 4 * smpnum                                  # nouveaux pointeurs d'instruments
    smp = list(struct.unpack_from("<%dI" % smpnum, data, base))
    pat = list(struct.unpack_from("<%dI" % patnum, data, base + 4 * smpnum))

    out = bytearray(data[:base]) + bytearray(shift) + bytearray(data[base:])
    # En-tête : nombre d'instruments, drapeau, compatibilité nouveau format.
    struct.pack_into("<H", out, 0x22, smpnum)
    struct.pack_into("<H", out, 0x2C, flags | USE_INSTRUMENTS)
    cmwt = struct.unpack_from("<H", out, 0x2A)[0]
    struct.pack_into("<H", out, 0x2A, max(cmwt, 0x0200))
    # Tout ce qui suit la liste d'ordres a glissé de `shift` octets.
    msg = struct.unpack_from("<I", out, 0x38)[0]
    if msg >= base:
        struct.pack_into("<I", out, 0x38, msg + shift)
    for i, p in enumerate(smp):
        struct.pack_into("<I", out, base + shift + 4 * i, p + shift)
        data_ptr = struct.unpack_from("<I", out, p + shift + 0x48)[0]
        struct.pack_into("<I", out, p + shift + 0x48, data_ptr + shift)
    for i, p in enumerate(pat):
        struct.pack_into("<I", out, base + shift + 4 * smpnum + 4 * i, p + shift if p else 0)
    # Instruments ajoutés en fin de fichier, pointés depuis la liste.
    ins_start = len(out)
    for k in range(smpnum):
        struct.pack_into("<I", out, base + 4 * k, ins_start + INS_SIZE * k)
        name = out[smp[k] + shift + 0x14:smp[k] + shift + 0x2E].rstrip(b"\0").decode("latin-1")
        out += instrument(k, name or "s%d" % (k + 1))
    return bytes(out)

def main():
    os.makedirs(os.path.join(ROOT, "data", "audio", "snes"), exist_ok=True)
    for name in MUSIC:
        src = os.path.join(ROOT, "data", "audio", name + ".it")
        dst = os.path.join(ROOT, "data", "audio", "snes", name + ".it")
        with open(src, "rb") as f:
            out = convert(f.read())
        with open(dst, "wb") as f:
            f.write(out)
        print("%s : %d octets" % (dst, len(out)))

if __name__ == "__main__":
    main()
