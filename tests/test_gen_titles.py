import unittest

from tooling import load

gen = load('tools/gen_titles.py')

TITLES = 'id\tname\n1\tFodderchief Flayer\n36\tSealer of the Portal of the Gods\n'


class Generate(unittest.TestCase):
    def test_titles_in_order(self):
        text = gen.generate(gen.parse_titles(TITLES))
        self.assertIn('    {1, "Fodderchief Flayer"},\n    {36, "Sealer of the Portal of the Gods"},\n', text)

    def test_a_file_without_its_header_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse_titles(TITLES.replace('id\tname', 'title\tname'))
        with self.assertRaises(gen.DataError):
            gen.parse_titles('')

    def test_an_id_out_of_order_is_an_error(self):
        with self.assertRaises(gen.DataError) as raised:
            gen.parse_titles(TITLES + '2\tWarchief Wrecker\n')
        self.assertIn('line 4', str(raised.exception))
        with self.assertRaises(gen.DataError):
            gen.parse_titles(TITLES + '36\tAgain\n')

    def test_a_row_that_is_not_an_id_and_a_name_is_an_error(self):
        for row in ['x\tFodderchief Flayer\n', '37\n', '37\t\n', '37\tA\tB\n']:
            with self.assertRaises(gen.DataError):
                gen.parse_titles(TITLES + row)
