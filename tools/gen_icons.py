#!/usr/bin/env python3
"""Generate src/generated/icons.inc from the MobDB icons in third_party/mobdb-icons (MIT License, ThornyFFXI) and
XIUI's player icons in third_party/xiui-icons (MIT License, tirem).

Each PNG is decoded here and compiled into the plugin as raw pixels, ready to copy into a Direct3D texture.
"""
import pathlib
import struct
import sys
import zlib

# src/icons.cpp builds its table from these bitmaps in headsup::Icon order: name, and PNG under third_party.
MOBDB = ['AggroNQ', 'AggroHQ', 'PassiveNQ', 'PassiveHQ', 'Link', 'Sight', 'TrueSight', 'Sound', 'Scent', 'Magic', 'JA',
         'Blood']
XIUI = [('Invite', 'invite'), ('Bazaar', 'bazaar'), ('Linkshell', 'linkshell'), ('Away', 'away'), ('Mentor', 'mentor'),
        ('NewAdventurer', 'newadventurer'), ('Gm', 'gm')]
ICONS = [(name, f'mobdb-icons/{name}.png') for name in MOBDB] + [(name, f'xiui-icons/{file}_icon.png') for name, file in XIUI]
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'
RGBA_8_BIT = (8, 6)     # bit depth, color type
PALETTE_8_BIT = (8, 3)
BYTES_PER_PIXEL = 4
MAX_SIDE = 64
# Adam7 interlacing: each pass's first column, first row, column step and row step.
ADAM7 = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]


class IconError(ValueError):
    pass


def paeth(left: int, up: int, up_left: int) -> int:
    p = left + up - up_left
    pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
    return left if pa <= pb and pa <= pc else up if pb <= pc else up_left


def unfilter(raw: bytes, pos: int, width: int, height: int, bpp: int) -> tuple[list[bytearray], int]:
    stride = width * bpp
    rows, prev = [], bytearray(stride)
    for _ in range(height):
        if pos + 1 + stride > len(raw):
            raise IconError('image data ends early')
        kind, row = raw[pos], bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):
            left = row[i - bpp] if i >= bpp else 0
            up_left = prev[i - bpp] if i >= bpp else 0
            predictions = (0, left, prev[i], (left + prev[i]) // 2, paeth(left, prev[i], up_left))
            if kind >= len(predictions):
                raise IconError(f'unknown row filter {kind}')
            row[i] = (row[i] + predictions[kind]) & 0xFF
        rows.append(row)
        prev = row
    return rows, pos


def decode_bgra(data: bytes) -> tuple[int, int, bytes]:
    """Width, height and the pixels of an 8-bit RGBA or palette PNG, rows top to bottom, each pixel B, G, R, A as
    D3DFMT_A8R8G8B8 keeps it in memory."""
    if not data.startswith(PNG_SIGNATURE):
        raise IconError('not a PNG')
    header, compressed, palette, alphas, pos = None, b'', None, b'', len(PNG_SIGNATURE)
    while pos + 8 <= len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            compressed += body
        elif kind == b'PLTE':
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body) - 2, 3)]
        elif kind == b'tRNS':
            alphas = body
        elif kind == b'IEND':
            break
    if header is None:
        raise IconError('no IHDR chunk')
    width, height, depth, color, _, _, interlace = header
    if (depth, color) not in (RGBA_8_BIT, PALETTE_8_BIT):
        raise IconError(f'bit depth {depth} and color type {color}; only 8-bit RGBA and palette are supported')
    if (depth, color) == PALETTE_8_BIT and not palette:
        raise IconError('a palette PNG without its palette')
    if not 0 < width <= MAX_SIDE or not 0 < height <= MAX_SIDE:
        raise IconError(f'{width}x{height} pixels; at most {MAX_SIDE}x{MAX_SIDE}')
    try:
        raw = zlib.decompress(compressed)
    except zlib.error as error:
        raise IconError(f'image data does not decompress: {error}') from None

    bpp = BYTES_PER_PIXEL if palette is None else 1
    rgba, pos = bytearray(width * height * BYTES_PER_PIXEL), 0
    for x0, y0, dx, dy in ADAM7 if interlace else [(0, 0, 1, 1)]:
        columns, lines = range(x0, width, dx), range(y0, height, dy)
        if not columns or not lines:
            continue
        rows, pos = unfilter(raw, pos, len(columns), len(lines), bpp)
        for y, row in zip(lines, rows):
            for i, x in enumerate(columns):
                at = (y * width + x) * BYTES_PER_PIXEL
                if palette is None:
                    rgba[at:at + BYTES_PER_PIXEL] = row[i * BYTES_PER_PIXEL:(i + 1) * BYTES_PER_PIXEL]
                    continue
                index = row[i]
                if index >= len(palette):
                    raise IconError(f'palette index {index} past its {len(palette)} colors')
                rgba[at:at + BYTES_PER_PIXEL] = bytes(palette[index]) + bytes([alphas[index] if index < len(alphas) else 255])
    bgra = bytearray(rgba)
    bgra[0::4], bgra[2::4] = rgba[2::4], rgba[0::4]
    return width, height, bytes(bgra)


def c_bitmap(name: str, width: int, height: int, bgra: bytes) -> str:
    lines = [f'const unsigned char kBgra{name}[] = {{']
    for start in range(0, len(bgra), 16):
        lines.append('    ' + ' '.join(f'0x{b:02x},' for b in bgra[start:start + 16]))
    lines.append('};')
    lines.append(f'const IconBitmap kIcon{name} = {{kBgra{name}, {width}, {height}}};')
    return '\n'.join(lines)


def generate(third_party: pathlib.Path) -> str:
    parts = ['// Generated by tools/gen_icons.py from third_party/mobdb-icons and third_party/xiui-icons. Do not edit.']
    for name, relative in ICONS:
        path = pathlib.Path(third_party) / relative
        if not path.is_file():
            raise IconError(f'missing icon {path}')
        try:
            parts.append(c_bitmap(name, *decode_bgra(path.read_bytes())))
        except IconError as error:
            raise IconError(f'{path}: {error}') from None
    return '\n'.join(parts) + '\n'


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    out = root / 'src' / 'generated' / 'icons.inc'
    try:
        text = generate(root / 'third_party')
    except IconError as error:
        print(f'gen_icons: {error}', file=sys.stderr)
        return 1
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists() or out.read_text(encoding='utf-8') != text:
        out.write_text(text, encoding='utf-8')
    print(f'icons: {len(ICONS)} -> {out}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
