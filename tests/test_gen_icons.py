import pathlib
import re
import struct
import tempfile
import unittest
import zlib

from tooling import load

gen = load('tools/gen_icons.py')

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


class Size(unittest.TestCase):
    def test_icons_are_at_most_64_pixels_a_side(self):
        gen.decode_bgra(png(64, 1, pixels(64, 1)))
        with self.assertRaises(gen.IconError) as caught:
            gen.decode_bgra(png(65, 1, pixels(65, 1)))
        self.assertIn('at most 64x64', str(caught.exception))


def image(width, height, visible):
    """BGRA pixels, transparent but for each (x, y): (alpha) in visible, colored by its place."""
    out = bytearray(width * height * 4)
    for (x, y), alpha in visible.items():
        out[(y * width + x) * 4:(y * width + x + 1) * 4] = bytes([x + 1, y + 1, 9, alpha])
    return bytes(out)


class Trim(unittest.TestCase):
    """Icons fill the same size on screen: each is cut to its visible pixels and centered on a square."""

    def test_the_transparent_margin_is_cut_away(self):
        visible = {(1, 1): 255, (2, 1): 255, (1, 2): 255, (2, 2): 255}
        self.assertEqual(gen.trim_square(4, 4, image(4, 4, visible)),
                         (2, 2, bytes([2, 2, 9, 255, 3, 2, 9, 255, 2, 3, 9, 255, 3, 3, 9, 255])))

    def test_a_wide_icon_is_centered_on_a_square_its_width(self):
        side, _, pixels = gen.trim_square(5, 3, image(5, 3, {(1, 1): 255, (2, 1): 255, (3, 1): 255}))
        self.assertEqual(side, 3)
        self.assertEqual(pixels[3 * 4:6 * 4], bytes([2, 2, 9, 255, 3, 2, 9, 255, 4, 2, 9, 255]))  # the middle row
        self.assertEqual(pixels[:3 * 4] + pixels[6 * 4:], bytes(6 * 4))

    def test_a_tall_icon_is_centered_across_and_rounds_left(self):
        side, _, pixels = gen.trim_square(3, 4, image(3, 4, {(1, 0): 255, (1, 1): 255}))  # 1 wide, 2 tall
        self.assertEqual(side, 2)
        self.assertEqual(pixels, bytes([2, 1, 9, 255, 0, 0, 0, 0, 2, 2, 9, 255, 0, 0, 0, 0]))

    def test_a_soft_edge_counts_as_visible(self):
        self.assertEqual(gen.trim_square(3, 1, image(3, 1, {(0, 0): 1, (2, 0): 255}))[0], 3)

    def test_an_icon_with_nothing_visible_fails(self):
        with self.assertRaises(gen.IconError):
            gen.trim_square(2, 2, bytes(16))


class GenIcons(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = pathlib.Path(self.tmp.name)
        for _, path, _ in gen.ICONS:
            (self.dir / path).parent.mkdir(exist_ok=True)
            (self.dir / path).write_bytes(png(2, 1, bytes([1, 2, 3, 4, 5, 6, 7, 8])))

    def tearDown(self):
        self.tmp.cleanup()

    def test_one_bitmap_per_icon(self):
        text = gen.generate(self.dir)
        # The 2x1 test icon, trimmed to a square: its row, then a transparent one.
        self.assertIn('const unsigned char kBgraAggroNQ[] = {\n    0x03, 0x02, 0x01, 0x04, 0x07, 0x06, 0x05, 0x08, 0x00, 0x00, '
                      '0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\n};\nconst IconBitmap kIconAggroNQ = {kBgraAggroNQ, 2, 2};', text)
        self.assertEqual(len(re.findall(r'const IconBitmap kIcon\w+ = ', text)), len(gen.ICONS))

    def test_the_enum_and_the_table_follow_one_order(self):
        table = gen.generate(self.dir).split('const IconBitmap kIcons[] = {')[1].split('}')[0]
        ids = gen.generate_ids()
        names = [name for name, _, _ in gen.ICONS]
        self.assertEqual([entry.strip() for entry in table.split(',')], [f'kIcon{name}' for name in names])
        self.assertEqual([line.strip().split(',')[0] for line in ids.splitlines() if line.startswith('        ')], names)
        self.assertIn('        Ability, // job abilities and weapon skills', ids)
        self.assertIn(f'constexpr int kIconCount = {len(names)};', ids)

    def test_a_missing_or_unreadable_icon_fails(self):
        (self.dir / 'mobdb-icons' / 'Link.png').write_bytes(b'GIF89a')
        with self.assertRaises(gen.IconError):
            gen.generate(self.dir)
        (self.dir / 'mobdb-icons' / 'Link.png').unlink()
        with self.assertRaises(gen.IconError):
            gen.generate(self.dir)


if __name__ == '__main__':
    unittest.main()
