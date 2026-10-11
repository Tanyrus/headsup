#!/usr/bin/env python3
"""Read zone names, regions and weather calendars from a Phoenix checkout into data/.

Usage: zones.py SERVER_DIR OUT_DIR. Phoenix keeps no display names, only script folder names, and no region column:
regions come from the switch in zoneutils.cpp's GetCurrentRegion, matched to zone numbers through data/enums/zone.yaml.
Zones share 60 weather calendars, so each distinct one is written once.
"""
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
from gen_zones import UNKNOWN_REGION, WEATHER_DAYS  # noqa: E402

# Script folders that lost an apostrophe, a hash or a spaced dash, and the ships with pirates aboard, which the client
# names as it does the same ships without (220, 221); the rest only trade underscores for spaces, and "dOria" and
# "dOraguille" take their apostrophe back by rule.
DISPLAY_NAMES = {
    'Carpenters_Landing': "Carpenters' Landing", 'PsoXja': "Pso'Xja", 'Mine_Shaft_2716': 'Mine Shaft #2716',
    'Riverne-Site_A01': 'Riverne - Site #A01', 'Riverne-Site_B01': 'Riverne - Site #B01',
    'Sealions_Den': "Sealion's Den", 'AlTaieu': "Al'Taieu", 'Grand_Palace_of_HuXzoi': "Grand Palace of Hu'Xzoi",
    'The_Garden_of_RuHmet': "The Garden of Ru'Hmet", 'The_Sanctuary_of_ZiTah': "The Sanctuary of Zi'Tah",
    'RoMaeve': "Ro'Maeve", 'Behemoths_Dominion': "Behemoth's Dominion", 'Ghoyus_Reverie': "Ghoyu's Reverie",
    'RuAun_Gardens': "Ru'Aun Gardens", 'Dragons_Aery': "Dragon's Aery",
    'Lower_Delkfutts_Tower': "Lower Delkfutt's Tower", 'Middle_Delkfutts_Tower': "Middle Delkfutt's Tower",
    'Upper_Delkfutts_Tower': "Upper Delkfutt's Tower", 'Crawlers_Nest': "Crawlers' Nest",
    'Crawlers_Nest_[S]': "Crawlers' Nest [S]", 'VeLugannon_Palace': "Ve'Lugannon Palace",
    'The_Shrine_of_RuAvitau': "The Shrine of Ru'Avitau", 'LaLoff_Amphitheater': "La'Loff Amphitheater",
    'King_Ranperres_Tomb': "King Ranperre's Tomb", 'Ordelles_Caves': "Ordelle's Caves", 'FeiYin': "Fei'Yin",
    'Ifrits_Cauldron': "Ifrit's Cauldron", 'QuBia_Arena': "Qu'Bia Arena", 'RuLude_Gardens': "Ru'Lude Gardens",
    'Outer_RaKaznar': "Outer Ra'Kaznar", 'Outer_RaKaznar_[U1]': "Outer Ra'Kaznar [U1]",
    'Outer_RaKaznar_[U2]': "Outer Ra'Kaznar [U2]", 'Outer_RaKaznar_[U3]': "Outer Ra'Kaznar [U3]",
    'RaKaznar_Inner_Court': "Ra'Kaznar Inner Court", 'RaKaznar_Turris': "Ra'Kaznar Turris",
    'Escha_ZiTah': "Escha - Zi'Tah", 'Escha_RuAun': "Escha - Ru'Aun", 'Balgas_Dais': "Balga's Dais",
    'Ship_bound_for_Selbina_Pirates': 'Ship bound for Selbina',
    'Ship_bound_for_Mhaura_Pirates': 'Ship bound for Mhaura',
}
SPACED_DASH_PREFIXES = ('Abyssea-', 'Promyvion-', 'Dynamis-')


class ZoneDataError(ValueError):
    pass


def display_name(script: str) -> str:
    if script in DISPLAY_NAMES:
        return DISPLAY_NAMES[script]
    name = script
    for prefix in SPACED_DASH_PREFIXES:
        if name.startswith(prefix):
            name = prefix[:-1] + ' - ' + name[len(prefix):]
    return re.sub(r"\bd(O[a-z])", r"d'\1", name.replace('_', ' '))


def parse_zone_settings(text: str) -> dict:
    rows = re.findall(r"\((\d+),'[^']*',\d+,'([^']*)'", text)
    if not rows:
        raise ZoneDataError('zone_settings.sql lists no zones')
    return {int(zone): name for zone, name in rows}


def parse_zone_ids(text: str) -> dict:
    return {name: int(value) for name, value in re.findall(r'^  ([a-z0-9_]+):\s+(\d+)', text, re.M)}


def parse_region_enum(text: str) -> dict:
    found = re.search(r'enum class REGION_TYPE[^{]*\{(.*?)\};', text, re.S)
    if found is None:
        raise ZoneDataError('zone.h has no REGION_TYPE')
    return {name: int(value) for name, value in re.findall(r'(\w+)\s*=\s*(\d+)', found.group(1))}


def pascal(snake: str) -> str:
    return ''.join(part.capitalize() for part in snake.split('_'))


def parse_regions(text: str, zone_ids: dict, region_ids: dict) -> dict:
    definition = re.search(r'^auto GetCurrentRegion\(', text, re.M)
    if definition is None:
        raise ZoneDataError('zoneutils.cpp has no GetCurrentRegion')
    start = definition.start()
    end = text.find('\n}\n', start)
    if end < 0:
        raise ZoneDataError('zoneutils.cpp: GetCurrentRegion has no end')
    body = text[start:end]
    by_pascal = {pascal(name): zone for name, zone in zone_ids.items()}
    regions, pending = {}, []
    for case, region in re.findall(r'case xi::ZoneId::(\w+):|return REGION_TYPE::(\w+);', body):
        if case:
            if case not in by_pascal:
                raise ZoneDataError(f'zoneutils.cpp names zone {case}, which zone.yaml does not list')
            pending.append(by_pascal[case])
            continue
        if region not in region_ids:
            raise ZoneDataError(f'zoneutils.cpp names region {region}, which zone.h does not list')
        for zone in pending:
            regions[zone] = region_ids[region]
        pending = []
    return regions


def parse_weather(text: str) -> dict:
    calendars = {}
    for zone, days in re.findall(r'VALUES \((\d+),0x([0-9A-Fa-f]*)\)', text):
        if len(days) != WEATHER_DAYS * 4:
            raise ZoneDataError(f'zone_weather.sql gives zone {zone} {len(days) // 4} days, not {WEATHER_DAYS}')
        calendars[int(zone)] = days.upper()
    if not calendars:
        raise ZoneDataError('zone_weather.sql lists no calendars')
    return calendars


def build(server: pathlib.Path):
    def read(relative: str) -> str:
        return (server / relative).read_text(encoding='utf-8')

    names = parse_zone_settings(read('sql/zone_settings.sql'))
    regions = parse_regions(read('src/map/utils/zoneutils.cpp'), parse_zone_ids(read('data/enums/zone.yaml')),
                            parse_region_enum(read('src/map/zone.h')))
    weather = parse_weather(read('sql/zone_weather.sql'))
    calendars, index, rows = [], {}, []
    for zone in sorted(names):
        days = weather.get(zone)
        calendar = ''
        if days is not None:
            if days not in index:
                index[days] = len(calendars)
                calendars.append(days)
            calendar = index[days]
        rows.append((zone, display_name(names[zone]), regions.get(zone, UNKNOWN_REGION), calendar))
    return rows, calendars


def write(out: pathlib.Path, rows, calendars) -> None:
    out.mkdir(parents=True, exist_ok=True)
    zones = ['id\tname\tregion\tcalendar']
    zones += [f'{zone}\t{name}\t{region}\t{calendar}' for zone, name, region, calendar in rows]
    (out / 'phoenix_zones.tsv').write_text('\n'.join(zones) + '\n', encoding='utf-8')
    weather = ['calendar\tdays'] + [f'{i}\t{days}' for i, days in enumerate(calendars)]
    (out / 'phoenix_weather.tsv').write_text('\n'.join(weather) + '\n', encoding='utf-8')


def main(argv) -> int:
    if len(argv) != 3:
        print('usage: zones.py SERVER_DIR OUT_DIR', file=sys.stderr)
        return 2
    try:
        rows, calendars = build(pathlib.Path(argv[1]))
    except (OSError, ZoneDataError) as error:
        print(f'zones: {error}', file=sys.stderr)
        return 1
    write(pathlib.Path(argv[2]), rows, calendars)
    print(f'zones: {len(rows)} zones, {len(calendars)} weather calendars -> {argv[2]}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
