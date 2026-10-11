import pathlib
import tempfile
import unittest

from tooling import load

zones = load('tools/phoenix/zones.py')

DAYS = '2104' * 2160
SETTINGS = ("INSERT INTO `zone_settings` VALUES (100,'127.0.0.1',54230,'West_Ronfaure');\n"
            "INSERT INTO `zone_settings` VALUES (190,'127.0.0.1',54230,'King_Ranperres_Tomb');\n")
ZONE_YAML = 'values:\n  west_ronfaure:                  100\n  king_ranperres_tomb:            190\n'
ZONE_H = 'enum class REGION_TYPE : uint8\n{\n    RONFAURE = 0,\n    ZULKHEIM = 1,\n    UNKNOWN = 255\n};\n'
ZONEUTILS = ('auto GetCurrentRegion(const xi::ZoneId zoneId) -> REGION_TYPE\n{\n    switch (zoneId)\n    {\n'
             '        case xi::ZoneId::KingRanperresTomb:\n        case xi::ZoneId::WestRonfaure:\n'
             '            return REGION_TYPE::RONFAURE;\n        default:\n            return REGION_TYPE::UNKNOWN;\n'
             '    }\n}\n')
WEATHER = ('INSERT INTO `zone_weather` VALUES (100,0x' + DAYS + ');\n'
           'INSERT INTO `zone_weather` VALUES (190,0x' + DAYS + ');\n')
REAL = pathlib.Path.home() / '.cache' / 'headsup-phoenix' / 'server'


def checkout(root: pathlib.Path, weather: str = WEATHER, zoneutils: str = ZONEUTILS) -> pathlib.Path:
    for relative, text in [('sql/zone_settings.sql', SETTINGS), ('sql/zone_weather.sql', weather),
                           ('data/enums/zone.yaml', ZONE_YAML), ('src/map/zone.h', ZONE_H),
                           ('src/map/utils/zoneutils.cpp', zoneutils)]:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
    return root


class DisplayNames(unittest.TestCase):
    def test_script_folders_become_the_names_players_know(self):
        for script, shown in [('Valkurm_Dunes', 'Valkurm Dunes'), ('King_Ranperres_Tomb', "King Ranperre's Tomb"),
                              ('Southern_San_dOria_[S]', "Southern San d'Oria [S]"),
                              ('Chateau_dOraguille', "Chateau d'Oraguille"),
                              ('Dynamis-San_dOria', "Dynamis - San d'Oria"),
                              ('Abyssea-La_Theine', 'Abyssea - La Theine'),
                              ('Riverne-Site_A01', 'Riverne - Site #A01'), ('RuLude_Gardens', "Ru'Lude Gardens"),
                              ('Mine_Shaft_2716', 'Mine Shaft #2716'),
                              ('Bastok-Jeuno_Airship', 'Bastok-Jeuno Airship'), ('Balgas_Dais', "Balga's Dais"),
                              ('Ship_bound_for_Selbina_Pirates', 'Ship bound for Selbina'),
                              ('Ship_bound_for_Mhaura_Pirates', 'Ship bound for Mhaura')]:
            self.assertEqual(zones.display_name(script), shown)


class Build(unittest.TestCase):
    def test_zones_get_names_regions_and_shared_calendars(self):
        with tempfile.TemporaryDirectory() as tmp:
            rows, calendars = zones.build(checkout(pathlib.Path(tmp)))
        self.assertEqual(rows, [(100, 'West Ronfaure', 0, 0), (190, "King Ranperre's Tomb", 0, 0)])
        self.assertEqual(calendars, [DAYS])

    def test_a_region_case_naming_no_zone_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), zoneutils=ZONEUTILS.replace('WestRonfaure', 'WestRonfaur'))
            with self.assertRaises(zones.ZoneDataError) as raised:
                zones.build(root)
        self.assertIn('WestRonfaur', str(raised.exception))

    def test_a_call_before_the_definition_is_not_the_switch(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp),
                            zoneutils='void Zone()\n{\n    return GetCurrentRegion(ZoneID);\n}\n' + ZONEUTILS)
            rows, _ = zones.build(root)
        self.assertEqual([region for _, _, region, _ in rows], [0, 0])

    def test_a_short_calendar_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), weather=WEATHER.replace(DAYS, DAYS[:-4], 1))
            with self.assertRaises(zones.ZoneDataError) as raised:
                zones.build(root)
        self.assertIn('zone 100', str(raised.exception))

    def test_a_weather_file_with_no_calendars_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), weather='INSERT INTO `zone_weather` VALUES (100,X\'2104\');\n')
            with self.assertRaises(zones.ZoneDataError) as raised:
                zones.build(root)
        self.assertIn('zone_weather.sql', str(raised.exception))

    def test_a_region_switch_without_its_end_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), zoneutils=ZONEUTILS[:-len('    }\n}\n')])
            with self.assertRaises(zones.ZoneDataError) as raised:
                zones.build(root)
        self.assertIn('GetCurrentRegion', str(raised.exception))

    def test_writes_both_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp) / 'out'
            zones.write(out, [(100, 'West Ronfaure', 0, 0), (999, 'Nowhere', 255, '')], [DAYS])
            self.assertEqual((out / 'phoenix_zones.tsv').read_text(encoding='utf-8'),
                             'id\tname\tregion\tcalendar\n100\tWest Ronfaure\t0\t0\n999\tNowhere\t255\t\n')
            self.assertEqual((out / 'phoenix_weather.tsv').read_text(encoding='utf-8'),
                             'calendar\tdays\n0\t' + DAYS + '\n')

    @unittest.skipUnless((REAL / 'sql' / 'zone_settings.sql').is_file(), 'no Phoenix checkout')
    def test_the_real_checkout(self):
        rows, calendars = zones.build(REAL)
        by_id = {zone: (name, region) for zone, name, region, _ in rows}
        self.assertEqual(len(rows), 300)
        self.assertEqual(len(calendars), 60)
        self.assertEqual(by_id[103], ('Valkurm Dunes', 1))
        self.assertEqual(by_id[190], ("King Ranperre's Tomb", 0))
