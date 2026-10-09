import pathlib
import unittest

from tooling import load

gen_fonts = load('tools/gen_fonts.py')


class Generate(unittest.TestCase):
    def setUp(self):
        self.fonts = pathlib.Path(__file__).resolve().parents[1] / 'third_party' / 'fonts'

    def test_every_bundled_font_is_embedded_in_order(self):
        text = gen_fonts.generate(self.fonts)
        self.assertIn('const unsigned char kFontMarcellusSC[] = {', text)
        self.assertIn('const unsigned char kFontCinzel[] = {', text)
        self.assertIn('const unsigned char kFontCormorantSC[] = {', text)
        table = text[text.index('const BundledFont kBundledFonts[]'):]
        self.assertLess(table.index('"Marcellus SC"'), table.index('"Cinzel"'))
        self.assertLess(table.index('"Cinzel"'), table.index('"Cormorant SC"'))

    def test_the_embedded_bytes_are_the_font_file(self):
        text = gen_fonts.generate(self.fonts)
        array = text[text.index('kFontMarcellusSC[] = {'):]
        array = array[array.index('{') + 1:array.index('};')]
        embedded = bytes(int(b, 16) for b in array.replace('\n', ' ').split(',') if b.strip())
        self.assertEqual(embedded, (self.fonts / 'MarcellusSC-Regular.ttf').read_bytes())

    def test_a_missing_font_file_is_an_error(self):
        with self.assertRaises(gen_fonts.FontError) as raised:
            gen_fonts.generate(pathlib.Path('/nonexistent'))
        self.assertIn('MarcellusSC-Regular.ttf', str(raised.exception))
