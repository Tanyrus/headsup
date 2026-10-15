import re
import unittest

from tooling import load

gen = load('tools/gen_views.py')

HIDDEN = 0x0E


def window(short, rect, kind=0):
    return {'short': short, 'rect': rect, 'type': kind, 'closed': 0, 'virtual': 0}


# Two views as the sweep of 2026-10-14 recorded them with HeadsUp unloaded: the strip, the hidden Commands menu and the
# chat log beside what each opened.
STRIP = [window('helpwind', [130, 16, 1790, 46]), window('titlewin', [16, 16, 128, 46], HIDDEN),
         window('netstat', [1792, 16, 1904, 46]), window('buff', [142, 48, 382, 52]),
         window('menuwind', [1792, 48, 1904, 250], HIDDEN), window('logwindo', [16, 1058, 1790, 1064])]
SWEEP = {
    'rest': {'windows': STRIP},
    'status': {'windows': STRIP + [window('level', [16, 208, 128, 296]), window('statcom2', [1792, 48, 1904, 266]),
                                   window('persona', [16, 16, 128, 206])]},
    'status/profile': {'windows': STRIP + [window('level', [16, 208, 128, 296]),
                                           window('statcom2', [1792, 48, 1904, 266]),
                                           window('persona', [16, 16, 128, 206]),
                                           window('profile', [130, 48, 382, 226])]},
    'items': {'windows': STRIP + [window('iteminfo', [16, 992, 382, 1064]), window('itmsort2', [1792, 48, 1904, 125]),
                                  window('inventor', [16, 48, 198, 238])]},
    'items/sort': {'windows': STRIP + [window('iteminfo', [16, 992, 382, 1064]),
                                       window('itmsort2', [1792, 48, 1904, 125]),
                                       window('inventor', [16, 48, 198, 238])]},
}
MENUS = {'nodes': [{'id': 'status', 'label': 'Status'}, {'id': 'status/profile', 'label': 'Profile'},
                   {'id': 'items', 'label': 'Items'}, {'id': 'items/sort', 'label': 'Sort'}]}
REFERENCE = {'menu': [1920, 1080], 'bandHelpOn': 62, 'bandHelpOff': 38}


class FromSweep(unittest.TestCase):
    def setUp(self):
        self.data = gen.from_sweep([SWEEP], MENUS, REFERENCE)
        self.views = {v['id']: v for v in self.data['views']}

    def test_a_view_keeps_the_shown_windows_hanging_from_the_top_and_not_the_strip(self):
        self.assertEqual([w['key'] for w in self.views['status']['windows']], ['buff', 'level', 'statcom2', 'persona'])
        self.assertEqual(self.views['status']['windows'][3]['rect'], [16, 16, 128, 206])
        self.assertEqual([w['key'] for w in self.views['items']['windows']], ['buff', 'itmsort2', 'inventor'])

    def test_views_are_named_by_their_way_through_the_menu_and_the_same_windows_are_one_view(self):
        self.assertEqual(self.views['status/profile']['name'], 'Status > Profile')
        self.assertEqual(self.views['rest']['name'], gen.REST_NAME)
        self.assertNotIn('items/sort', self.views)
        self.assertEqual(self.data['reference'], REFERENCE)


class Generate(unittest.TestCase):
    def test_each_view_has_its_highest_top_and_its_windows_padded_as_the_game_names_them(self):
        text = gen.generate(gen.from_sweep([SWEEP], MENUS, REFERENCE))
        self.assertIn('constexpr int kReferenceBandHelpOn = 62;', text)
        self.assertIn('constexpr int kReferenceBandHelpOff = 38;', text)
        views = re.findall(r'\{"([^"]+)", "([^"]+)", (\d+), \{kViewWindows \+ (\d+), (\d+)\}\}', text)
        self.assertEqual(views[1], ('status', 'Status', '16', '1', '4'))
        self.assertEqual(views[3], ('items', 'Items', '48', '10', '3'))
        keys = re.findall(r"\{((?:'.', ){7}'.')\}", text.split('kViewWindows[] = {')[1])
        self.assertEqual(keys[4].replace("'", '').replace(', ', ''), 'persona ')

    def test_a_view_named_twice_or_without_windows_or_a_window_no_game_name_fits_is_an_error(self):
        data = gen.from_sweep([SWEEP], MENUS, REFERENCE)
        for change, words in ((lambda d: d['views'].append(dict(d['views'][0])), 'rest'),
                              (lambda d: d['views'][1].update(windows=[]), 'status'),
                              (lambda d: d['views'][1]['windows'][0].update(key='toolongname'), 'toolongname')):
            broken = gen.from_sweep([SWEEP], MENUS, REFERENCE)
            change(broken)
            with self.assertRaises(gen.DataError) as raised:
                gen.generate(broken)
            self.assertIn(words, str(raised.exception))
        self.assertTrue(gen.generate(data))


if __name__ == '__main__':
    unittest.main()
