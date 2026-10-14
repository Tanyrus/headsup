import pathlib
import tempfile
import unittest

from tooling import load

gen = load('tools/gen_mobdata.py')

HEADER = ('id\tzone\tname\tminLevel\tmaxLevel\tflags\trespawn\tdetects\texpLevelMod\tplaceholderOf\t'
          'immunity\tsdt\telementRanks\tstatusRanks\tdrops')
NEUTRAL = '\t0\t' + ','.join(['0'] * 12) + '\t' + ','.join(['0'] * 8) + '\t' + ','.join(['0'] * 10) + '\t0'
DOMAN = ('\t12312\t0,2500,0,0,0,0,0,0,0,0,0,0\t-2,-2,-2,-2,-2,-2,-3,4\t0,0,0,0,0,6,6,0,0,0')
GOOD = (HEADER + '\n17190918\t101\tWild Rabbit\t1\t1\t0\t60\t257\t-2\t0' + NEUTRAL + '\n'
        '17191194\t101\tCarrion Worm\t4\t5\t0\t180\t2\t0\t17191196' + DOMAN + '\t1\n'
        '17199648\t103\tGoblin Bounty Hunter\t17\t20\t161\t300\t259\t0\t0' + DOMAN + '\t2\n')
ZEROS = {'immunity': 0, 'sdt': [0] * 12, 'elementRanks': [0] * 8, 'statusRanks': [0] * 10, 'drops': 0}
DROPS_HEADER = 'list\tkind\tgroup\tgroupRate\titem\trate'
# The Carrion Worm's list (East Ronfaure) and a goblin's with a group, ids from Phoenix's sql/item_basic.sql.
DROPS = (DROPS_HEADER + '\n1\tdrop\t0\t0\t768\t150\n1\tsteal\t0\t0\t17296\t0\n1\tdespoil\t0\t0\t640\t0\n'
         '2\tdrop\t0\t0\t12552\t50\n2\tdrop\t1\t100\t4495\t600\n2\tdrop\t1\t100\t0\t400\n'
         '2\tdrop\t2\t50\t4496\t1\n')
LISTS = gen.parse_drops(DROPS)
DOMAN_VALUES = {'immunity': 12312, 'sdt': [0, 2500] + [0] * 10, 'elementRanks': [-2] * 6 + [-3, 4],
                'statusRanks': [0] * 5 + [6, 6] + [0] * 3}


class Parse(unittest.TestCase):
    def test_rows(self):
        self.assertEqual(gen.parse(GOOD), [
            dict({'id': 17190918, 'zone': 101, 'name': 'Wild Rabbit', 'minLevel': 1, 'maxLevel': 1, 'flags': 0,
                  'respawn': 60, 'detects': 257, 'expLevelMod': -2, 'placeholderOf': 0}, **ZEROS),
            dict({'id': 17191194, 'zone': 101, 'name': 'Carrion Worm', 'minLevel': 4, 'maxLevel': 5, 'flags': 0,
                  'respawn': 180, 'detects': 2, 'expLevelMod': 0, 'placeholderOf': 17191196, 'drops': 1},
                 **DOMAN_VALUES),
            dict({'id': 17199648, 'zone': 103, 'name': 'Goblin Bounty Hunter', 'minLevel': 17, 'maxLevel': 20,
                  'flags': 161, 'respawn': 300, 'detects': 259, 'expLevelMod': 0, 'placeholderOf': 0, 'drops': 2},
                 **DOMAN_VALUES)])

    def assertBadRow(self, row, message):
        with self.assertRaises(gen.DataError) as caught:
            gen.parse(HEADER + '\n' + row + '\n', 'x.tsv')
        self.assertIn('x.tsv:2:', str(caught.exception))
        self.assertIn(message, str(caught.exception))

    def test_bad_rows_name_their_line(self):
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t0\t0', 'expected 15 columns')
        self.assertBadRow('1\t103\t\t1\t2\t0\t0\t0\t0\t0' + NEUTRAL, 'empty name')
        self.assertBadRow('1\t103\tA\t1\t300\t0\t0\t0\t0\t0' + NEUTRAL, 'maxLevel must be')
        self.assertBadRow('1\t103\tA\t-1\t2\t0\t0\t0\t0\t0' + NEUTRAL, 'minLevel must be')
        self.assertBadRow('1\t103\tA\t5\t2\t0\t0\t0\t0\t0' + NEUTRAL, 'minLevel is above maxLevel')
        self.assertBadRow('1\t103\tA\t1\t2\t256\t0\t0\t0\t0' + NEUTRAL, 'flags must be')
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t65536\t0\t0' + NEUTRAL, 'detects must be')
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t0\t-32769\t0' + NEUTRAL, 'expLevelMod must be')
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t0\t--2\t0' + NEUTRAL, 'expLevelMod must be')
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t0\t0\t-1' + NEUTRAL, 'placeholderOf must be')
        self.assertBadRow('1\t103\tA\t1\t2\t0\t0\t0\t0\t0' + NEUTRAL[:-1] + '65536', 'drops must be')

    def test_lists_must_have_their_length_and_fit_the_plugin(self):
        base = '1\t103\tA\t1\t2\t0\t0\t0\t0\t0'
        self.assertBadRow(base + '\t-1' + NEUTRAL[2:], 'immunity must be')
        self.assertBadRow(base + '\t0\t' + ','.join(['0'] * 11) + NEUTRAL[NEUTRAL.index('\t', 3):],
                          'sdt must be 12 whole numbers from -32768 to 32767')
        self.assertBadRow(base + '\t0\t' + ','.join(['0'] * 11) + ',40000' + NEUTRAL[NEUTRAL.index('\t', 3):],
                          'sdt must be 12 whole numbers')
        self.assertBadRow(base + '\t0\t' + ','.join(['0'] * 12) + '\t' + ','.join(['0'] * 7) + ',128\t' +
                          ','.join(['0'] * 10) + '\t0', 'elementRanks must be 8 whole numbers from -128 to 127')
        self.assertBadRow(base + '\t0\t' + ','.join(['0'] * 12) + '\t' + ','.join(['0'] * 8) + '\t0,,0\t0',
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
        self.assertEqual(gen.c_record(records[2], 1, 0, 0),
                         '    {17199648u, "Goblin Bounty Hunter", 17, 20, 161, 300u, 259, 0, 0u, 0, 1, 2, 0u},')
        self.assertEqual(gen.c_record(records[0], 0, 3, 240),
                         '    {17190918u, "Wild Rabbit", 1, 1, 0, 60u, 257, -2, 0u, 3, 0, 0, 240u},')

    def test_an_nm_record_counts_the_mobs_that_are_its_placeholders(self):
        worm = '\t101\tCarrion Worm\t4\t5\t0\t180\t2\t0\t17191196' + NEUTRAL + '\n'
        text = gen.generate(gen.parse(HEADER + '\n17191194' + worm + '17191195' + worm +
                                      '17191196\t101\tBigmouth Billy\t9\t9\t8\t0\t2\t0\t0' + NEUTRAL + '\n'), LISTS, 'abc')
        self.assertIn('    {17191194u, "Carrion Worm", 4, 5, 0, 180u, 2, 0, 17191196u, 0, 0, 0, 0u},', text)
        self.assertIn('    {17191196u, "Bigmouth Billy", 9, 9, 8, 0u, 2, 0, 0u, 2, 0, 0, 180u},', text)

    def test_an_nm_record_carries_the_shortest_respawn_of_its_placeholders_on_a_timer(self):
        def billy(*respawns):
            rows = ''.join(f'{17191190 + i}\t101\tCarrion Worm\t4\t5\t0\t{respawn}\t2\t0\t17191196' + NEUTRAL + '\n'
                           for i, respawn in enumerate(respawns))
            text = gen.generate(gen.parse(HEADER + '\n' + rows + '17191196\t101\tBigmouth Billy\t9\t9\t8\t0\t2\t0\t0' +
                                          NEUTRAL + '\n'), LISTS, 'abc')
            return next(line for line in text.splitlines() if '"Bigmouth Billy"' in line)
        self.assertTrue(billy(900, 600).endswith(', 600u},'))  # the earlier is the one not to miss
        self.assertTrue(billy(0, 600).endswith(', 600u},'))  # a placeholder off any timer brings no time
        self.assertTrue(billy(0, 0).endswith(', 0u},'))

    def test_more_placeholders_than_a_byte_holds_is_an_error(self):
        rows = ''.join(f'{1000 + i}\t101\tWorm\t1\t1\t0\t0\t0\t0\t5000' + NEUTRAL + '\n' for i in range(256))
        with self.assertRaises(gen.DataError) as caught:
            gen.generate(gen.parse(HEADER + '\n' + rows + '5000\t101\tNM\t1\t1\t8\t0\t0\t0\t0' + NEUTRAL + '\n'), LISTS, 'x')
        self.assertIn('256 placeholders', str(caught.exception))

    def test_each_distinct_set_of_resistances_is_stored_once_after_the_neutral_one(self):
        text = gen.generate(gen.parse(GOOD), LISTS, 'abc123')
        table = text.split('const MobResistances kMobResistances[] = {\n')[1].split('};')[0].splitlines()
        self.assertEqual(table, ['    {0u, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, '
                                 '{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},',
                                 '    {12312u, {0, 2500, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {-2, -2, -2, -2, -2, -2, -3, 4}, '
                                 '{0, 0, 0, 0, 0, 6, 6, 0, 0, 0}},'])
        self.assertIn('"Wild Rabbit", 1, 1, 0, 60u, 257, -2, 0u, 0, 0, 0, 0u},', text)
        self.assertIn('"Carrion Worm", 4, 5, 0, 180u, 2, 0, 17191196u, 0, 1, 1, 0u},', text)
        self.assertIn('"Goblin Bounty Hunter", 17, 20, 161, 300u, 259, 0, 0u, 0, 1, 2, 0u},', text)

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
        self.assertIn('    enum class DropKind : uint8_t\n    {\n        Drop,\n        Steal,\n        Despoil,\n    };\n', flags)

    def test_generate_names_the_commit(self):
        text = gen.generate(gen.parse(GOOD), LISTS, 'abc123')
        self.assertTrue(text.startswith('// Generated by tools/gen_mobdata.py from data/phoenix_mobs.tsv and '
                                        'data/phoenix_drops.tsv (Phoenix abc123)'))
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


class Drops(unittest.TestCase):
    def test_lists_and_their_entries_in_order(self):
        self.assertEqual(len(LISTS), 2)
        self.assertEqual(LISTS[0], [
            {'list': 1, 'kind': 'drop', 'group': 0, 'groupRate': 0, 'item': 768, 'rate': 150},
            {'list': 1, 'kind': 'steal', 'group': 0, 'groupRate': 0, 'item': 17296, 'rate': 0},
            {'list': 1, 'kind': 'despoil', 'group': 0, 'groupRate': 0, 'item': 640, 'rate': 0}])
        self.assertEqual([(e['group'], e['groupRate'], e['item']) for e in LISTS[1]],
                         [(0, 0, 12552), (1, 100, 4495), (1, 100, 0), (2, 50, 4496)])

    def assertBadDrops(self, rows, message):
        with self.assertRaises(gen.DataError) as caught:
            gen.parse_drops(DROPS_HEADER + '\n' + rows, 'd.tsv')
        self.assertIn('d.tsv:', str(caught.exception))
        self.assertIn(message, str(caught.exception))

    def test_bad_rows_name_their_line(self):
        self.assertBadDrops('1\tdrop\t0\t0\t768\n', 'expected 6 columns')
        self.assertBadDrops('1\tpilfer\t0\t0\t768\t150\n', 'kind must be one of drop, steal, despoil')
        self.assertBadDrops('1\tdrop\t0\t0\t65536\t150\n', 'item must be a whole number from 0 to 65535')
        self.assertBadDrops('1\tdrop\t0\t0\t768\t-1\n', 'rate must be')
        self.assertBadDrops('1\tdrop\t0\t0\t768\t1001\n', "a drop's rate must be at most 1000")
        self.assertBadDrops('1\tdrop\t1\t1001\t768\t1\n', 'groupRate must be a whole number from 0 to 1000')
        self.assertBadDrops('1\tdrop\t0\t100\t768\t150\n', 'an ungrouped entry must have groupRate 0')
        self.assertBadDrops('1\tsteal\t1\t100\t768\t0\n', 'only a drop can be in a group')

    def test_a_despoil_weight_may_pass_a_drop_rate(self):
        self.assertEqual(gen.parse_drops(DROPS_HEADER + '\n1\tdespoil\t0\t0\t640\t2000\n')[0][0]['rate'], 2000)

    def test_lists_and_groups_are_numbered_from_1_with_their_rows_together(self):
        self.assertBadDrops('2\tdrop\t0\t0\t768\t150\n', 'lists must be numbered from 1')
        self.assertBadDrops('1\tdrop\t0\t0\t768\t150\n2\tdrop\t0\t0\t768\t150\n1\tdrop\t0\t0\t1\t1\n',
                            'lists must be numbered from 1')
        self.assertBadDrops('1\tdrop\t2\t100\t768\t1\n', 'groups must be numbered from 1')
        self.assertBadDrops('1\tdrop\t1\t100\t768\t1\n1\tdrop\t0\t0\t1\t1\n1\tdrop\t1\t100\t2\t1\n',
                            'groups must be numbered from 1')
        self.assertBadDrops('1\tdrop\t1\t100\t768\t1\n1\tdrop\t1\t50\t2\t1\n', 'every entry of group 1 must have its')
        # A new list starts its groups again from 1.
        lists = gen.parse_drops(DROPS_HEADER + '\n1\tdrop\t1\t100\t768\t1\n2\tdrop\t1\t50\t768\t1\n')
        self.assertEqual([loot[0]['groupRate'] for loot in lists], [100, 50])

    def test_the_tables_hold_each_list_once_after_the_empty_one(self):
        text = gen.generate(gen.parse(GOOD), LISTS, 'abc123')
        self.assertIn('const MobDrop kMobDrops[] = {\n'
                      '    {768, 150, DropKind::Drop, 0}, {17296, 0, DropKind::Steal, 0}, {640, 0, DropKind::Despoil, 0},\n'
                      '    {12552, 50, DropKind::Drop, 0}, {4495, 600, DropKind::Drop, 1}, {0, 400, DropKind::Drop, 1}, '
                      '{4496, 1, DropKind::Drop, 2},\n};\n', text)
        self.assertIn('const MobDropGroup kMobDropGroups[] = {\n    {0, 0},\n    {100, 1000},\n    {50, 1},\n};\n', text)
        self.assertIn('const MobDropList kMobDropLists[] = {\n    {0u, 0, 0, 0},\n    {0u, 3, 1, 0},\n    {3u, 4, 1, 2},\n};\n',
                      text)

    def test_a_mob_past_the_lists_is_an_error(self):
        with self.assertRaises(gen.DataError) as caught:
            gen.generate(gen.parse(GOOD), LISTS[:1], 'x')
        self.assertIn('mob 17199648 has drop list 2, past the 1', str(caught.exception))

    def test_no_lists_at_all_is_an_error(self):
        with self.assertRaises(gen.DataError) as caught:
            gen.generate(gen.parse(HEADER + '\n1\t1\tA\t1\t1\t0\t0\t0\t0\t0' + NEUTRAL + '\n'), [], 'x')
        self.assertIn('no drop lists', str(caught.exception))

    def test_a_group_heavier_than_its_weight_holds_is_an_error(self):
        rows = ''.join(f'1\tdrop\t1\t100\t{item}\t40000\n' for item in (1, 2))
        with self.assertRaises(gen.DataError) as caught:
            gen.generate(gen.parse(GOOD), gen.parse_drops(DROPS_HEADER + '\n' + rows) * 2, 'x')
        self.assertIn('weighs more than MobDropGroup::weight holds', str(caught.exception))


if __name__ == '__main__':
    unittest.main()
