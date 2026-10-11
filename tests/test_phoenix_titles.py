import pathlib
import tempfile
import unittest

from tooling import load

titles = load('tools/phoenix/titles.py')

TITLE_LUA = ('-----------------------------------\n-- Title IDs\n-----------------------------------\n'
             'xi = xi or {}\n\n---@enum xi.title\nxi.title =\n{\n'
             '    SEALER_OF_THE_PORTAL_OF_THE_GODS          = 36,\n'
             '    FODDERCHIEF_FLAYER                        = 1,\n'
             '}\n')
REAL = pathlib.Path.home() / '.cache' / 'headsup-phoenix' / 'server'


def checkout(root: pathlib.Path, text: str = TITLE_LUA) -> pathlib.Path:
    path = root / 'scripts' / 'enum' / 'title.lua'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8')
    return root


class DisplayNames(unittest.TestCase):
    def test_identifiers_become_the_names_players_know(self):
        for identifier, shown in [('FODDERCHIEF_FLAYER', 'Fodderchief Flayer'),
                                  ('SEALER_OF_THE_PORTAL_OF_THE_GODS', 'Sealer of the Portal of the Gods'),
                                  ('A_MOSS_KIND_PERSON', 'A Moss Kind Person'),
                                  ('THE_LOST_ONE_AND_AN_HEIR_TO_A_THRONE_FOR_ON_AT_IN',
                                   'The Lost One and an Heir to a Throne for on at in'),
                                  ('TORCHBEARER_OF_THE_1ST_WALK', 'Torchbearer of the 1st Walk'),
                                  ('MYTHRIL_MUSKETEER_NO_6', 'Mythril Musketeer No 6'),
                                  ('BRANDED_BY_LIGHTNING', 'Branded by Lightning'),
                                  ('TALKS_WITH_TONBERRIES', 'Talks with Tonberries'),
                                  ('CONTRIBUTER_FROM_THE_SHADOWS', 'Contributer from the Shadows'),
                                  ('GET_OVER_HERE', 'Get Over Here')]:
            self.assertEqual(titles.display_name(identifier), shown)


class Build(unittest.TestCase):
    def test_titles_by_number(self):
        with tempfile.TemporaryDirectory() as tmp:
            rows = titles.build(checkout(pathlib.Path(tmp)))
        self.assertEqual(rows, [(1, 'Fodderchief Flayer'), (36, 'Sealer of the Portal of the Gods')])

    def test_a_repeated_number_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), TITLE_LUA.replace('= 36,', '= 1,'))
            with self.assertRaises(titles.TitleDataError) as raised:
                titles.build(root)
        self.assertIn('1', str(raised.exception))

    def test_a_value_that_is_not_a_number_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), TITLE_LUA.replace('= 36,', '= xi.title.FODDERCHIEF_FLAYER,'))
            with self.assertRaises(titles.TitleDataError) as raised:
                titles.build(root)
        self.assertIn('SEALER_OF_THE_PORTAL_OF_THE_GODS', str(raised.exception))

    def test_a_file_with_no_titles_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), 'xi.title =\n{\n}\n')
            with self.assertRaises(titles.TitleDataError):
                titles.build(root)

    def test_writes_the_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp) / 'out'
            titles.write(out, [(1, 'Fodderchief Flayer'), (36, 'Sealer of the Portal of the Gods')])
            self.assertEqual((out / 'phoenix_titles.tsv').read_text(encoding='utf-8'),
                             'id\tname\n1\tFodderchief Flayer\n36\tSealer of the Portal of the Gods\n')

    @unittest.skipUnless((REAL / 'scripts' / 'enum' / 'title.lua').is_file(), 'no Phoenix checkout')
    def test_the_real_checkout(self):
        by_id = dict(titles.build(REAL))
        self.assertEqual(len(by_id), 1100)
        self.assertEqual(by_id[206], 'New Adventurer')
        self.assertEqual(by_id[9], 'Behemoths Bane')  # the identifiers have no apostrophes
