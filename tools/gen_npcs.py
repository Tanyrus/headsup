#!/usr/bin/env python3
"""The format of data/phoenix_npcs.tsv and the lists it points into, data/phoenix_npc_shops.tsv,
data/phoenix_npc_links.tsv and data/phoenix_npc_systems.tsv (written by tools/phoenix/refresh.sh).

With --check DIR, check DIR's four files and write nothing: refresh.sh does, before it copies a refresh's NPC files
into data/. Nothing reads them into the plugin yet.
"""
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gen_mobdata import DataError, number, table_rows  # noqa: E402

NPCS_NAME = 'phoenix_npcs.tsv'
SHOPS_NAME = 'phoenix_npc_shops.tsv'
LINKS_NAME = 'phoenix_npc_links.tsv'
SYSTEMS_NAME = 'phoenix_npc_systems.tsv'
# phoenix_npcs.tsv: every NPC content restriction leaves on, by server ID. script: its internal name, empty for a blank
# placeholder; name: the name it shows, empty for one that shows none. pos: x, y, z and rotation (0 to 255) where the
# dump found it. trades: the items its trade handlers ask for; gives: the items it hands out, quest rewards included;
# keyItems: the key items it gives; needs: the key items it checks for; pops: the mobs it spawns; shop: its list in
# phoenix_npc_shops.tsv, 0 for none; links: its quests, missions and battlefields in phoenix_npc_links.tsv; systems:
# its calls in phoenix_npc_systems.tsv; unread: UNREAD kinds its scripts do but in a way the refresh could not read.
# Lists are comma-separated and increasing.
NPC_COLUMNS = ['id', 'zone', 'script', 'name', 'pos', 'trades', 'gives', 'keyItems', 'needs', 'pops', 'shop', 'links',
               'systems', 'unread']
LIST_LIMITS = {'trades': (1, 0xFFFF), 'gives': (1, 0xFFFF), 'keyItems': (1, 0xFFFF), 'needs': (1, 0xFFFF),
               'pops': (1, 0xFFFFFFFF), 'links': (1, 0xFFFF), 'systems': (1, 0xFFFF)}
UNREAD = ['trade', 'gives', 'pops', 'shop']
# phoenix_npc_shops.tsv: each distinct stock, numbered from 1 with its rows together: an item, its price before fame,
# and the nation rank xi.shop.nation sells it from (1, 2 or 3), 0 for a shop that sells it to all.
SHOP_COLUMNS = ['list', 'item', 'price', 'rank']
SHOP_LIMITS = {'list': (1, 0xFFFF), 'item': (1, 0xFFFF), 'price': (0, 0xFFFFFFFF), 'rank': (0, 3)}
# phoenix_npc_links.tsv: each quest, mission and battlefield an NPC takes part in, numbered from 1. log: the quest or
# mission log (0 for a battlefield or hidden quest); id: the quest, mission or battlefield ID; file: its script, empty
# for a quest an NPC's own script names; expansion: the one EXPANSIONS names from its folder, empty when the folder does
# not tell; live: 0 when live's settings turn its expansion off, or every step of it an NPC takes part in needs a
# setting live has off (the trust quests' ENABLE_TRUST_QUESTS), else 1. A city quest folder names no expansion, so a
# later quest there that no setting gates counts as live.
LINK_COLUMNS = ['link', 'kind', 'log', 'id', 'name', 'file', 'expansion', 'live']
LINK_KINDS = ['quest', 'mission', 'battlefield', 'hidden']
EXPANSIONS = ['rotz', 'cop', 'toau', 'wotg', 'acp', 'amk', 'asa', 'abyssea', 'soa', 'rov', 'tvr']
# phoenix_npc_systems.tsv: each distinct call an NPC makes into a shared system (xi.helm, xi.dynamis, ...), numbered
# from 1: the system and the call as written, spaces collapsed.
SYSTEM_COLUMNS = ['system', 'family', 'call']
POSITION = re.compile(r'-?[0-9]+\.[0-9]{3},-?[0-9]+\.[0-9]{3},-?[0-9]+\.[0-9]{3},[0-9]{1,3}')


def increasing(value: str, lo: int, hi: int, what: str) -> list:
    values = [number(part, lo, hi, what) for part in value.split(',')] if value else []
    if values != sorted(set(values)):
        raise DataError(f'{what} must be increasing, got {value!r}')
    return values


def parse_npcs(text: str, source: str = NPCS_NAME) -> list:
    npcs, last_id = [], 0
    for line, row in table_rows(text, NPC_COLUMNS, source):
        at = f'{source}:{line}'
        npc = {'id': number(row['id'], 1, 0xFFFFFFFF, f'{at}: id'), 'zone': number(row['zone'], 0, 0xFFFF, f'{at}: zone'),
               'script': row['script'], 'name': row['name'], 'shop': number(row['shop'], 0, 0xFFFF, f'{at}: shop')}
        if not POSITION.fullmatch(row['pos']) or int(row['pos'].rsplit(',', 1)[1]) > 0xFF:
            raise DataError(f'{at}: pos must be x,y,z with three decimals and a rotation from 0 to 255, got {row["pos"]!r}')
        *xyz, rotation = row['pos'].split(',')
        npc['pos'] = [float(v) for v in xyz] + [int(rotation)]
        for column, (lo, hi) in LIST_LIMITS.items():
            npc[column] = increasing(row[column], lo, hi, f'{at}: {column}')
        npc['unread'] = row['unread'].split(',') if row['unread'] else []
        if any(kind not in UNREAD for kind in npc['unread']):
            raise DataError(f'{at}: unread must name {", ".join(UNREAD)}, got {row["unread"]!r}')
        if npc['id'] <= last_id:
            raise DataError(f'{at}: ids must be strictly increasing')
        last_id = npc['id']
        npcs.append(npc)
    return npcs


def numbered(rows, column: str, source: str) -> list:
    """Rows numbered from 1 in their column, one each."""
    for expected, (line, row) in enumerate(rows, start=1):
        if row[column] != expected:
            raise DataError(f'{source}:{line}: {column} must count from 1, got {row[column]}')
    return [row for _, row in rows]


def parse_shops(text: str, source: str = SHOPS_NAME) -> list:
    """The stocks in order, list n at index n - 1, each a list of {list, item, price, rank}."""
    lists = []
    for line, row in table_rows(text, SHOP_COLUMNS, source):
        entry = {column: number(row[column], *limits, f'{source}:{line}: {column}')
                 for column, limits in SHOP_LIMITS.items()}
        if entry['list'] == len(lists) + 1:
            lists.append([])
        elif entry['list'] != len(lists):
            raise DataError(f"{source}:{line}: lists must be numbered from 1 with each list's rows together")
        lists[-1].append(entry)
    return lists


def live_flag(value: str, what: str) -> int:
    if value not in ('0', '1'):
        raise DataError(f'{what} must be 0 or 1, got {value!r}')
    return int(value)


def parse_links(text: str, source: str = LINKS_NAME) -> list:
    rows = []
    for line, row in table_rows(text, LINK_COLUMNS, source):
        at = f'{source}:{line}'
        if row['kind'] not in LINK_KINDS:
            raise DataError(f'{at}: kind must be one of {", ".join(LINK_KINDS)}, got {row["kind"]!r}')
        if row['expansion'] and row['expansion'] not in EXPANSIONS:
            raise DataError(f'{at}: expansion must be empty or one of {", ".join(EXPANSIONS)}, got {row["expansion"]!r}')
        if not row['name']:
            raise DataError(f'{at}: empty name')
        rows.append((line, {'link': number(row['link'], 1, 0xFFFF, f'{at}: link'), 'kind': row['kind'],
                            'log': number(row['log'], 0, 0xFF, f'{at}: log'), 'id': number(row['id'], 0, 0xFFFF, f'{at}: id'),
                            'name': row['name'], 'file': row['file'], 'expansion': row['expansion'],
                            'live': live_flag(row['live'], f'{at}: live')}))
    return numbered(rows, 'link', source)


def parse_systems(text: str, source: str = SYSTEMS_NAME) -> list:
    rows = []
    for line, row in table_rows(text, SYSTEM_COLUMNS, source):
        at = f'{source}:{line}'
        if not re.fullmatch(r'[A-Za-z0-9]+', row['family']) or not row['call'].startswith(row['family'] + '.'):
            raise DataError(f'{at}: a call must start with its system, got {row["family"]!r} and {row["call"]!r}')
        rows.append((line, {'system': number(row['system'], 1, 0xFFFF, f'{at}: system'), 'family': row['family'],
                            'call': row['call']}))
    return numbered(rows, 'system', source)


def check(npcs, shops, links, systems) -> None:
    """Every number an NPC names is in its list, and every list entry is named."""
    used_shops, used_links, used_systems = set(), set(), set()
    for npc in npcs:
        if npc['shop'] > len(shops) or any(n > len(links) for n in npc['links']) or \
                any(n > len(systems) for n in npc['systems']):
            raise DataError(f"NPC {npc['id']} names a shop, link or system past the {len(shops)}, {len(links)} and "
                            f'{len(systems)} in {SHOPS_NAME}, {LINKS_NAME} and {SYSTEMS_NAME}')
        used_shops.add(npc['shop'])
        used_links.update(npc['links'])
        used_systems.update(npc['systems'])
    for name, count, used in ((SHOPS_NAME, len(shops), used_shops), (LINKS_NAME, len(links), used_links),
                              (SYSTEMS_NAME, len(systems), used_systems)):
        unused = sorted(set(range(1, count + 1)) - used)
        if unused:
            raise DataError(f'{name}: {len(unused)} entries no NPC names, the first {unused[0]}')


def load(data: pathlib.Path):
    tables = [parse((data / name).read_text(encoding='utf-8'), str(data / name)) for parse, name in
              ((parse_npcs, NPCS_NAME), (parse_shops, SHOPS_NAME), (parse_links, LINKS_NAME),
               (parse_systems, SYSTEMS_NAME))]
    check(*tables)
    return tables


def main(argv) -> int:
    if len(argv) != 3 or argv[1] != '--check':
        print('usage: gen_npcs.py --check DATA_DIR', file=sys.stderr)
        return 2
    data = pathlib.Path(argv[2])
    try:
        npcs, shops, links, systems = load(data)
    except (OSError, DataError) as error:
        print(f'gen_npcs: {error}', file=sys.stderr)
        return 1
    print(f'npcs: {len(npcs)} NPCs, {len(shops)} shops, {len(links)} links and {len(systems)} system calls in {data} pass')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
