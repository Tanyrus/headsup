import importlib.util
import struct
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location('gen_icons', ROOT / 'tools' / 'gen_icons.py')
gen = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gen)

PNG = b'\x89PNG\r\n\x1a\n' + struct.pack('>I4sII', 13, b'IHDR', 32, 32)


class GenIcons(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = pathlib.Path(self.tmp.name)
        for n, name in enumerate(gen.ICONS):
            (self.dir / f'{name}.png').write_bytes(PNG + bytes([n]))

    def tearDown(self):
        self.tmp.cleanup()

    def test_one_named_array_per_icon(self):
        text = gen.generate(self.dir)
        self.assertIn('const unsigned char kPngAggroNQ[] = {\n'
                      '    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,\n'
                      '    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00,\n};', text)
        self.assertEqual(text.count('const unsigned char kPng'), len(gen.ICONS))

    def test_missing_or_non_png_files_fail(self):
        (self.dir / 'Link.png').unlink()
        with self.assertRaises(gen.IconError):
            gen.generate(self.dir)
        (self.dir / 'Link.png').write_bytes(b'GIF89a')
        with self.assertRaises(gen.IconError):
            gen.generate(self.dir)
        # The plugin reads each icon's size from the header that follows the signature.
        (self.dir / 'Link.png').write_bytes(PNG[:8] + b'\x00\x00\x00\x0dtEXt')
        with self.assertRaises(gen.IconError):
            gen.generate(self.dir)


if __name__ == '__main__':
    unittest.main()
