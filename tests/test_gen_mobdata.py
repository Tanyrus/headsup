import pathlib
import tempfile
import unittest

from tooling import load

gen = load('tools/gen_mobdata.py')

HEADER = ('id\tzone\tname\tminLevel\tmaxLevel\tflags\trespawn\tdetects\texpLevelMod\tplaceholderOf\t'
          'immunity\tsdt\telementRanks\tstatusRanks')
NEUTRAL = '\t0\t' + ','.join(['0'] * 12) + '\t' + ','.join(['0'] * 8) + '\t' + ','.join(['0'] * 10)
DOMAN = ('\t12312\t0,2500,0,0,0,0,0,0,0,0,0,0\t-2,-2,-2,-2,-2,-2,-3,4\t0,0,0,0,0,6,6,0,0,0')
GOOD = (HEADER + '\n17190918\t101\tWild Rabbit\t1\t1\t0\t60\t257\t-2\t0' + NEUTRAL + '\n'
        '17191194\t101\tCarrion Worm\t4\t5\t0\t180\t2\t0\t17191196' + DOMAN + '\n'
        '17199648\t103\tGoblin Bounty Hunter\t17\t20\t161\t300\t259\t0\t0' + DOMAN + '\n')
ZEROS = {'immunity': 0, 'sdt': [0] * 12, 'elementRanks': [0] * 8, 'statusRanks': [0] * 10}
DOMAN_VALUES = {'immunity': 12312, 'sdt': [0, 2500] + [0] * 10, 'elementRanks': [-2] * 6 + [-3, 4],
                'statusRanks': [0] * 5 + [6, 6] + [0] * 3}


class Parse(unittest.TestCase):
    def test_rows(self):
        self.assertEqual(gen.parse(GOOD), [
            dict({'id': 17190918, 'zone': 101, 'name': 'Wild Rabbit', 'minLevel': 1, 'maxLevel': 1, 'flags': 0,
                  'respawn': 60, 'detects': 257, 'expLevelMod': -2, 'placeholderOf': 0}, **ZEROS),
            dict({'id': 17191194, 'zone': 101, 'name': 'Carrion Worm', 'minLevel': 4, 'maxLevel': 5, 'flags': 0,
                  'respawn': 180, 'detects': 2, 'expLevelMod': 0, 'placeholderOf': 17191196}, **DOMAN_VALUES),
            dict({'id': 17199648, 'zone': 103, 'name': 'Goblin Bounty Hunter', 'minLevel': 17, 'maxLevel': 20,
                  'flags': 161, 'respawn': 300, 'detects': 259, 'expLevelMod': 0, 'placeholderOf': 0}, **DOMAN_VALUES)])

    def assertBadRow(self, row, message):
        with self.assertRaises(gen.DataError) as caught:
            gen.parse(HEADER + '\n' + row + '\n', 'x.tsv')
        self.assertIn('x.tsv:2:', str(caught.exception))
        self.assertIn(message, str(caught.exception))

    def test_bad_rows_name_their_line(self):
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t0\t0', 'expected 14 columns')
        self.assertBadRow('1\t103\t\t1\t2\t0\t0\t0\t0\t0' + NEUTRAL, 'empty name')
        self.assertBadRow('1\t103\tA\t1\t300\t0\t0\t0\t0\t0' + NEUTRAL, 'maxLevel must be')
        self.assertBadRow('1\t103\tA\t-1\t2\t0\t0\t0\t0\t0' + NEUTRAL, 'minLevel must be')
        self.assertBadRow('1\t103\tA\t5\t2\t0\t0\t0\t0\t0' + NEUTRAL, 'minLevel is above maxLevel')
        self.assertBadRow('1\t103\tA\t1\t2\t256\t0\t0\t0\t0' + NEUTRAL, 'flags must be')
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t65536\t0\t0' + NEUTRAL, 'detects must be')
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t0\t-32769\t0' + NEUTRAL, 'expLevelMod must be')
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t0\t--2\t0' + NEUTRAL, 'expLevelMod must be')
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t0\t0\t-1' + NEUTRAL, 'placeholderOf must be')

    def test_lists_must_have_their_length_and_fit_the_plugin(self):
        base = '1\t103\tA\t1\t2\t0\t0\t0\t0\t0'
        self.assertBadRow(base + '\t-1' + NEUTRAL[2:], 'immunity must be')
        self.assertBadRow(base + '\t0\t' + ','.join(['0'] * 11) + NEUTRAL[NEUTRAL.index('\t', 3):],
                          'sdt must be 12 whole numbers from -32768 to 32767')
        self.assertBadRow(base + '\t0\t' + ','.join(['0'] * 11) + ',40000' + NEUTRAL[NEUTRAL.index('\t', 3):],
                          'sdt must be 12 whole numbers')
        self.assertBadRow(base + '\t0\t' + ','.join(['0'] * 12) + '\t' + ','.join(['0'] * 7) + ',128\t' +
                          ','.join(['0'] * 10), 'elementRanks must be 8 whole numbers from -128 to 127')
        self.assertBadRow(base + '\t0\t' + ','.join(['0'] * 12) + '\t' + ','.join(['0'] * 8) + '\t0,,0',
                          'statusRanks must be 10 whole numbers')

    def test_ids_must_increase(self):
        with self.assertRaises(gen.DataError) as caught:
            gen.parse(HEADER + '\n5\t1\tA\t1\t1\t0\t0\t0\t0\t0' + NEUTRAL + '\n5\t1\tB\t1\t1\t0\t0\t0\t0\t0' + NEUTRAL + '\n')
        self.assertIn(':3: ids must be strictly increasing', str(caught.exception))

    def test_wrong_header(self):
        with self.assertRaises(gen.DataError):
            gen.parse('id\tname\n1\tA\n')


class Output(unittest.TestCase):
    def test_c_strings_escape_quotes_backslashes_and_non_ascii(self):
        self.assertEqual(gen.c_string("Do'Bho"), '"Do\'Bho"')
        self.assertEqual(gen.c_string('a"b\\c?'), '"a\\042b\\134c?"')
        self.assertEqual(gen.c_string('é'), '"\\303\\251"')

    def test_record_line(self):
        records = gen.parse(GOOD)
        self.assertEqual(gen.c_record(records[2], 1),
                         '    {17199648u, "Goblin Bounty Hunter", 17, 20, 161, 300u, 259, 0, 0u, 1},')
        self.assertEqual(gen.c_record(records[0], 0), '    {17190918u, "Wild Rabbit", 1, 1, 0, 60u, 257, -2, 0u, 0},')

    def test_each_distinct_set_of_resistances_is_stored_once_after_the_neutral_one(self):
        text = gen.generate(gen.parse(GOOD), 'abc123')
        table = text.split('const MobResistances kMobResistances[] = {\n')[1].split('};')[0].splitlines()
        self.assertEqual(table, ['    {0u, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, '
                                 '{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},',
                                 '    {12312u, {0, 2500, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {-2, -2, -2, -2, -2, -2, -3, 4}, '
                                 '{0, 0, 0, 0, 0, 6, 6, 0, 0, 0}},'])
        self.assertIn('"Wild Rabbit", 1, 1, 0, 60u, 257, -2, 0u, 0},', text)
        self.assertIn('"Carrion Worm", 4, 5, 0, 180u, 2, 0, 17191196u, 1},', text)
        self.assertIn('"Goblin Bounty Hunter", 17, 20, 161, 300u, 259, 0, 0u, 1},', text)

    def test_the_plugin_gets_the_immunity_bits_and_the_list_orders(self):
        flags = gen.generate_flags()
        self.assertIn('    enum MobImmunity : uint32_t\n    {\n        kImmuneAddle = 1,\n        kImmuneGravity = 2,\n', flags)
        self.assertIn('        kImmuneLightSleep = 2048,\n        kImmuneDarkSleep = 4096,\n', flags)
        self.assertIn('        kImmunePetrify = 65536,\n', flags)
        self.assertIn('    enum class Sdt : uint8_t\n    {\n        Slashing,\n        Piercing,\n        Impact,\n'
                      '        H2H,\n        Fire,\n', flags)
        self.assertIn('    constexpr int kSdtCount = 12;\n', flags)
        self.assertIn('    enum class Element : uint8_t\n    {\n        Fire,\n', flags)
        self.assertIn('    constexpr int kElementCount = 8;\n', flags)
        self.assertIn('    enum class StatusRank : uint8_t\n    {\n        Paralyze,\n        Bind,\n        Silence,\n'
                      '        Slow,\n        Poison,\n        LightSleep,\n        DarkSleep,\n        Blind,\n'
                      '        Stun,\n        Gravity,\n    };\n    constexpr int kStatusRankCount = 10;\n', flags)

    def test_generate_names_the_commit(self):
        text = gen.generate(gen.parse(GOOD), 'abc123')
        self.assertTrue(text.startswith('// Generated by tools/gen_mobdata.py from data/phoenix_mobs.tsv (Phoenix abc123)'))
        self.assertIn('const MobRecord kMobs[] = {', text)

    def test_the_plugin_gets_every_bit_under_its_cpp_name(self):
        self.assertIn('    enum MobFlag : uint8_t\n    {\n'
                      '        kMobAggressive = 1,\n        kMobAlwaysAggro = 2,\n        kMobNoAggro = 4,\n'
                      '        kMobNotorious = 8,\n        kMobLink = 32,\n        kMobTrueDetection = 64,\n'
                      '        kMobFollows = 128,\n    };\n', gen.generate_flags())
        self.assertIn('    enum MobDetect : uint16_t\n    {\n'
                      '        kDetectSight = 1,\n        kDetectHearing = 2,\n        kDetectLowHp = 4,\n'
                      '        kDetectMagic = 32,\n        kDetectAbility = 64,\n        kDetectScent = 256,\n    };\n',
                      gen.generate_flags())

    def test_a_meta_file_without_the_commit_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            meta = pathlib.Path(tmp) / 'phoenix_mobs.meta'
            meta.write_text('mobs\t3\n')
            with self.assertRaises(gen.DataError) as caught:
                gen.read_commit(meta)
            self.assertIn('no phoenix_commit line', str(caught.exception))
            meta.write_text('generated\tnow\nphoenix_commit\tabc123\n')
            self.assertEqual(gen.read_commit(meta), 'abc123')


if __name__ == '__main__':
    unittest.main()
