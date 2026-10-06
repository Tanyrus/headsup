import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location('gen_icons', ROOT / 'tools' / 'gen_icons.py')
gen = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gen)

PNG = b'\x89PNG\r\n\x1a\n'


class GenIcons(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = pathlib.Path(self.tmp.name)
        for n, name in enumerate(gen.ICONS):
            (self.dir / f'{name}.png').write_bytes(PNG + bytes([n]))

    def tearDown(self):
        self.tmp.cleanup()

    def test_order_matches_the_icon_enum(self):
        self.assertEqual(gen.ICONS, ['AggroNQ', 'AggroHQ', 'PassiveNQ', 'PassiveHQ', 'Link', 'Sight', 'TrueSight',
                                     'Sound', 'Scent', 'Magic', 'JA', 'Blood'])

    def test_arrays_and_table_in_order(self):
        text = gen.generate(self.dir)
        self.assertIn('const unsigned char kPngAggroNQ[] = {\n    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00,\n};',
                      text)
        table = text[text.index('const IconPng kIconPngs[] = {'):]
        self.assertLess(table.index('kPngAggroNQ'), table.index('kPngBlood'))
        self.assertIn('{kPngBlood, sizeof(kPngBlood)},', table)

    def test_missing_or_non_png_files_fail(self):
        (self.dir / 'Link.png').unlink()
        with self.assertRaises(gen.IconError):
            gen.generate(self.dir)
        (self.dir / 'Link.png').write_bytes(b'GIF89a')
        with self.assertRaises(gen.IconError):
            gen.generate(self.dir)


if __name__ == '__main__':
    unittest.main()
