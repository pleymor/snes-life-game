#!/usr/bin/env python3
"""Convertit les modules IT du mode échantillons au mode instruments.

snesmod (PVSnesLib) ne joue que les modules en mode instruments : un module
en mode échantillons se charge mais reste muet. La conversion ajoute un
instrument par échantillon, qui renvoie toutes les notes vers cet
échantillon ; les motifs ne changent pas, le numéro d'échantillon de la
colonne d'instrument devenant le numéro d'instrument. Un module déjà en
mode instruments est recopié tel quel. Déterministe.

Le pilote de snesmod ne déclenche jamais une note portant l'effet G
(portamento) : il fait seulement glisser la voix déjà en cours. Impulse
Tracker, lui, démarre normalement une telle note sur une voix muette. Une
note avec G jouée sur une voix muette, dans l'ordre de lecture, perd donc son
G : elle sonne pareil dans un tracker, et snesmod la joue enfin.

Lit data/audio/<nom>.it (tel que téléchargé), écrit data/audio/snes/<nom>.it.
"""
import os, struct

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
MUSIC = ["alonely", "offerthelight", "purity"]
INS_SIZE = 554                  # instrument IT « nouveau format » (IMPI)
USE_INSTRUMENTS = 0x04          # drapeau d'en-tête
CMD_G = 7                       # portamento vers la note
CMD_B, CMD_C = 2, 3             # saut de position, fin de motif
NOTE_STOPS = (254, 255, 246)    # coupure, relâchement, fondu

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

def unpack_pattern(data, ptr):
    """Décode le motif IT pointé par ptr.

    Renvoie une liste de lignes ; chaque ligne associe à un canal (0-based)
    [note, instrument, volume, commande, paramètre], None pour un champ
    absent. La mémoire par canal du format (masque et valeurs repris de la
    cellule précédente) est résolue.
    """
    length, nrows = struct.unpack_from("<HH", data, ptr)
    q, end = ptr + 8, ptr + 8 + length
    rows, row = [], {}
    mask, last = {}, {}
    while len(rows) < nrows and q < end:
        cv = data[q]
        q += 1
        if cv == 0:
            rows.append(row)
            row = {}
            continue
        ch = (cv - 1) & 63
        if cv & 0x80:
            mask[ch] = data[q]
            q += 1
        m = mask[ch]
        mem = last.setdefault(ch, [None] * 5)
        cell = [None] * 5
        for bit, field in ((1, 0), (2, 1), (4, 2)):
            if m & bit:
                mem[field] = data[q]
                q += 1
            if m & (bit | bit << 4):
                cell[field] = mem[field]
        if m & 8:
            mem[3], mem[4] = data[q], data[q + 1]
            q += 2
        if m & 0x88:
            cell[3], cell[4] = mem[3], mem[4]
        row[ch] = cell
    rows.extend({} for _ in range(nrows - len(rows)))
    return rows

def pack_pattern(rows):
    """Encode des lignes (format d'unpack_pattern) en motif IT, en-tête compris.

    Chaque cellule est écrite en entier, sans mémoire de canal.
    """
    body = bytearray()
    for row in rows:
        for ch in sorted(row):
            cell = row[ch]
            m = (1 if cell[0] is not None else 0) | (2 if cell[1] is not None else 0) \
                | (4 if cell[2] is not None else 0) | (8 if cell[3] is not None else 0)
            if not m:
                continue
            body += bytes([(ch + 1) | 0x80, m])
            body += bytes(v for v in cell[:3] if v is not None)
            if m & 8:
                body += bytes(cell[3:5])
        body.append(0)
    return struct.pack("<HH4x", len(body), len(rows)) + bytes(body)

def silent_portamento_notes(data):
    """Cellules (motif, ligne, canal) d'une note avec G jouée sur une voix
    muette, en suivant l'ordre de lecture (sauts B et fins de motif C compris)."""
    ordnum, insnum, smpnum, patnum = struct.unpack_from("<HHHH", data, 0x20)
    orders = list(data[0xC0:0xC0 + ordnum])
    ptrs = struct.unpack_from("<%dI" % patnum, data, 0xC0 + ordnum + 4 * (insnum + smpnum))
    pats = [unpack_pattern(data, p) if p else None for p in ptrs]
    playing, found, seen = set(), set(), set()
    pos, row = 0, 0
    while pos < len(orders) and orders[pos] != 255:
        if orders[pos] == 254 or orders[pos] >= patnum or pats[orders[pos]] is None:
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
            if cmd == CMD_C:
                nxt = (pos + 1, param)
            elif cmd == CMD_B:
                nxt = (param, 0)
        pos, row = nxt
    return found

def start_portamento_notes(data):
    """Retire le G des notes jouées sur une voix muette (voir l'en-tête).

    Un motif modifié est réécrit à sa place et ce qui le suit est décalé :
    rien n'est ajouté en fin de fichier, où OpenMPT range ses extensions
    (niveaux de mixage...). Les autres motifs restent octet pour octet.
    """
    cells = silent_portamento_notes(data)
    if not cells:
        return data
    ordnum, insnum, smpnum, patnum = struct.unpack_from("<HHHH", data, 0x20)
    table = 0xC0 + ordnum + 4 * (insnum + smpnum)
    out = bytearray(data)
    for k in sorted({c[0] for c in cells}):
        ptr = struct.unpack_from("<I", out, table + 4 * k)[0]
        rows = unpack_pattern(out, ptr)
        for pat, r, ch in cells:
            if pat == k:
                rows[r][ch][3:5] = [None, None]
        old = 8 + struct.unpack_from("<H", out, ptr)[0]
        out = splice(out, ptr, old, pack_pattern(rows))
    return bytes(out)

def splice(data, pos, length, blob):
    """Remplace data[pos:pos+length] par blob et recale tous les pointeurs du
    fichier qui visent au-delà (message, instruments, échantillons et leurs
    données, motifs)."""
    ordnum, insnum, smpnum, patnum = struct.unpack_from("<HHHH", data, 0x20)
    delta = len(blob) - length
    out = bytearray(data[:pos]) + bytearray(blob) + bytearray(data[pos + length:])

    def moved(p):
        return p + delta if p > pos else p

    table = 0xC0 + ordnum
    fields = [0x38] + [table + 4 * i for i in range(insnum + smpnum + patnum)]
    for off in fields:
        p = struct.unpack_from("<I", out, off)[0]
        if p:
            struct.pack_into("<I", out, off, moved(p))
    for i in range(smpnum):
        smp = struct.unpack_from("<I", out, table + 4 * (insnum + i))[0]
        p = struct.unpack_from("<I", out, smp + 0x48)[0]
        struct.pack_into("<I", out, smp + 0x48, moved(p))
    return out

def convert(data):
    """Rend un module IT jouable par snesmod (voir l'en-tête du fichier)."""
    return start_portamento_notes(to_instruments(data))

def to_instruments(data):
    """Passe un module du mode échantillons au mode instruments."""
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
