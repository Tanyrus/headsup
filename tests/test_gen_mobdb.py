import importlib.util
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
_spec = importlib.util.spec_from_file_location('gen_mobdb', ROOT / 'tools' / 'gen_mobdb.py')
gen = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gen)

SAMPLE = """--Zone: Test
return {
    Names = {
        ['Goblin\\'s Dragonfly'] = { Name='Goblin\\'s Dragonfly', Notorious=false, Aggro=false, Link=true, TrueSight=false, Job=0, MinLevel=23, MaxLevel=25, Drops={} },
        ['Beach Monk'] = { Name='Beach Monk', Notorious=true, Aggro=true, Link=false, TrueSight=false, Job=0, MinLevel=20, MaxLevel=20, Drops={} },
    },
    Indices = {
        [235] = { Name='Ghoul', Notorious=false, Aggro=true, Link=false, TrueSight=false, Job=0, MinLevel=18, MaxLevel=22, Drops={} },
    },
}
"""


class ParseZone(unittest.TestCase):
    def test_sections_are_separated(self):
        by_index, by_name = gen.parse_zone(SAMPLE, 103)
        self.assertEqual([(r['index'], r['name']) for r in by_index], [(235, 'Ghoul')])
        self.assertEqual(sorted(r['name'] for r in by_name), ['Beach Monk', "Goblin's Dragonfly"])

    def test_fields(self):
        _, by_name = gen.parse_zone(SAMPLE, 103)
        monk = next(r for r in by_name if r['name'] == 'Beach Monk')
        self.assertEqual((monk['zone'], monk['index'], monk['aggro'], monk['notorious'], monk['min'], monk['max']),
                         (103, 0, True, True, 20, 20))

    def test_c_output_escapes_quotes_and_backslashes(self):
        self.assertEqual(gen.c_string('a"b\\c'), '"a\\"b\\\\c"')
        self.assertEqual(gen.c_record({'zone': 4, 'index': 12, 'name': "Goblin's", 'aggro': True, 'notorious': False,
                                       'min': 37, 'max': 37}),
                         '    {4, 12, "Goblin\'s", true, false, 37, 37},')


if __name__ == '__main__':
    unittest.main()
