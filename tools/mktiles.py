#!/usr/bin/env python3
"""Génère data/tiles.bmp : planche 4bpp de 32 emplacements de 8x8.

L'ordre des tuiles suit exactement les constantes TILE_* de src/core/view.h.
Relancer après toute modification de cet ordre.
"""
import struct, pathlib

W_TILES, H_TILES, TS = 16, 2, 8
W, H = W_TILES * TS, H_TILES * TS

PALETTE = [
    (0x10, 0x18, 0x28),   # 0 fond
    (0x2A, 0x34, 0x48),   # 1 point de portée
    (0x3B, 0x82, 0xF6),   # 2 bleu clair
    (0x1D, 0x4E, 0xD8),   # 3 bleu foncé
    (0xEF, 0x44, 0x44),   # 4 rouge clair
    (0xB9, 0x1C, 0x1C),   # 5 rouge foncé
    (0xE8, 0xEC, 0xF4),   # 6 blanc
    (0x4A, 0x55, 0x68),   # 7 gris
]
PALETTE += [(0, 0, 0)] * (16 - len(PALETTE))

FONT = {
 '0': ["#####","#...#","#...#","#...#","#...#","#...#","#####"],
 '1': ["..#..",".##..","..#..","..#..","..#..","..#..",".###."],
 '2': ["#####","....#","....#","#####","#....","#....","#####"],
 '3': ["#####","....#","....#","#####","....#","....#","#####"],
 '4': ["#...#","#...#","#...#","#####","....#","....#","....#"],
 '5': ["#####","#....","#....","#####","....#","....#","#####"],
 '6': ["#####","#....","#....","#####","#...#","#...#","#####"],
 '7': ["#####","....#","....#","...#.","..#..","..#..","..#.."],
 '8': ["#####","#...#","#...#","#####","#...#","#...#","#####"],
 '9': ["#####","#...#","#...#","#####","....#","....#","#####"],
 'R': ["####.","#...#","#...#","####.","#.#..","#..#.","#...#"],
 '/': ["....#","....#","...#.","..#..",".#...","#....","#...."],
 'X': [".....","#...#",".#.#.","..#..",".#.#.","#...#","....."],
 'P': ["####.","#...#","#...#","####.","#....","#....","#...."],
}

def blank():
    return [[0] * TS for _ in range(TS)]

def cell(light, dark):
    """Carré de 6x6 bordé, laissant un pixel de gouttière sur deux côtés."""
    t = blank()
    for y in range(1, 7):
        for x in range(1, 7):
            edge = y in (1, 6) or x in (1, 6)
            t[y][x] = dark if edge else light
    return t

def glyph(ch, colour):
    t = blank()
    for y, row in enumerate(FONT[ch]):
        for x, c in enumerate(row):
            if c == '#':
                t[y][x + 1] = colour
    return t

def disc(colour, filled):
    t = blank()
    pts = [(2,3),(2,4),(3,2),(3,5),(4,2),(4,5),(5,3),(5,4)]
    if filled:
        pts += [(3,3),(3,4),(4,3),(4,4)]
    for y, x in pts:
        t[y][x] = colour
    return t

def range_dot():
    t = blank()
    for y, x in ((3,3),(3,4),(4,3),(4,4)):
        t[y][x] = 1
    return t

def write_indexed_bmp(path, width, height, palette16, img):
    """BMP 8 bits indexé, lignes du bas vers le haut, chaque ligne alignée
    sur 4 octets. `img` est une matrice [height][width] d'index de palette,
    `palette16` une liste de 16 tuples (r, g, b)."""
    row_pad = (-width) % 4
    pixels = b''.join(bytes(img[y]) + b'\0' * row_pad for y in reversed(range(height)))
    palette = b''.join(struct.pack('<BBBB', b, g, r, 0) for (r, g, b) in palette16)
    offset = 14 + 40 + len(palette)
    out = (struct.pack('<2sIHHI', b'BM', offset + len(pixels), 0, 0, offset)
           + struct.pack('<IiiHHIIiiII', 40, width, height, 1, 8, 0, len(pixels), 2835, 2835, 16, 16)
           + palette + pixels)
    path.parent.mkdir(exist_ok=True)
    path.write_bytes(out)

tiles = [blank(), range_dot(), cell(2, 3), cell(4, 5)]      # 0..3
tiles += [glyph(str(d), 6) for d in range(10)]              # 4..13
tiles += [disc(6, True), disc(7, False)]                    # 14, 15
tiles += [glyph('R', 6), glyph('/', 6), glyph('X', 6)]      # 16, 17, 18
tiles += [glyph('P', 6)]                                    # 19
tiles += [blank()] * (W_TILES * H_TILES - len(tiles))

# Composition de la planche
img = [[0] * W for _ in range(H)]
for i, t in enumerate(tiles):
    ox, oy = (i % W_TILES) * TS, (i // W_TILES) * TS
    for y in range(TS):
        for x in range(TS):
            img[oy + y][ox + x] = t[y][x]

tiles_path = pathlib.Path(__file__).resolve().parent.parent / 'data' / 'tiles.bmp'
write_indexed_bmp(tiles_path, W, H, PALETTE, img)
print(f"{tiles_path} : {W}x{H}, {len(tiles)} emplacements")

# Planche de sprites : un unique emplacement 8x8, le cadre du curseur.
# Quatre segments d'angle en blanc (index 1) sur fond transparent (index 0 :
# sur un sprite SNES, la couleur 0 de la palette est toujours transparente,
# quelle que soit sa valeur RGB). Séparée de data/tiles.bmp car sprites et
# fonds ne partagent ni VRAM ni palette sur SNES.
SPRITE_PALETTE = [(0, 0, 0), (0xE8, 0xEC, 0xF4)] + [(0, 0, 0)] * 14

def cursor_frame():
    t = blank()
    corners = [
        (0, 0), (1, 0), (0, 1),   # coin haut-gauche
        (7, 0), (6, 0), (7, 1),   # coin haut-droit
        (0, 7), (1, 7), (0, 6),   # coin bas-gauche
        (7, 7), (6, 7), (7, 6),   # coin bas-droit
    ]
    for x, y in corners:
        t[y][x] = 1
    return t

sprites_path = pathlib.Path(__file__).resolve().parent.parent / 'data' / 'sprites.bmp'
write_indexed_bmp(sprites_path, TS, TS, SPRITE_PALETTE, cursor_frame())
print(f"{sprites_path} : {TS}x{TS}, 1 emplacement (curseur)")
