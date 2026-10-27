import unittest

from tooling import load

gen = load('tools/gen_world.py')

TRAVEL = ('group\tname\tevery\tdepartures\nAirships\tJeuno to San d\'Oria\t864\t28:182\n'
          'Barge\tCentral to South Landing\t3456\t678:752,2790:2864\n')
GUILDS = ('guild\tshops\tzone\topen\tclose\tholiday\nAlchemy\tWahraga, Gathweeda\tAht Urhgan Whitegate\t8\t23\t5\n'
          "Woodworking\tChaupire\tNorthern San d'Oria\t6\t21\t0\n")


class Generate(unittest.TestCase):
    def test_routes_and_shops(self):
        text = gen.generate(gen.parse_travel(TRAVEL), gen.parse_guilds(GUILDS))
        self.assertIn('    {"Airships", "Jeuno to San d\'Oria", 864, 1, {{28, 182}}},\n', text)
        self.assertIn('    {"Barge", "Central to South Landing", 3456, 2, {{678, 752}, {2790, 2864}}},\n', text)
        self.assertIn('    {"Alchemy", "Wahraga, Gathweeda", "Aht Urhgan Whitegate", 8, 23, 5},\n', text)

    def test_a_file_without_its_header_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse_travel(TRAVEL.replace('departures', 'times'))
        with self.assertRaises(gen.DataError):
            gen.parse_guilds('')

    def test_a_departure_outside_its_cycle_is_an_error(self):
        with self.assertRaises(gen.DataError) as raised:
            gen.parse_travel(TRAVEL.replace('28:182', '28:864'))
        self.assertIn('line 2', str(raised.exception))

    def test_more_departures_than_a_route_holds_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse_travel(TRAVEL.replace('28:182', '28:182,300:400,500:600'))

    def test_hours_and_holidays_out_of_range_are_errors(self):
        for row in ['Cooking\tKopopo\tWindurst Waters\t20\t5\t7\n', 'Cooking\tKopopo\tWindurst Waters\t5\t25\t7\n',
                    'Cooking\tKopopo\tWindurst Waters\t5\t20\t8\n', 'Cooking\tKopopo\tWindurst Waters\t5\t20\n']:
            with self.assertRaises(gen.DataError):
                gen.parse_guilds(GUILDS + row)
