import contextlib
import io
import pathlib
import re
import tempfile
import unittest

from tooling import ROOT, load

gen = load('tools/gen_zones.py')

DAYS = '2104' + '0000' * 2158 + '4204'  # day 0 sunshine three times (0x0421), blank days, the last 0x0442
ZONES = 'id\tname\tregion\tcalendar\n1\tKing Ranperre\'s Tomb\t0\t\n0\tValkurm Dunes\t1\t0\n'
WEATHER = 'calendar\tdays\n0\t' + DAYS + '\n'


def numbers(text: str, array: str) -> list:
    """The values a generated array holds, in order."""
    body = text.split(array + ' = {')[1].split('};')[0]
    return [int(value, 0) for value in re.findall(r'0x[0-9a-f]+|\d+', body)]


class Generate(unittest.TestCase):
    def test_zones_and_calendars(self):
        text = gen.generate(gen.parse_zones(ZONES), gen.parse_calendars(WEATHER))
        self.assertIn('const ZoneRecord kZones[] = {\n    {"Valkurm Dunes", 1, 0},\n'
                      '    {"King Ranperre\'s Tomb", 0, 255},\n};', text)
        self.assertIn('static_assert(kWeatherDays == 2160', text)
        self.assertIn('static_assert(kUnknownRegion == 255', text)
        self.assertIn('static_assert(kNoCalendar == 255', text)
        self.assertIn('constexpr size_t kRegionCount = 46;', text)

    def test_a_blank_day_is_the_day_before_it(self):
        text = gen.generate(gen.parse_zones(ZONES), gen.parse_calendars(WEATHER))
        self.assertEqual(numbers(text, 'const uint16_t kWeatherOdds[]'), [0x0421, 0x0442])
        self.assertEqual(numbers(text, 'const uint8_t kWeatherCalendars[][kWeatherDays]'), [0] * 2159 + [1])

    def test_a_calendar_starting_on_a_blank_day_is_an_error(self):
        with self.assertRaises(gen.DataError) as raised:
            gen.generate(gen.parse_zones(ZONES), gen.parse_calendars(WEATHER.replace('\t2104', '\t0000')))
        self.assertIn('calendar 0', str(raised.exception))

    def test_more_kinds_of_day_than_a_byte_tells_apart_is_an_error(self):
        days = ''.join(f'{day & 0xFF:02X}{day >> 8:02X}' for day in range(1, 258)) + '0000' * (2160 - 257)
        with self.assertRaises(gen.DataError) as raised:
            gen.generate(gen.parse_zones(ZONES), gen.parse_calendars('calendar\tdays\n0\t' + days + '\n'))
        self.assertIn('257', str(raised.exception))

    def test_a_missing_zone_id_is_an_error(self):
        with self.assertRaises(gen.DataError) as raised:
            gen.generate(gen.parse_zones(ZONES.replace('\n1\t', '\n2\t')), gen.parse_calendars(WEATHER))
        self.assertIn('zone 1', str(raised.exception))

    def test_a_calendar_number_past_the_last_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.generate(gen.parse_zones(ZONES.replace('\t1\t0\n', '\t1\t3\n')), gen.parse_calendars(WEATHER))

    def test_a_short_calendar_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse_calendars(WEATHER.replace(DAYS, DAYS[:-4]))

    def test_a_repeated_zone_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse_zones(ZONES + '0\tAgain\t1\t0\n')

    def test_a_file_without_its_header_is_an_error(self):
        with self.assertRaises(gen.DataError):
            gen.parse_zones(ZONES.replace('id\tname', 'zone\tname'))
        with self.assertRaises(gen.DataError):
            gen.parse_calendars(WEATHER.replace('calendar\tdays', 'calendar\tweather'))

    def test_a_region_phoenix_does_not_have_is_an_error(self):
        self.assertEqual(gen.parse_zones(ZONES.replace('\t1\t0\n', '\t45\t0\n'))[0]['region'], 45)  # East Ulbuka
        with self.assertRaises(gen.DataError):
            gen.parse_zones(ZONES.replace('\t1\t0\n', '\t46\t0\n'))

    def test_calendars_must_be_numbered_in_order(self):
        with self.assertRaises(gen.DataError):
            gen.parse_calendars('calendar\tdays\n1\t' + DAYS + '\n')
        with self.assertRaises(gen.DataError):
            gen.parse_calendars(WEATHER + '0\t' + DAYS + '\n')

    def test_days_are_upper_case_hex(self):
        with self.assertRaises(gen.DataError):
            gen.parse_calendars(WEATHER.replace('4204', '4a04'))


class Check(unittest.TestCase):
    """refresh.sh checks a Phoenix refresh's zone files with the generator's own rules before they reach data/."""

    def check(self, zones: str, weather: str) -> int:
        generated = ROOT / 'src' / 'generated' / 'zones.inc'
        before = generated.read_bytes() if generated.is_file() else None
        with tempfile.TemporaryDirectory() as tmp:
            (pathlib.Path(tmp) / 'phoenix_zones.tsv').write_text(zones, encoding='utf-8')
            (pathlib.Path(tmp) / 'phoenix_weather.tsv').write_text(weather, encoding='utf-8')
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                status = gen.main(['gen_zones.py', '--check', tmp])
        self.assertEqual(generated.read_bytes() if generated.is_file() else None, before)  # writes nothing
        return status

    def test_files_the_plugin_can_use_pass(self):
        self.assertEqual(self.check(ZONES, WEATHER), 0)

    def test_what_only_the_generator_checks_fails(self):
        self.assertEqual(self.check(ZONES.replace('\n1\t', '\n2\t'), WEATHER), 1)  # zone 1 missing
        self.assertEqual(self.check(ZONES, WEATHER.replace('\t2104', '\t0000')), 1)  # a calendar's first day blank

    def test_a_missing_file_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(gen.main(['gen_zones.py', '--check', tmp]), 1)
