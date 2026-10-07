import json
import pathlib
import tempfile
import unittest

from tooling import load

compact = load('tools/phoenix/compact.py')
validate = load('tools/phoenix/validate.py')

MOBS = [
    {'id': 17199648, 'zone': 103, 'name': 'Goblin_Bounty_Hunter', 'minLevel': 17, 'maxLevel': 20, 'respawn': 300,
     'aggro': True, 'alwaysAggro': 0, 'noAggro': 0, 'type': 0,
     'link': True, 'detects': 0x001 | 0x002, 'trueDetection': False, 'expLevelMod': 0, 'follows': False,
     'placeholderOf': 17199700},
    {'id': 17199105, 'zone': 103, 'name': 'Stag_Crab', 'minLevel': 15, 'maxLevel': 17, 'respawn': 0,
     'aggro': True, 'alwaysAggro': 1, 'noAggro': 0, 'type': 0x04,
     'link': False, 'detects': 0x002, 'trueDetection': False, 'expLevelMod': 0, 'follows': False, 'placeholderOf': 0},
    {'id': 17272838, 'zone': 121, 'name': 'Guardian_Treant', 'minLevel': 32, 'maxLevel': 32, 'respawn': 960,
     'aggro': False, 'alwaysAggro': 0, 'noAggro': 1, 'type': 0x02 | 0x10,
     'link': False, 'detects': 0x100, 'trueDetection': True, 'expLevelMod': -2, 'follows': True, 'placeholderOf': 0},
]

# As the dump module writes them: differences out past the table, which GetBaseExp clamps to, and cons as numbers.
RULES = {'firstDifference': -3,
         'baseExp': [[0] * 20, [0] * 20, [50] * 20, [100] * 20, [120] * 20, [120] * 20, [120] * 20],
         'difficulty': [{'minExp': 120, 'con': 5}, {'minExp': 100, 'con': 4}],
         'incrediblyEasyPrey': {'minLevel': 255, 'minExp': 255},
         'sittingAnimations': [33, 47]}


def dump(mobs, rules=RULES):
    return {'mobs': mobs, 'rules': rules}


def run_compact(tmp, *dumps):
    paths = []
    for n, contents in enumerate(dumps):
        path = pathlib.Path(tmp) / f'run-{n}.json'
        path.write_text(json.dumps(contents))
        paths.append(path)
    compact.compact(paths, tmp, 'abc123')
    out = pathlib.Path(tmp)
    rows = [line.split('\t') for line in (out / 'phoenix_mobs.tsv').read_text().splitlines()]
    return rows, (out / 'phoenix_mobs.meta').read_text(), json.loads((out / 'phoenix_rules.json').read_text())


class Compact(unittest.TestCase):
    def setUp(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.rows, self.meta, self.rules = run_compact(tmp, dump(MOBS))

    def test_rows_are_sorted_by_id_with_display_names(self):
        self.assertEqual(self.rows[0], ['id', 'zone', 'name', 'minLevel', 'maxLevel', 'flags', 'respawn', 'detects',
                                        'expLevelMod', 'placeholderOf'])
        self.assertEqual([row[0] for row in self.rows[1:]], ['17199105', '17199648', '17272838'])
        self.assertEqual(self.rows[2], ['17199648', '103', 'Goblin Bounty Hunter', '17', '20', '33', '300', '3', '0',
                                        '17199700'])

    def test_flags_respawn_detection_and_the_level_mod(self):
        self.assertEqual(self.rows[1][5:], [str(1 | 2), '0', '2', '0', '0'])  # aggro + always aggro (fished ignored)
        # no aggro + notorious + true detection + follows (battlefield ignored)
        self.assertEqual(self.rows[3][5:], [str(4 | 8 | 64 | 128), '960', '256', '-2', '0'])

    def test_meta(self):
        self.assertIn('phoenix_commit\tabc123\n', self.meta)
        self.assertIn('mobs\t3\n', self.meta)
        self.assertIn('zones\t2\n', self.meta)

    def test_the_table_keeps_one_of_each_clamped_end_row(self):
        self.assertEqual(self.rules['firstDifference'], -2)
        self.assertEqual([row[0] for row in self.rules['baseExp']], [0, 50, 100, 120])

    def test_cons_are_named(self):
        self.assertEqual(self.rules['difficulty'], [{'minExp': 120, 'con': 'Tough'}, {'minExp': 100, 'con': 'EvenMatch'}])
        self.assertEqual(self.rules['incrediblyEasyPrey'], {'minLevel': 255, 'minExp': 255})
        self.assertEqual(self.rules['sittingAnimations'], [33, 47])

    def test_names_with_tabs_are_rejected(self):
        with self.assertRaises(ValueError):
            compact.display_name('Bad\tName')


class Trim(unittest.TestCase):
    def test_a_table_that_never_changes_keeps_one_row(self):
        self.assertEqual(compact.trim(-5, [[7], [7], [7]]), (-5 + 2, [[7]]))

    def test_rows_inside_the_table_are_kept_even_when_equal(self):
        self.assertEqual(compact.trim(0, [[1], [2], [2], [3]]), (0, [[1], [2], [2], [3]]))


def mob(**fields):
    base = {'id': 16797854, 'zone': 5, 'name': 'Ice_Elemental', 'minLevel': 76, 'maxLevel': 77, 'respawn': 300,
            'aggro': False, 'alwaysAggro': 0, 'noAggro': 0, 'type': 0, 'link': False, 'detects': 0x020,
            'trueDetection': False, 'expLevelMod': 0, 'follows': False, 'placeholderOf': 0}
    base.update(fields)
    return base


class MergeSnapshots(unittest.TestCase):
    """Some mobs change aggro at runtime (elementals, Ghrah forms); a mob counts as attacking if any run saw it attack."""

    def compact_rows(self, *mob_lists):
        with tempfile.TemporaryDirectory() as tmp:
            rows, meta, _ = run_compact(tmp, *(dump(mobs) for mobs in mob_lists))
        return rows[1:], meta

    def test_attacking_in_any_snapshot_counts(self):
        rows, _ = self.compact_rows([mob(aggro=False)], [mob(aggro=True)])
        self.assertEqual(rows[0][5], '1')

    def test_a_mob_the_dump_saw_attacking_during_its_run_counts(self):
        # The Ghrahs turn aggressive and back every minute, so a dump can end on their passive form.
        attacking, passive = {'aggro': True, 'alwaysAggro': 0, 'noAggro': 0}, {'aggro': False, 'alwaysAggro': 0, 'noAggro': 0}
        rows, _ = self.compact_rows([mob(aggro=False, seenAttacking=attacking)])
        self.assertEqual(rows[0][5], '1')
        rows, _ = self.compact_rows([mob(aggro=False)], [mob(aggro=False, seenAttacking=attacking)])
        self.assertEqual(rows[0][5], '1')
        rows, _ = self.compact_rows([mob(aggro=False, seenAttacking=passive)])
        self.assertEqual(rows[0][5], '0')

    def test_the_dumped_state_comes_before_the_one_seen_before_the_spawn(self):
        # Lioumere: the run saw it attack before its spawn script set AlwaysAggro.
        spawned = mob(aggro=True, alwaysAggro=1, seenAttacking={'aggro': True, 'alwaysAggro': 0, 'noAggro': 0})
        for snapshots in ([[spawned]], [[mob()], [spawned]]):
            with self.subTest(snapshots=len(snapshots)):
                rows, _ = self.compact_rows(*snapshots)
                self.assertEqual(rows[0][5], str(1 | 2))

    def test_passive_in_every_snapshot_keeps_the_first(self):
        rows, _ = self.compact_rows([mob(aggro=False)], [mob(aggro=True, noAggro=1)])
        self.assertEqual(rows[0][5], '0')

    def test_all_but_the_levels_and_aggro_come_from_the_first_snapshot(self):
        first = mob(minLevel=76, maxLevel=77, link=True, detects=0x002, expLevelMod=-2, placeholderOf=16797860)
        later = mob(zone=6, name='Label_Only', minLevel=75, maxLevel=78, respawn=999, type=0x02, detects=0x101,
                    trueDetection=True, follows=True)
        rows, _ = self.compact_rows([first], [later])
        self.assertEqual(rows[0], ['16797854', '5', 'Ice Elemental', '75', '78', '32', '300', '2', '-2', '16797860'])

    def test_mobs_from_any_snapshot_are_kept_and_counted(self):
        rows, meta = self.compact_rows([mob()], [mob(), mob(id=16797855)])
        self.assertEqual([r[0] for r in rows], ['16797854', '16797855'])
        self.assertIn('dumps\t2\n', meta)


class Rules(unittest.TestCase):
    def assertCompactFails(self, message, *dumps):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(compact.schema.DataError) as caught:
                run_compact(tmp, *dumps)
        self.assertIn(message, str(caught.exception))

    def test_dumps_that_disagree_are_an_error(self):
        other = dict(RULES, sittingAnimations=[33])
        self.assertCompactFails('run-1.json: its rules differ from those of', dump([mob()]), dump([mob()], other))

    def test_an_unknown_difficulty_is_an_error(self):
        broken = dict(RULES, difficulty=[{'minExp': 1, 'con': 8}])
        self.assertCompactFails('8 is not a difficulty', dump([mob()], broken))
        broken = dict(RULES, difficulty=[{'minExp': 1, 'con': -1}])
        self.assertCompactFails('-1 is not a difficulty', dump([mob()], broken))

    def test_rules_the_plugin_cannot_hold_are_an_error(self):
        broken = dict(RULES, baseExp=[[0] * 19])
        self.assertCompactFails('phoenix_rules.json: baseExp row 0 must have 20 columns', dump([mob()], broken))

    def test_a_dump_without_mobs_or_rules_names_its_file(self):
        self.assertCompactFails('run-0.json: not a dump this compact.py reads', {'rules': RULES})
        self.assertCompactFails('run-0.json: not a dump this compact.py reads', [mob()])

    def test_a_mob_without_a_field_names_its_file(self):
        broken = mob()
        del broken['detects'], broken['follows']
        self.assertCompactFails('run-1.json: mob 16797854 has no detects, follows', dump([mob()]), dump([broken]))


class CompactRejects(unittest.TestCase):
    def compact_one(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / 'run-1.json'
            path.write_text(text)
            compact.compact([path], tmp, 'abc123')

    def test_a_value_the_plugin_cannot_hold(self):
        for fields, message in (({'detects': 0x10000}, 'detects must be a whole number from 0 to 65535'),
                                ({'respawn': -1}, 'respawn must be a whole number from 0 to 4294967295')):
            with self.subTest(fields=fields):
                with self.assertRaises(compact.schema.DataError) as caught:
                    self.compact_one(json.dumps(dump([mob(**fields)])))
                self.assertIn(message, str(caught.exception))

    def test_a_truncated_dump_names_its_file(self):
        with self.assertRaises(compact.schema.DataError) as caught:
            self.compact_one(json.dumps(dump([mob()]))[:40])
        self.assertIn('run-1.json: not a complete dump', str(caught.exception))


def record(mob_id, zone, name, lo, hi, flags, respawn, detects, level_mod=0, placeholder_of=0):
    return mob_id, {'id': mob_id, 'zone': zone, 'name': name, 'minLevel': lo, 'maxLevel': hi, 'flags': flags,
                    'respawn': respawn, 'detects': detects, 'expLevelMod': level_mod, 'placeholderOf': placeholder_of}


class Validate(unittest.TestCase):
    RECORDS = dict([
        record(17199105, 103, 'Stag Crab', 15, 17, 3, 0, 2),
        record(17272838, 121, 'Guardian Treant', 32, 32, 12, 0, 2),
        record(17199322, 103, 'Snipper', 19, 20, 0, 300, 2),
        record(17199648, 103, 'Goblin Bounty Hunter', 17, 20, 33, 300, 1),
        record(17190918, 101, 'Wild Rabbit', 1, 1, 0, 60, 257, -2),
        record(17191194, 101, 'Carrion Worm', 4, 5, 0, 180, 2, 0, 17191196),
        record(17191195, 101, 'Carrion Worm', 4, 5, 0, 180, 2, 0, 17191196),
    ])

    def check(self, records, **limits):
        return validate.check(records, **dict(dict(min_mobs=5, min_zones=3), **limits))

    def test_known_spawns_pass(self):
        self.assertEqual(self.check(self.RECORDS), [])

    def test_wrong_level_and_aggro_fail(self):
        records = dict(self.RECORDS)
        records.update([record(17199322, 103, 'Snipper', 19, 21, 1, 300, 2)])
        self.assertEqual(self.check(records), ['17199322: expected Snipper 19-20, got Snipper 19-21',
                                               '17199322 Snipper: expected aggressive=False'])

    def test_missing_link_or_detection_fails(self):
        records = dict(self.RECORDS)
        records.update([record(17199648, 103, 'Goblin Bounty Hunter', 17, 20, 1, 300, 0)])
        self.assertEqual(self.check(records), ['17199648 Goblin Bounty Hunter: expected links=True',
                                               '17199648 Goblin Bounty Hunter: expected detects 1, got 0'])

    def test_a_missing_level_mod_fails(self):
        records = dict(self.RECORDS)
        records.update([record(17190918, 101, 'Wild Rabbit', 1, 1, 0, 60, 257, 0)])
        self.assertEqual(self.check(records), ['17190918: expected level mod -2, got 0'])

    def test_a_missing_placeholder_fails(self):
        records = dict(self.RECORDS)
        records.update([record(17191195, 101, 'Carrion Worm', 4, 5, 0, 180, 2)])
        self.assertEqual(self.check(records), ['17191195: expected a placeholder of 17191196, got 0'])

    def test_too_few_mobs_or_zones_fail(self):
        self.assertEqual(self.check(self.RECORDS, min_mobs=8, min_zones=4),
                         ['only 7 mobs (expected at least 8)', 'only 3 zones (expected at least 4)'])

    def test_load_reads_every_column(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / 'mobs.tsv'
            path.write_text('id\tzone\tname\tminLevel\tmaxLevel\tflags\trespawn\tdetects\texpLevelMod\tplaceholderOf\n'
                            '17191194\t101\tCarrion Worm\t4\t5\t0\t180\t2\t0\t17191196\n')
            self.assertEqual(validate.load(path), dict([record(17191194, 101, 'Carrion Worm', 4, 5, 0, 180, 2, 0, 17191196)]))


class ValidateRules(unittest.TestCase):
    GOOD = {'firstDifference': -1, 'baseExp': [[0] * 20, [100] * 20, [120] * 20],
            'difficulty': [{'minExp': 100, 'con': 'EvenMatch'}], 'incrediblyEasyPrey': {'minLevel': 255, 'minExp': 255},
            'sittingAnimations': [33, 47, 63]}

    def test_known_rules_pass(self):
        self.assertEqual(validate.check_rules(self.GOOD), [])

    def test_an_even_match_must_give_100_in_every_bracket(self):
        uneven = [100] * 19 + [90]
        self.assertEqual(validate.check_rules(dict(self.GOOD, baseExp=[[0] * 20, uneven, [120] * 20])),
                         [f'an even match should give 100 experience in every bracket, got {uneven}'])
        self.assertEqual(validate.check_rules(dict(self.GOOD, firstDifference=1)),  # no row for 0
                         ['an even match should give 100 experience in every bracket, got None'])

    def test_resting_and_sitting_must_count(self):
        self.assertEqual(validate.check_rules(dict(self.GOOD, sittingAnimations=[47, 63])),
                         ['resting (33) and /sit (47) should count as sitting, got [47, 63]'])


if __name__ == '__main__':
    unittest.main()
