import importlib.util
import json
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


def load_tool(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / 'phoenix' / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


compact = load_tool('compact')
validate = load_tool('validate')

DUMP = [
    {'id': 17199648, 'zone': 103, 'name': 'Goblin_Bounty_Hunter', 'minLevel': 17, 'maxLevel': 20, 'respawn': 300,
     'aggro': True, 'alwaysAggro': 0, 'noAggro': 0, 'neutral': False, 'type': 0, 'spawned': True,
     'link': True, 'detects': 0x001 | 0x002, 'trueDetection': False},
    # A dump from before link and detection were exported: those default to none.
    {'id': 17199105, 'zone': 103, 'name': 'Stag_Crab', 'minLevel': 15, 'maxLevel': 17, 'respawn': 0,
     'aggro': True, 'alwaysAggro': 1, 'noAggro': 0, 'neutral': False, 'type': 0x04, 'spawned': False},
    {'id': 17272838, 'zone': 121, 'name': 'Guardian_Treant', 'minLevel': 32, 'maxLevel': 32, 'respawn': -1,
     'aggro': False, 'alwaysAggro': 0, 'noAggro': 1, 'neutral': True, 'type': 0x02 | 0x10, 'spawned': False,
     'link': False, 'detects': 0x100, 'trueDetection': True},
]


class Compact(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        dump = pathlib.Path(self.tmp.name) / 'dump.json'
        dump.write_text(json.dumps(DUMP))
        self.count, self.zones = compact.compact([dump], self.tmp.name, 'abc123')
        self.lines = (pathlib.Path(self.tmp.name) / 'phoenix_mobs.tsv').read_text().splitlines()

    def tearDown(self):
        self.tmp.cleanup()

    def test_rows_are_sorted_by_id_with_display_names(self):
        self.assertEqual(self.lines[0], 'id\tzone\tname\tminLevel\tmaxLevel\tflags\trespawn\tdetects')
        self.assertEqual([line.split('\t')[0] for line in self.lines[1:]], ['17199105', '17199648', '17272838'])
        self.assertEqual(self.lines[2], '17199648\t103\tGoblin Bounty Hunter\t17\t20\t65\t300\t3')  # aggro + link

    def test_flags_and_negative_respawn(self):
        self.assertEqual(self.lines[1].split('\t')[5:], [str(1 | 2), '0', '0'])  # aggro + always aggro (fished ignored)
        self.assertEqual(self.lines[3].split('\t')[5:], [str(4 | 8 | 16 | 32 | 128), '0', '256'])  # + true detection

    def test_meta(self):
        meta = (pathlib.Path(self.tmp.name) / 'phoenix_mobs.meta').read_text()
        self.assertIn('phoenix_commit\tabc123\n', meta)
        self.assertIn('mobs\t3\n', meta)
        self.assertIn('zones\t2\n', meta)

    def test_names_with_tabs_are_rejected(self):
        with self.assertRaises(ValueError):
            compact.display_name('Bad\tName')


def mob(**fields):
    base = {'id': 16797854, 'zone': 5, 'name': 'Ice_Elemental', 'minLevel': 76, 'maxLevel': 77, 'respawn': 300,
            'aggro': False, 'alwaysAggro': 0, 'noAggro': 0, 'neutral': False, 'type': 0, 'spawned': True}
    base.update(fields)
    return base


class MergeSnapshots(unittest.TestCase):
    """Some mobs change aggro at runtime (elementals, Ghrah forms); a mob counts as attacking if any run saw it attack."""

    def compact_rows(self, *dumps):
        with tempfile.TemporaryDirectory() as tmp:
            paths = []
            for n, dump in enumerate(dumps):
                path = pathlib.Path(tmp) / f'dump{n}.json'
                path.write_text(json.dumps(dump))
                paths.append(path)
            compact.compact(paths, tmp, 'abc123')
            lines = (pathlib.Path(tmp) / 'phoenix_mobs.tsv').read_text().splitlines()
            meta = (pathlib.Path(tmp) / 'phoenix_mobs.meta').read_text()
        return [line.split('\t') for line in lines[1:]], meta

    def test_attacking_in_any_snapshot_counts(self):
        rows, _ = self.compact_rows([mob(aggro=False)], [mob(aggro=True)])
        self.assertEqual(rows[0][5], '1')

    def test_a_neutral_snapshot_does_not_hide_an_attacking_one(self):
        rows, _ = self.compact_rows([mob(aggro=True, neutral=True)], [mob(aggro=True, neutral=False)])
        self.assertEqual(rows[0][5], '1')

    def test_passive_in_every_snapshot_keeps_the_first(self):
        rows, _ = self.compact_rows([mob(aggro=False)], [mob(aggro=True, neutral=True)])
        self.assertEqual(rows[0][5], '0')

    def test_identity_comes_from_the_first_snapshot_and_levels_span_all(self):
        rows, _ = self.compact_rows([mob(name="Ice_Elemental", minLevel=76, maxLevel=77, respawn=300)],
                                    [mob(name='Label_Only', minLevel=75, maxLevel=78, respawn=999)])
        self.assertEqual(rows[0][2:5] + [rows[0][6]], ['Ice Elemental', '75', '78', '300'])

    def test_link_and_detection_come_from_the_first_snapshot(self):
        rows, _ = self.compact_rows([mob(link=True, detects=0x002, trueDetection=False)],
                                    [mob(link=False, detects=0x101, trueDetection=True)])
        self.assertEqual((rows[0][5], rows[0][7]), ('64', '2'))

    def test_detection_comes_from_a_snapshot_that_has_it(self):
        old = {k: v for k, v in mob().items() if k not in ('link', 'detects', 'trueDetection')}
        rows, _ = self.compact_rows([old], [mob(link=True, detects=0x002, trueDetection=True)])
        self.assertEqual((rows[0][5], rows[0][7]), (str(64 | 128), '2'))

    def test_mobs_from_any_snapshot_are_kept_and_counted(self):
        rows, meta = self.compact_rows([mob()], [mob(), mob(id=16797855)])
        self.assertEqual([r[0] for r in rows], ['16797854', '16797855'])
        self.assertIn('dumps\t2\n', meta)


class Validate(unittest.TestCase):
    RECORDS = {
        17199105: (103, 'Stag Crab', 15, 17, 3, 0, 2),
        17272838: (121, 'Guardian Treant', 32, 32, 60, 0, 2),
        17199322: (103, 'Snipper', 19, 20, 0, 300, 2),
        17199648: (103, 'Goblin Bounty Hunter', 17, 20, 65, 300, 1),
    }

    def test_known_spawns_pass(self):
        self.assertEqual(validate.check(self.RECORDS, min_mobs=4, min_zones=2), [])

    def test_wrong_level_and_aggro_fail(self):
        records = dict(self.RECORDS)
        records[17199322] = (103, 'Snipper', 19, 21, 1, 300, 2)
        problems = validate.check(records, min_mobs=4, min_zones=2)
        self.assertEqual(len(problems), 2)

    def test_missing_link_or_detection_fails(self):
        records = dict(self.RECORDS)
        records[17199648] = (103, 'Goblin Bounty Hunter', 17, 20, 1, 300, 0)
        self.assertEqual(len(validate.check(records, min_mobs=4, min_zones=2)), 2)

    def test_too_few_mobs_or_zones_fail(self):
        self.assertEqual(len(validate.check(self.RECORDS, min_mobs=5, min_zones=3)), 2)

    def test_load_reads_the_detects_column(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / 'mobs.tsv'
            path.write_text('id\tzone\tname\tminLevel\tmaxLevel\tflags\trespawn\tdetects\n'
                            '17199648\t103\tGoblin Bounty Hunter\t17\t20\t65\t300\t3\n')
            self.assertEqual(validate.load(path), {17199648: (103, 'Goblin Bounty Hunter', 17, 20, 65, 300, 3)})


if __name__ == '__main__':
    unittest.main()
