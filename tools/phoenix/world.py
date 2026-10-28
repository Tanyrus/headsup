#!/usr/bin/env python3
"""Read the ships' schedules and the guild shops' hours from a Phoenix checkout into data/.

Usage: world.py SERVER_DIR OUT_DIR. A ship's runs are in the transport block of data/zones/<zone>/zone.yaml, timed in
Earth seconds since the Vana'diel epoch; a run with a door carries passengers. Guild shops are scripts/data/guild_shops.lua,
with each NPC's place and guild in the header of its script under scripts/zones.
"""
import pathlib
import re
import sys

# A run's name as players know the route, by Phoenix's run key. A run Phoenix adds or renames stops the refresh here.
TRAVEL_NAMES = {
    'selbina_mhaura_boat': ('Boats', 'Selbina to Mhaura'),
    'mhaura_selbina_boat': ('Boats', 'Mhaura to Selbina'),
    'mhaura_whitegate_boat': ('Boats', 'Mhaura to Whitegate'),
    'whitegate_mhaura_boat': ('Boats', 'Whitegate to Mhaura'),
    'whitegate_nashmau_boat': ('Boats', 'Whitegate to Nashmau'),
    'nashmau_whitegate_boat': ('Boats', 'Nashmau to Whitegate'),
    'jeuno_sandoria_airship': ('Airships', "Jeuno to San d'Oria"),
    'jeuno_bastok_airship': ('Airships', 'Jeuno to Bastok'),
    'jeuno_windurst_airship': ('Airships', 'Jeuno to Windurst'),
    'jeuno_kazham_airship': ('Airships', 'Jeuno to Kazham'),
    'sandoria_jeuno_airship': ('Airships', "San d'Oria to Jeuno"),
    'bastok_jeuno_airship': ('Airships', 'Bastok to Jeuno'),
    'windurst_jeuno_airship': ('Airships', 'Windurst to Jeuno'),
    'kazham_jeuno_airship': ('Airships', 'Kazham to Jeuno'),
    'manaclip_bibiki_tours': ('Manaclipper', 'Bibiki Bay tour'),
    'manaclip_bibiki_purgonorgo': ('Manaclipper', 'Bibiki Bay to Purgonorgo Isle'),
    'manaclip_purgonorgo_bibiki': ('Manaclipper', 'Purgonorgo Isle to Bibiki Bay'),
    'barge_south_central': ('Barge', 'South to Central Landing'),
    'barge_central_south_day': ('Barge', 'Central to South Landing'),
    'barge_central_south_night': ('Barge', 'Central to South Landing'),
    'barge_south_north': ('Barge', 'South to North Landing'),
    'barge_north_central': ('Barge', 'North to Central Landing'),
}
GROUPS = ['Airships', 'Boats', 'Manaclipper', 'Barge']
BOARDING, LEAVING = 'docked', 'departing'  # the door opens as the ship docks

# A shop's kind from its script's header, the first word found; the headers name guilds in several ways.
GUILD_WORDS = [('Goldsmithing', 'Goldsmithing'), ('Smithing', 'Smithing'), ('Woodworking', 'Woodworking'),
               ('Cloth', 'Clothcraft'), ('Leather', 'Leathercraft'), ('Bone', 'Bonecraft'), ('Alchem', 'Alchemy'),
               ('Cooking', 'Cooking'), ('Fishing', 'Fishing'), ('Tenshodo', 'Tenshodo'), ('Linkshell', 'Linkshell')]
# Merchants with guild hours that are no guild, left out: the goblins who sell crystal chips.
NOT_GUILDS = ['acts like a guild merchant']
WEEKDAYS = ['FIRESDAY', 'EARTHSDAY', 'WATERSDAY', 'WINDSDAY', 'ICEDAY', 'LIGHTNINGDAY', 'LIGHTSDAY', 'DARKSDAY']
HEADER_LINES = 8


class WorldDataError(ValueError):
    pass


def parse_runs(text: str, zone: str) -> dict:
    """Each run of the zone's transport block: its door, cycle, offset and phases as (state, seconds or None)."""
    runs, run, in_block, in_phases = {}, None, False, False
    for line in text.splitlines():
        line = line.split('#', 1)[0].rstrip()
        if not line.strip():
            continue
        indent = len(line) - len(line.lstrip())
        if indent == 0:
            in_block = line == 'transport:'
            continue
        if not in_block:
            continue
        key, _, value = line.strip().partition(':')
        value = value.strip()
        if indent == 4:
            run = runs.setdefault(key, {'door': None, 'every': None, 'offset': 0, 'phases': []})
            in_phases = False
        elif run is None:
            continue
        elif indent == 6:
            in_phases = key == 'phases'
            if key == 'door':
                run['door'] = value
            elif key in ('every', 'offset'):
                if not value.isdigit():
                    raise WorldDataError(f'{zone}: {key} of a run is {value}, not a number')
                run[key] = int(value)
        elif in_phases and indent == 8 and key == '- state':
            run['phases'].append([value, None])
        elif in_phases and indent == 10 and key == 'seconds' and run['phases']:
            run['phases'][-1][1] = int(value)
    return runs


def times(key: str, run: dict) -> tuple:
    """Where in its cycle the run's door opens and where it leaves."""
    every, phases = run['every'], run['phases']
    if not every:
        raise WorldDataError(f'run {key} has no cycle')
    unset = [state for state, seconds in phases if seconds is None]
    if len(unset) > 1:
        raise WorldDataError(f'run {key} leaves the length of {", ".join(unset)} unset')
    rest = every - sum(seconds for _, seconds in phases if seconds is not None)
    starts, at = {}, 0
    for state, seconds in phases:
        starts.setdefault(state, at)
        at += rest if seconds is None else seconds
    for state in (BOARDING, LEAVING):
        if state not in starts:
            raise WorldDataError(f'run {key} has no {state} phase')
    return (run['offset'] + starts[BOARDING]) % every, (run['offset'] + starts[LEAVING]) % every


def build_travel(server: pathlib.Path, names: dict = TRAVEL_NAMES) -> list:
    found = {}
    for path in sorted((server / 'data' / 'zones').glob('*/zone.yaml')):
        for key, run in parse_runs(path.read_text(encoding='utf-8'), path.parent.name).items():
            if run['door'] is None:
                continue
            if key in found:
                raise WorldDataError(f'run {key} is in two zones')
            if key not in names:
                raise WorldDataError(f'{path.parent.name} has run {key}, which has no name here')
            found[key] = run
    if missing := sorted(set(names) - set(found)):
        raise WorldDataError(f'no zone has the runs {", ".join(missing)}')
    routes = {}
    for key, run in found.items():
        group, name = names[key]
        route = routes.setdefault((group, name), [run['every'], []])
        if route[0] != run['every']:
            raise WorldDataError(f'the runs of {name} have cycles of {route[0]} and {run["every"]} seconds')
        route[1].append(times(key, run))
    return [(group, name, every, sorted(departures))
            for (group, name), (every, departures) in sorted(routes.items(), key=lambda r: (GROUPS.index(r[0][0]), r[0][1]))]


def guild_kind(header: str):
    return next((kind for word, kind in GUILD_WORDS if word.lower() in header.lower()), None)


def build_guilds(server: pathlib.Path) -> list:
    text = (server / 'scripts' / 'data' / 'guild_shops.lua').read_text(encoding='utf-8')
    parts = re.split(r"^    \['([^']+)'\] =", text, flags=re.M)
    entries = list(zip(parts[1::2], parts[2::2]))
    scripts = {}
    for path in sorted((server / 'scripts' / 'zones').glob('*/npcs/*.lua')):
        scripts.setdefault(path.stem, path)  # an NPC on a ship and its pirates' copy: the first
    shops, aliases = {}, {}
    for npc, body in entries:
        if shared := re.search(r"sharedStock\s*=\s*'([^']+)'", body):
            aliases.setdefault(shared.group(1), []).append(npc)
            continue
        hours = re.search(r'hours\s*=\s*\{\s*(\d+)\s*,\s*(\d+)\s*\}', body)
        holiday = re.search(r'holiday\s*=\s*xi\.day\.([A-Z]+)', body)
        if not hours or not holiday or holiday.group(1) not in WEEKDAYS:
            raise WorldDataError(f'guild_shops.lua gives {npc} no hours or holiday')
        if npc not in scripts:
            raise WorldDataError(f'guild_shops.lua has {npc}, who has no script')
        header = '\n'.join(scripts[npc].read_text(encoding='utf-8').splitlines()[:HEADER_LINES])
        if any(words.lower() in header.lower() for words in NOT_GUILDS):
            continue
        area, kind = re.search(r'--\s*Area:\s*(.+)', header), guild_kind(header)
        if not area or not kind:
            raise WorldDataError(f'the script of {npc} names no place or no guild HeadsUp knows')
        shops[npc] = (kind, area.group(1).strip(), int(hours.group(1)), int(hours.group(2)),
                      WEEKDAYS.index(holiday.group(1)))
    if strays := sorted(set(aliases) - set(shops)):
        raise WorldDataError(f'guild_shops.lua shares the stock of {", ".join(strays)}, which is no shop')
    rows = []
    for npc, (kind, zone, opens, closes, holiday) in shops.items():
        names = ', '.join(name.replace('_', ' ') for name in [npc] + aliases.get(npc, []))
        rows.append((kind, names, zone, opens, closes, holiday))
    return sorted(rows, key=lambda row: (row[0], row[2], row[1]))


def write(out: pathlib.Path, routes, shops) -> None:
    out.mkdir(parents=True, exist_ok=True)
    travel = ['group\tname\tevery\tdepartures'] + [
        f'{group}\t{name}\t{every}\t' + ','.join(f'{board}:{leave}' for board, leave in departures)
        for group, name, every, departures in routes]
    (out / 'phoenix_travel.tsv').write_text('\n'.join(travel) + '\n', encoding='utf-8')
    guilds = ['guild\tshops\tzone\topen\tclose\tholiday'] + ['\t'.join(str(field) for field in row) for row in shops]
    (out / 'phoenix_guilds.tsv').write_text('\n'.join(guilds) + '\n', encoding='utf-8')


def main(argv) -> int:
    if len(argv) != 3:
        print('usage: world.py SERVER_DIR OUT_DIR', file=sys.stderr)
        return 2
    try:
        server = pathlib.Path(argv[1])
        routes, shops = build_travel(server), build_guilds(server)
    except (OSError, WorldDataError) as error:
        print(f'world: {error}', file=sys.stderr)
        return 1
    write(pathlib.Path(argv[2]), routes, shops)
    print(f'world: {len(routes)} routes and {len(shops)} guild shops -> {argv[2]}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
