import contextlib
import io
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
     'placeholderOf': 17199700, 'immunity': 0x1000 | 0x10, 'sdt': [0, 2500] + [0] * 10,
     'elementRanks': [-2] * 6 + [-3, 0], 'statusRanks': [0] * 10, 'dropList': 1},
    {'id': 17199105, 'zone': 103, 'name': 'Stag_Crab', 'minLevel': 15, 'maxLevel': 17, 'respawn': 0,
     'aggro': True, 'alwaysAggro': 1, 'noAggro': 0, 'type': 0x04,
     'link': False, 'detects': 0x002, 'trueDetection': False, 'expLevelMod': 0, 'follows': False, 'placeholderOf': 0,
     'immunity': 0, 'sdt': [0] * 12, 'elementRanks': [0] * 8, 'statusRanks': [0] * 10, 'dropList': 0},
    {'id': 17272838, 'zone': 121, 'name': 'Guardian_Treant', 'minLevel': 32, 'maxLevel': 32, 'respawn': 960,
     'aggro': False, 'alwaysAggro': 0, 'noAggro': 1, 'type': 0x02 | 0x10,
     'link': False, 'detects': 0x100, 'trueDetection': True, 'expLevelMod': -2, 'follows': True, 'placeholderOf': 0,
     'immunity': 0, 'sdt': [0] * 12, 'elementRanks': [0] * 8, 'statusRanks': [0] * 10},
]

# As the dump module writes them: differences out past the table, which GetBaseExp clamps to, and cons as numbers.
# As the dump module writes them: [DROP_TYPE, item, rate, fixed], 0 normal, 1 grouped, 2 steal, 4 despoil.
DROP_LISTS = [
    {'items': [[0, 768, 150, False], [2, 17296, 0, False], [4, 640, 0, False]], 'groups': []},
    {'items': [[0, 12552, 50, False]],
     'groups': [{'rate': 100, 'fixed': False, 'items': [[1, 4495, 600, False], [1, 0, 400, False]]}]},
]
DROPS_ROWS = [['list', 'kind', 'group', 'groupRate', 'item', 'rate'],
              ['1', 'drop', '0', '0', '768', '150'], ['1', 'steal', '0', '0', '17296', '0'],
              ['1', 'despoil', '0', '0', '640', '0'],
              ['2', 'drop', '0', '0', '12552', '50'], ['2', 'drop', '1', '100', '4495', '600'],
              ['2', 'drop', '1', '100', '0', '400']]

RULES = {'firstDifference': -3,
         'baseExp': [[0] * 20, [0] * 20, [50] * 20, [100] * 20, [120] * 20, [120] * 20, [120] * 20],
         'difficulty': [{'minExp': 120, 'con': 5}, {'minExp': 100, 'con': 4}],
         'incrediblyEasyPrey': {'minLevel': 255, 'minExp': 255},
         'sittingAnimations': [33, 47]}


ZERO_LISTS = ['0', ','.join(['0'] * 12), ','.join(['0'] * 8), ','.join(['0'] * 10)]
ZEROS_RECORD = {'immunity': 0, 'sdt': [0] * 12, 'elementRanks': [0] * 8, 'statusRanks': [0] * 10, 'drops': 0}


# What live_settings.lua sets, as LIVE_SETTINGS_TEXT spells it and a dump writes it back.
LIVE_SETTINGS_TEXT = """-- live's
xi = xi or {}
xi.settings = xi.settings or {}

-- content
xi.settings.main.RESTRICT_CONTENT = 1
xi.settings.main.ENABLE_WOTG      = 0

xi.settings.main.USE_ADOULIN_WEAPON_SKILL_CHANGES = false
xi.settings.map.DROP_RATE_MULTIPLIER = 1.5
"""
SETTINGS = {'main': {'RESTRICT_CONTENT': 1, 'ENABLE_WOTG': 0, 'ENABLE_SOA': 1,
                     'USE_ADOULIN_WEAPON_SKILL_CHANGES': False, 'SERVER_NAME': 'Nameless'},
            'map': {'DROP_RATE_MULTIPLIER': 1.5, 'MOB_STAT_MULTIPLIER': 1.0}}


def dump(mobs, rules=RULES, drop_lists=DROP_LISTS, settings=SETTINGS):
    return {'mobs': mobs, 'dropLists': drop_lists, 'rules': rules, 'settings': settings}


def run_compact(tmp, *dumps):
    paths = []
    for n, contents in enumerate(dumps):
        path = pathlib.Path(tmp) / f'run-{n}.json'
        path.write_text(json.dumps(contents))
        paths.append(path)
    settings = pathlib.Path(tmp) / 'live_settings.lua'
    settings.write_text(LIVE_SETTINGS_TEXT)
    compact.compact(paths, tmp, 'abc123', settings)
    out = pathlib.Path(tmp)
    rows = [line.split('\t') for line in (out / 'phoenix_mobs.tsv').read_text().splitlines()]
    drops = [line.split('\t') for line in (out / 'phoenix_drops.tsv').read_text().splitlines()]
    return rows, (out / 'phoenix_mobs.meta').read_text(), json.loads((out / 'phoenix_rules.json').read_text()), drops


class Compact(unittest.TestCase):
    def setUp(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.rows, self.meta, self.rules, self.drops = run_compact(tmp, dump(MOBS))

    def test_rows_are_sorted_by_id_with_display_names(self):
        self.assertEqual(self.rows[0], ['id', 'zone', 'name', 'minLevel', 'maxLevel', 'flags', 'respawn', 'detects',
                                        'expLevelMod', 'placeholderOf', 'immunity', 'sdt', 'elementRanks', 'statusRanks',
                                        'drops'])
        self.assertEqual([row[0] for row in self.rows[1:]], ['17199105', '17199648', '17272838'])
        self.assertEqual(self.rows[2], ['17199648', '103', 'Goblin Bounty Hunter', '17', '20', '33', '300', '3', '0',
                                        '17199700', '4112', '0,2500,0,0,0,0,0,0,0,0,0,0', '-2,-2,-2,-2,-2,-2,-3,0',
                                        '0,0,0,0,0,0,0,0,0,0', '2'])

    def test_flags_respawn_detection_and_the_level_mod(self):
        self.assertEqual(self.rows[1][5:10], [str(1 | 2), '0', '2', '0', '0'])  # aggro + always aggro (fished ignored)
        # no aggro + notorious + true detection + follows (battlefield ignored)
        self.assertEqual(self.rows[3][5:10], [str(4 | 8 | 64 | 128), '960', '256', '-2', '0'])

    def test_meta(self):
        self.assertIn('phoenix_commit\tabc123\n', self.meta)
        self.assertIn('mobs\t3\n', self.meta)
        self.assertIn('zones\t2\n', self.meta)
        self.assertIn('dropLists\t2\n', self.meta)

    def test_each_list_is_numbered_by_its_first_mob_and_a_mob_without_loot_has_0(self):
        # The crab, first by ID, has the dump's list 0 and the goblin its list 1; the treant has none.
        self.assertEqual([row[-1] for row in self.rows[1:]], ['1', '2', '0'])
        self.assertEqual(self.drops, DROPS_ROWS)

    def test_equal_lists_are_written_once(self):
        with tempfile.TemporaryDirectory() as tmp:
            rows, _, _, drops = run_compact(tmp, dump([mob(), mob(id=16797855, dropList=1)], drop_lists=DROP_LISTS[:1] * 2))
        self.assertEqual([row[-1] for row in rows[1:]], ['1', '1'])
        self.assertEqual(drops, DROPS_ROWS[:4])

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
            'trueDetection': False, 'expLevelMod': 0, 'follows': False, 'placeholderOf': 0, 'immunity': 0,
            'sdt': [0] * 12, 'elementRanks': [0] * 8, 'statusRanks': [0] * 10, 'dropList': 0}
    base.update(fields)
    return base


class MergeSnapshots(unittest.TestCase):
    """Some mobs change aggro at runtime (elementals, Ghrah forms); a mob counts as attacking if any run saw it attack."""

    def compact_rows(self, *mob_lists):
        """The rows and meta, with phoenix_drops.tsv's rows in self.drops and what compact wrote to stderr in
        self.stderr."""
        with tempfile.TemporaryDirectory() as tmp, contextlib.redirect_stderr(io.StringIO()) as err:
            rows, meta, _, self.drops = run_compact(tmp, *(dump(mobs) for mobs in mob_lists))
        self.stderr = err.getvalue()
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
        first = mob(minLevel=76, maxLevel=77, link=True, detects=0x002, expLevelMod=-2, placeholderOf=16797860,
                    immunity=0x800, statusRanks=[0] * 5 + [6] + [0] * 4)
        later = mob(zone=6, name='Label_Only', minLevel=75, maxLevel=78, respawn=999, type=0x02, detects=0x101,
                    trueDetection=True, follows=True, immunity=0x2, dropList=1)
        rows, _ = self.compact_rows([first], [later])
        self.assertEqual(rows[0], ['16797854', '5', 'Ice Elemental', '75', '78', '32', '300', '2', '-2', '16797860',
                                   '2048', ','.join(['0'] * 12), ','.join(['0'] * 8), '0,0,0,0,0,6,0,0,0,0', '1'])
        self.assertEqual(self.drops, DROPS_ROWS[:4])

    def test_a_drop_list_that_differs_in_a_later_snapshot_is_named(self):
        self.compact_rows([mob(), mob(id=16797855)], [mob(dropList=1), mob(id=16797855)])
        self.assertIn('1 mobs have another drop list in a later dump, kept the first: 16797854', self.stderr)

    def test_lists_are_compared_by_their_entries_not_their_dump_index(self):
        with tempfile.TemporaryDirectory() as tmp, contextlib.redirect_stderr(io.StringIO()) as err:
            run_compact(tmp, dump([mob()]), dump([mob(dropList=1)], drop_lists=DROP_LISTS[::-1]))
        self.assertEqual(err.getvalue(), '')

    def test_a_mob_without_loot_has_no_list(self):
        lootless = {field: value for field, value in mob(id=16797855).items() if field != 'dropList'}
        rows, _ = self.compact_rows([mob(), lootless])
        self.assertEqual([r[-1] for r in rows], ['1', '0'])
        self.assertEqual(self.drops, DROPS_ROWS[:4])

    def test_a_group_keeps_its_rate_and_its_members_weights(self):
        rows, _ = self.compact_rows([mob(dropList=1)])
        self.assertEqual(rows[0][-1], '1')
        self.assertEqual(self.drops[1:], [['1'] + row[1:] for row in DROPS_ROWS[4:]])

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

    def test_a_dump_run_with_other_settings_than_live_s_names_each(self):
        # XI_MAIN_USE_ADOULIN_WEAPON_SKILL_CHANGES=0 runs with the number 0, which Lua counts as true.
        other = {'main': dict(SETTINGS['main'], RESTRICT_CONTENT=0, USE_ADOULIN_WEAPON_SKILL_CHANGES=0),
                 'map': SETTINGS['map']}
        self.assertCompactFails('run-1.json: the server ran with main.RESTRICT_CONTENT 0 and '
                                'main.USE_ADOULIN_WEAPON_SKILL_CHANGES 0, where live_settings.lua has 1 and False',
                                dump([mob()]), dump([mob()], settings=other))

    def test_a_dump_missing_a_live_setting_names_it(self):
        other = {'main': SETTINGS['main'], 'map': {}}
        self.assertCompactFails('run-0.json: the server ran with main.RESTRICT_CONTENT', dump([mob()], settings={}))
        self.assertCompactFails('map.DROP_RATE_MULTIPLIER None, where live_settings.lua has 1.5',
                                dump([mob()], settings=other))

    def test_a_dump_without_mobs_or_rules_names_its_file(self):
        self.assertCompactFails('run-0.json: not a dump this compact.py reads', {'rules': RULES})
        self.assertCompactFails('run-0.json: not a dump this compact.py reads', [mob()])

    def test_drop_lists_the_tsv_cannot_say_are_an_error(self):
        def lists(items=(), groups=()):
            return [{'items': list(items), 'groups': list(groups)}]
        for drop_lists, message in (
                (lists([[1, 768, 150, False]]), 'drop list 0: drop type 1 outside a group'),
                (lists([[3, 768, 150, False]]), 'drop list 0: drop type 3 outside a group'),
                (lists(groups=[{'rate': 100, 'fixed': False, 'items': [[0, 768, 1, False]]}]), 'drop type 0 inside a group'),
                (lists([[0, 768, 150, True]]), 'item 768 has a fixed rate'),
                (lists(groups=[{'rate': 100, 'fixed': True, 'items': [[1, 768, 1, False]]}]), 'group 1 has a fixed rate'),
                (lists([[0, 768, 150]]), 'is not a drop list entry'),
                (lists([[0, 768, 1001, False]]), "a drop's rate must be at most 1000")):
            with self.subTest(message=message):
                self.assertCompactFails(message, dump([mob()], drop_lists=drop_lists))

    def test_a_mob_past_the_dumped_lists_is_an_error(self):
        self.assertCompactFails('run-0.json: mob 16797854 has drop list 2, past the 2 dumped', dump([mob(dropList=2)]))
        self.assertCompactFails("has drop list '0', past the 2 dumped", dump([mob(dropList='0')]))

    def test_a_dump_without_drop_lists_names_its_file(self):
        self.assertCompactFails('run-0.json: not a dump this compact.py reads', {'mobs': [mob()], 'rules': RULES})

    def test_a_mob_without_a_field_names_its_file(self):
        broken = mob()
        del broken['detects'], broken['follows']
        self.assertCompactFails('run-1.json: mob 16797854 has no detects, follows', dump([mob()]), dump([broken]))


class CompactRejects(unittest.TestCase):
    def compact_one(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / 'run-1.json'
            path.write_text(text)
            settings = pathlib.Path(tmp) / 'live_settings.lua'
            settings.write_text(LIVE_SETTINGS_TEXT)
            compact.compact([path], tmp, 'abc123', settings)

    def test_a_value_the_plugin_cannot_hold(self):
        for fields, message in (({'detects': 0x10000}, 'detects must be a whole number from 0 to 65535'),
                                ({'respawn': -1}, 'respawn must be a whole number from 0 to 4294967295')):
            with self.subTest(fields=fields):
                with self.assertRaises(compact.schema.DataError) as caught:
                    self.compact_one(json.dumps(dump([mob(**fields)])))
                self.assertIn(message, str(caught.exception))

    def test_a_resistance_list_the_plugin_cannot_hold(self):
        with self.assertRaises(compact.schema.DataError) as caught:
            self.compact_one(json.dumps(dump([mob(sdt=[0] * 11)])))
        self.assertIn('sdt must be 12 whole numbers', str(caught.exception))

    def test_a_truncated_dump_names_its_file(self):
        with self.assertRaises(compact.schema.DataError) as caught:
            self.compact_one(json.dumps(dump([mob()]))[:40])
        self.assertIn('run-1.json: not a complete dump', str(caught.exception))


def record(mob_id, zone, name, lo, hi, flags, respawn, detects, level_mod=0, placeholder_of=0, immunity=0, drops=0):
    return mob_id, dict({'id': mob_id, 'zone': zone, 'name': name, 'minLevel': lo, 'maxLevel': hi, 'flags': flags,
                         'respawn': respawn, 'detects': detects, 'expLevelMod': level_mod, 'placeholderOf': placeholder_of},
                        **dict(ZEROS_RECORD, immunity=immunity, drops=drops))


class Validate(unittest.TestCase):
    RECORDS = dict([
        record(17199105, 103, 'Stag Crab', 15, 17, 3, 0, 2),
        record(17272838, 121, 'Guardian Treant', 32, 32, 12, 0, 2),
        record(17199322, 103, 'Snipper', 19, 20, 0, 300, 2),
        record(17199648, 103, 'Goblin Bounty Hunter', 17, 20, 33, 300, 1),
        record(17190918, 101, 'Wild Rabbit', 1, 1, 0, 60, 257, -2),
        record(17191194, 101, 'Carrion Worm', 4, 5, 0, 180, 2, 0, 17191196),
        record(17191195, 101, 'Carrion Worm', 4, 5, 0, 180, 2, 0, 17191196),
        record(17199999, 103, 'Doman', 76, 76, 8, 0, 0, immunity=0x1000 | 0x10 | 0x8),
    ])

    def check(self, records, **limits):
        return validate.check(records, **dict(dict(min_mobs=5, min_zones=3, min_resisting=0), **limits))

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
        self.assertEqual(self.check(self.RECORDS, min_mobs=9, min_zones=4),
                         ['only 8 mobs (expected at least 9)', 'only 3 zones (expected at least 4)'])

    def test_some_mobs_must_resist_something_or_the_columns_were_read_wrong(self):
        self.assertEqual(self.check(self.RECORDS, min_resisting=1), [
            'only 0 mobs with damage taken other than 100% (expected at least 1)',
            'only 0 mobs with a resistance rank of 4 or more (expected at least 1)'])
        records = dict(self.RECORDS)
        _, crab = record(17199105, 103, 'Stag Crab', 15, 17, 3, 0, 2)
        records[17199105] = dict(crab, sdt=[0, 2500] + [0] * 10, statusRanks=[4] + [0] * 9)
        self.assertEqual(self.check(records, min_resisting=1), [])
        self.assertEqual(self.check(records, min_resisting=2), [
            'only 1 mobs with immunities (expected at least 2)',
            'only 1 mobs with damage taken other than 100% (expected at least 2)',
            'only 1 mobs with a resistance rank of 4 or more (expected at least 2)'])

    def test_doman_must_carry_the_immunities_its_spawn_script_adds(self):
        records = dict(self.RECORDS)
        records.update([record(17199999, 103, 'Doman', 76, 76, 8, 0, 0, immunity=0x1000)])
        self.assertEqual(self.check(records), ['Doman (zone 103): expected immunity bits 4120, got 4096'])
        del records[17199999]
        self.assertEqual(self.check(records), ['Doman (zone 103): missing'])

    def test_load_reads_every_column(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / 'mobs.tsv'
            path.write_text('id\tzone\tname\tminLevel\tmaxLevel\tflags\trespawn\tdetects\texpLevelMod\tplaceholderOf'
                            '\timmunity\tsdt\telementRanks\tstatusRanks\tdrops\n'
                            '17191194\t101\tCarrion Worm\t4\t5\t0\t180\t2\t0\t17191196\t' + '\t'.join(ZERO_LISTS) +
                            '\t0\n')
            self.assertEqual(validate.load(path), dict([record(17191194, 101, 'Carrion Worm', 4, 5, 0, 180, 2, 0, 17191196)]))


def entry(kind, item, rate, group=0, group_rate=0, list_number=1):
    return {'list': list_number, 'kind': kind, 'group': group, 'groupRate': group_rate, 'item': item, 'rate': rate}


class ValidateDrops(unittest.TestCase):
    BILLY = [entry('drop', validate.IRON_ORE, 240), entry('drop', validate.MYTHRIL_ORE, 240),
             entry('drop', 1, 10), entry('steal', validate.GOLD_ORE, 0)]
    BAT = [entry('drop', 922, 150, list_number=2)]
    WORM = [entry('drop', 768, 150, list_number=3), entry('despoil', 640, 0, list_number=3),
            entry('drop', 4495, 600, 1, 100, 3), entry('drop', 0, 400, 1, 100, 3)]
    RECORDS = dict([record(17191196, 100, 'Bigmouth Billy', 9, 9, 8, 0, 2, drops=1),
                    record(17211463, 106, 'Fledermaus', 3, 4, 0, 180, 2, drops=2),
                    record(17191194, 101, 'Carrion Worm', 4, 5, 0, 180, 2, drops=3),
                    record(17191195, 101, 'Carrion Worm', 4, 5, 0, 180, 2)])

    def check(self, records, lists, **limits):
        return validate.check_drops(records, lists, **dict(dict(min_looting=3, min_lists=3, min_of_a_kind=1), **limits))

    def test_known_lists_pass(self):
        self.assertEqual(self.check(self.RECORDS, [self.BILLY, self.BAT, self.WORM]), [])

    def test_too_few_lists_or_kinds_fail(self):
        self.assertEqual(self.check(self.RECORDS, [self.BILLY, self.BAT, self.WORM], min_looting=4, min_lists=4,
                                    min_of_a_kind=2), [
            'only 3 mobs with loot (expected at least 4)', 'only 3 drop lists (expected at least 4)',
            'only 1 drop lists with steal entries (expected at least 2)',
            'only 1 drop lists with despoil entries (expected at least 2)',
            'only 1 drop lists with groups (expected at least 2)'])

    def test_a_list_no_mob_has_fails(self):
        self.assertEqual(self.check(self.RECORDS, [self.BILLY, self.BAT, self.WORM, self.BAT]),
                         ['1 drop lists no mob has, the first 4'])

    def test_bigmouth_billy_must_drop_ore_and_have_gold_to_steal(self):
        self.assertEqual(self.check(self.RECORDS, [self.BILLY[:2], self.BAT, self.WORM]), [
            'only 0 drop lists with steal entries (expected at least 1)',
            f'17191196: its drop list has no [(\'steal\', 0, 0, {validate.GOLD_ORE}, 0)]'])
        records = dict(self.RECORDS)
        records.update([record(17191196, 100, 'Bigmouth Billy', 9, 9, 8, 0, 2)])
        self.assertEqual(len(self.check(records, [self.BILLY, self.BAT, self.WORM], min_looting=0)), 2)  # + list 1 unused

    def test_the_bats_fiend_blood_must_be_gone(self):
        bat = self.BAT + [entry('drop', validate.FIEND_BLOOD, 10, list_number=2)]
        self.assertEqual(self.check(self.RECORDS, [self.BILLY, bat, self.WORM]),
                         [f'Fledermaus (zone 106): still drops one of [{validate.FIEND_BLOOD}], which a module removes'])


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


class LiveSettings(unittest.TestCase):
    def test_each_table_s_keys_are_read_with_their_lua_values(self):
        self.assertEqual(compact.live_settings(LIVE_SETTINGS_TEXT, 'live_settings.lua'),
                         {('main', 'RESTRICT_CONTENT'): 1, ('main', 'ENABLE_WOTG'): 0,
                          ('main', 'USE_ADOULIN_WEAPON_SKILL_CHANGES'): False, ('map', 'DROP_RATE_MULTIPLIER'): 1.5})

    def test_a_value_it_cannot_compare_is_an_error(self):
        for line in ("xi.settings.main.SERVER_NAME = 'Phoenix'", 'xi.settings.main.EGG_HUNT = { START = 1 }',
                     'xi.settings.main = { RESTRICT_CONTENT = 1 }'):
            with self.subTest(line=line):
                text = LIVE_SETTINGS_TEXT.replace('xi.settings.main.RESTRICT_CONTENT = 1', line)
                with self.assertRaises(compact.schema.DataError) as caught:
                    compact.live_settings(text, 'live_settings.lua')
                self.assertIn('live_settings.lua:', str(caught.exception))
