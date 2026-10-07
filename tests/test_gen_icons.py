import importlib.util
import pathlib
import struct
import tempfile
import unittest
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location('gen_icons', ROOT / 'tools' / 'gen_icons.py')
gen = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gen)

SIGNATURE = b'\x89PNG\r\n\x1a\n'
# The seven Adam7 passes: first column, first row, column step, row step.
ADAM7 = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]


def predictor(kind, left, up, up_left):
    if kind == 1:
        return left
    if kind == 2:
        return up
    if kind == 3:
        return (left + up) // 2
    if kind == 4:
        p = left + up - up_left
        pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
        return left if pa <= pb and pa <= pc else up if pb <= pc else up_left
    return 0


def filtered(row, prev, kind):
    out = bytearray()
    for i, value in enumerate(row):
        left = row[i - 4] if i >= 4 else 0
        up_left = prev[i - 4] if i >= 4 else 0
        out.append((value - predictor(kind, left, prev[i], up_left)) & 0xFF)
    return bytes(out)


def chunk(kind, body):
    return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))


def png(width, height, rgba, interlace=0, kind=0, color_type=6):
    raw = bytearray()
    for x0, y0, dx, dy in ADAM7 if interlace else [(0, 0, 1, 1)]:
        columns = range(x0, width, dx)
        prev = bytes(len(columns) * 4)
        for y in range(y0, height, dy):
            row = b''.join(rgba[(y * width + x) * 4:(y * width + x) * 4 + 4] for x in columns)
            if row:
                raw += bytes([kind]) + filtered(row, prev, kind)
                prev = row
    header = struct.pack('>IIBBBBB', width, height, 8, color_type, 0, 0, interlace)
    return SIGNATURE + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(bytes(raw))) + chunk(b'IEND', b'')


def palette_png(width, height, indices, palette, alphas, with_palette=True):
    raw = b''.join(bytes([0]) + bytes(indices[y * width:(y + 1) * width]) for y in range(height))
    header = struct.pack('>IIBBBBB', width, height, 8, 3, 0, 0, 0)
    colors = chunk(b'PLTE', bytes(v for rgb in palette for v in rgb)) if with_palette else b''
    return (SIGNATURE + chunk(b'IHDR', header) + colors + chunk(b'tRNS', bytes(alphas)) +
            chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def pixels(width, height):
    return bytes((i * 37 + 11) & 0xFF for i in range(width * height * 4))


def bgra(rgba):
    out = bytearray(rgba)
    out[0::4], out[2::4] = rgba[2::4], rgba[0::4]
    return bytes(out)


class Decode(unittest.TestCase):
    def test_every_filter_and_both_layouts_give_the_pixels_back(self):
        rgba = pixels(9, 10)
        for interlace in (0, 1):
            for kind in range(5):
                with self.subTest(interlace=interlace, kind=kind):
                    self.assertEqual(gen.decode_bgra(png(9, 10, rgba, interlace, kind)), (9, 10, bgra(rgba)))

    def test_palette_pngs_use_their_colors_and_transparency(self):
        palette = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
        png = palette_png(3, 1, [0, 1, 2], palette, [0, 128])  # the third color has no alpha entry: opaque
        self.assertEqual(gen.decode_bgra(png), (3, 1, bytes([0, 0, 255, 0, 0, 255, 0, 128, 255, 0, 0, 255])))

    def test_a_palette_png_needs_its_palette(self):
        with self.assertRaises(gen.IconError):
            gen.decode_bgra(palette_png(1, 1, [0], [(1, 2, 3)], [], with_palette=False))
        with self.assertRaises(gen.IconError):
            gen.decode_bgra(palette_png(1, 1, [5], [(1, 2, 3)], []))  # an index past the palette

    def test_only_complete_8_bit_rgba_pngs_are_accepted(self):
        with self.assertRaises(gen.IconError):
            gen.decode_bgra(png(2, 2, bytes(12), color_type=2))
        with self.assertRaises(gen.IconError):
            gen.decode_bgra(b'GIF89a')
        with self.assertRaises(gen.IconError):
            gen.decode_bgra(png(2, 2, pixels(2, 2))[:-30])  # image data cut short


class GenIcons(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = pathlib.Path(self.tmp.name)
        for _, path in gen.ICONS:
            (self.dir / path).parent.mkdir(exist_ok=True)
            (self.dir / path).write_bytes(png(2, 1, bytes([1, 2, 3, 4, 5, 6, 7, 8])))

    def tearDown(self):
        self.tmp.cleanup()

    def test_one_bitmap_per_icon(self):
        text = gen.generate(self.dir)
        self.assertIn('const unsigned char kBgraAggroNQ[] = {\n    0x03, 0x02, 0x01, 0x04, 0x07, 0x06, 0x05, 0x08,\n};\n'
                      'const IconBitmap kIconAggroNQ = {kBgraAggroNQ, 2, 1};', text)
        self.assertEqual(text.count('const IconBitmap kIcon'), len(gen.ICONS))

    def test_a_missing_or_unreadable_icon_fails(self):
        (self.dir / 'mobdb-icons' / 'Link.png').write_bytes(b'GIF89a')
        with self.assertRaises(gen.IconError):
            gen.generate(self.dir)
        (self.dir / 'mobdb-icons' / 'Link.png').unlink()
        with self.assertRaises(gen.IconError):
            gen.generate(self.dir)


if __name__ == '__main__':
    unittest.main()
