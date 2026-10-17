#!/usr/bin/env python3
"""The format of data/phoenix_npcs.tsv and the lists it points into, data/phoenix_npc_shops.tsv,
data/phoenix_npc_links.tsv, data/phoenix_npc_systems.tsv and data/phoenix_key_items.tsv, and of what the refresh's
probe saw each ??? do, data/phoenix_npc_acts.tsv and data/phoenix_npc_texts.tsv (all written by
tools/phoenix/refresh.sh), and src/generated/npcdata.inc from them: each NPC with something to show, for the plugin's
Target popup.

With --check DIR, check DIR's files and write nothing: refresh.sh does, before it copies a refresh's NPC files into
data/.
"""
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gen_mobdata import META_NAME, DataError, c_string, number, read_commit, table_rows  # noqa: E402

NPCS_NAME = 'phoenix_npcs.tsv'
SHOPS_NAME = 'phoenix_npc_shops.tsv'
LINKS_NAME = 'phoenix_npc_links.tsv'
SYSTEMS_NAME = 'phoenix_npc_systems.tsv'
KEY_ITEMS_NAME = 'phoenix_key_items.tsv'
ACTS_NAME = 'phoenix_npc_acts.tsv'
TEXTS_NAME = 'phoenix_npc_texts.tsv'
# phoenix_npcs.tsv: every NPC content restriction leaves on, by server ID. script: its internal name, empty for a blank
# placeholder; name: the name it shows, empty for one that shows none. pos: x, y, z and rotation (0 to 255) where the
# dump found it. trades: the items its trade handlers ask for; gives: the items it hands out, quest rewards included;
# keyItems: the key items it gives; needs: the key items it checks for; pops: the mobs it spawns; shop: its list in
# phoenix_npc_shops.tsv, 0 for none; links: its quests, missions and battlefields in phoenix_npc_links.tsv; systems:
# its calls in phoenix_npc_systems.tsv; unread: UNREAD kinds its scripts do but in a way the refresh could not read.
# A step of a quest, mission or battlefield adds to these only when its link is live and its own check needs no setting
# live has off, so the later content of an NPC live has stays out; links still names every one. Lists are
# comma-separated and increasing.
NPC_COLUMNS = ['id', 'zone', 'script', 'name', 'pos', 'trades', 'gives', 'keyItems', 'needs', 'pops', 'shop', 'links',
               'systems', 'unread']
LIST_LIMITS = {'trades': (1, 0xFFFF), 'gives': (1, 0xFFFF), 'keyItems': (1, 0xFFFF), 'needs': (1, 0xFFFF),
               'pops': (1, 0xFFFFFFFF), 'links': (1, 0xFFFF), 'systems': (1, 0xFFFF)}
UNREAD = ['trade', 'gives', 'pops', 'shop']
# phoenix_npc_shops.tsv: each distinct stock, numbered from 1 with its rows together: an item, its price before fame,
# and the nation rank xi.shop.nation sells it from (1, 2 or 3), 0 for a shop that sells it to all; fame, the same on
# each of a list's rows, is 1 when the shop's prices move with fame (xi.shop.nation, and xi.shop.general given a fame
# area: from 10% off to 10% more).
SHOP_COLUMNS = ['list', 'item', 'price', 'rank', 'fame']
SHOP_LIMITS = {'list': (1, 0xFFFF), 'item': (1, 0xFFFF), 'price': (0, 0xFFFFFFFF), 'rank': (0, 3), 'fame': (0, 1)}
# phoenix_npc_links.tsv: each quest, mission and battlefield an NPC takes part in, numbered from 1. log: the quest or
# mission log (0 for a battlefield or hidden quest); id: the quest, mission or battlefield ID; file: its script, empty
# for a quest an NPC's own script names; expansion: the one EXPANSIONS names from its folder, empty when the folder does
# not tell; live: 0 when live's settings turn its expansion off, or every step of it an NPC takes part in needs a
# setting live has off (the trust quests' ENABLE_TRUST_QUESTS), else 1. A city quest folder names no expansion, so a
# later quest there that no setting gates counts as live.
LINK_COLUMNS = ['link', 'kind', 'log', 'id', 'name', 'file', 'expansion', 'live']
LINK_KINDS = ['quest', 'mission', 'battlefield', 'hidden']
# Each quest and mission log's name, by its number (xi.questLog and xi.mission.log_id): npcs.py checks the server's own
# names are these, written as the game's quest log and the players' short names for the expansions write them.
QUEST_LOGS = ["San d'Oria", 'Bastok', 'Windurst', 'Jeuno', 'Other Areas', 'Outlands', 'Aht Urhgan', 'Crystal War',
              'Abyssea', 'Adoulin', 'Coalition']
MISSION_LOGS = ["San d'Oria", 'Bastok', 'Windurst', 'Zilart', 'ToAU', 'WotG', 'CoP', 'Assault', 'Campaign', 'ACP', 'AMK',
                'ASA', 'SoA', 'RoV', 'TVR']
EXPANSIONS = ['rotz', 'cop', 'toau', 'wotg', 'acp', 'amk', 'asa', 'abyssea', 'soa', 'rov', 'tvr']
# phoenix_npc_systems.tsv: each distinct call an NPC makes into a shared system (xi.helm, xi.dynamis, ...), numbered
# from 1: the system and the call as written, spaces collapsed.
SYSTEM_COLUMNS = ['system', 'family', 'call']
# phoenix_key_items.tsv: the name of each key item an NPC gives or checks for, by its ID: its identifier in Phoenix's
# scripts in words, which drops the game's punctuation ("C L Report" for the game's "C.L. Report").
KEY_ITEM_COLUMNS = ['id', 'name']
# What a shared system's call makes an NPC, in words, for the popup: the first rule of its system whose pattern the
# call matches gives the label, {} filled with the pattern's group in words, or None for a call that says nothing a
# player looks for (a walking path, a debug print, a part of a system that has its own label elsewhere). Every system
# an NPC calls needs a rule, so a new one fails the check until it is named here.
ANY = ''
SYSTEM_RULES = {
    'appraisal': [(ANY, 'Appraisal')],
    'armorStorage': [(ANY, 'Armor storage')],
    'assault': [(ANY, 'Assault')],
    'automaton': [(ANY, None)],
    'barge': [(r'timekeeper', 'Barge timekeeper'), (r'TicketShop', 'Barge tickets')],
    'battlefield': [(ANY, 'Battlefield entrance')],
    'beastmenTreasure': [(ANY, "Beastmen's treasure")],
    'besieged': [(r'RunicPortal\(player, xi\.teleport\.runic_portal\.([A-Z_]+)\)', 'Runic portal to {}'),
                 (r'besieged\.shop\(', 'Besieged shop'), (r'besieged\.on', 'Besieged'), (ANY, None)],
    'brigandsChart': [(ANY, "Brigand's chart")],
    'caskets': [(ANY, 'Treasure casket')],
    'chocoboRacing': [(ANY, 'Chocobo racing')],
    'chocoboRaising': [(r'Debug', None), (ANY, 'Chocobo raising')],
    'clamming': [(ANY, 'Clamming')],
    'conquest': [(r'\.overseer', 'Conquest overseer'), (r'\.vendor', 'Outpost vendor'),
                 (r'\.teleporter', 'Outpost teleporter')],
    'crafting': [(r'guildPoint.*xi\.guild\.([A-Z_]+)', '{} guild points'), (r'guildMaster', 'Guild master'),
                 (r'ImageSupport', 'Image support'), (r'ephemeralMoogle', 'Crystal storage'), (ANY, None)],
    'dealerMoogle': [(ANY, 'Mog Bonanza')],
    'dynamis': [(r'entryNpc', 'Dynamis entrance'), (r'hourglassAndCurrency', 'Dynamis hourglasses and currency'),
                (r'debugPrint', None), (ANY, 'Dynamis')],
    'einherjar': [(ANY, 'Einherjar')],
    'enm': [(ANY, 'ENM timer')],
    'equipment': [(ANY, None)],
    'eventStorage': [(ANY, 'Event item storage')],
    'expeditionaryForce': [(ANY, 'Expeditionary Force')],
    'extravaganza': [(ANY, None)],  # the Alter Ego Extravaganza, of trusts, which live has not
    'festiveMoogle': [(ANY, 'Festive moogle')],
    'fishingContest': [(ANY, 'Fishing contest')],
    'goblinfootprint': [(ANY, 'Cutscene replay')],
    'guildShops': [(ANY, 'Guild shop')],
    'helm': [(r'helmType\.([A-Z_]+)', '{} point')],
    'homepoint': [(ANY, 'Home point')],
    'instance': [(r'xi\.zone\.([A-Z_]+)', 'Entrance to {}'), (ANY, None)],
    'kidMithraShop': [(ANY, None)],
    'limbus': [(ANY, 'Limbus')],
    'linkshellConcierge': [(ANY, 'Linkshell concierge')],
    'manaclipper': [(ANY, 'Manaclipper timekeeper')],
    'mannequin': [(ANY, None)],
    'maps': [(ANY, 'Maps')],
    'maws': [(ANY, 'Cavernous Maw')],
    'moghouse': [(r'MogLocker', 'Mog Locker'), (r'moogle', 'Mog House moogle'), (ANY, None)],
    'path': [(ANY, None)],
    'pirates': [(ANY, None)],
    'piratesChart': [(ANY, "Pirate's chart")],
    'rentalChocobo': [(ANY, 'Chocobo rental')],
    'seals': [(ANY, 'Seals and crests')],
    'shop': [(r'handleRegionalShop', 'Regional goods, by conquest'), (r'handleValerianoShop', 'Troupe Valeriano')],
    'strangeApparatus': [(ANY, 'Strange apparatus')],
    'teleport': [(r'xi\.teleport\.id\.([A-Z_]+)', 'Teleports to {}'), (r'toChamberOfPassage', 'Teleports to the Chamber of Passage'),
                 (ANY, 'Teleports')],
    'titleChanger': [(ANY, 'Title changer')],
    'transport': [(r'Timekeeper', 'Boat timekeeper'), (r'Message\(', None)],
    'treasure': [(ANY, 'Treasure chest or coffer')],
    'waypoint': [(ANY, 'Waypoint')],
}
SMALL_WORDS = {'of', 'the', 'and', 'a', 'an', 'in', 'on', 'to', 'for', 'with', 'from', 'at', 'by'}
# phoenix_npc_acts.tsv: what each ??? did when the refresh's probe clicked and traded it on the server, one distinct
# thing a row, the rows of an NPC together: act, a click, a trade, or the ??? shown or hidden by its zone; items, a
# trade's items (item, or item*qty); when, the condition tokens of the plainest scenario it happened in (hour:FROM-TO,
# weather:N, job:N, day:N, nation:N, keyItem:N held, noKeyItem:N not held, keyItems held, quest under way, link:N with
# step:N a quest step, primed for the ??? 's own state, nmKilled, roll for a lucky roll, mob:SCRIPT for a mob's death,
# zoneStart); says, its messages (a text of phoenix_npc_texts.tsv, then the message's parameters, ':' between);
# pops, gives and keyItems as in phoenix_npcs.tsv; event, 1 when it starts a cutscene.
ACT_COLUMNS = ['npc', 'act', 'items', 'when', 'says', 'pops', 'gives', 'keyItems', 'event']
ACT_KINDS = ['click', 'trade', 'shows', 'hides']
WHEN = re.compile(r'hour:\d+-\d+|(?:weather|job|day|nation|keyItem|noKeyItem|link|step):\d+|mob:\w+|'
                  r'keyItems|quest|primed|nmKilled|roll|zoneStart')
# phoenix_npc_texts.tsv: the messages acts say, numbered from 1: the zone and its message number, and the words as the
# zone's IDs.lua gives them, with <item>, <keyitem>, <number> and the like where a parameter goes.
TEXT_COLUMNS = ['text', 'zone', 'message', 'words']
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
    """The stocks in order, list n at index n - 1, each a list of {list, item, price, rank, fame}."""
    lists = []
    for line, row in table_rows(text, SHOP_COLUMNS, source):
        entry = {column: number(row[column], *limits, f'{source}:{line}: {column}')
                 for column, limits in SHOP_LIMITS.items()}
        if entry['list'] == len(lists) + 1:
            lists.append([])
        elif entry['list'] != len(lists):
            raise DataError(f"{source}:{line}: lists must be numbered from 1 with each list's rows together")
        elif entry['fame'] != lists[-1][0]['fame']:
            raise DataError(f"{source}:{line}: fame must be the same on each of a list's rows")
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


def parse_key_items(text: str, source: str = KEY_ITEMS_NAME) -> dict:
    """{key item: name}, in ID order."""
    names = {}
    for line, row in table_rows(text, KEY_ITEM_COLUMNS, source):
        at = f'{source}:{line}'
        key_item = number(row['id'], 1, 0xFFFF, f'{at}: id')
        if names and key_item <= max(names):
            raise DataError(f'{at}: ids must be strictly increasing')
        if not row['name']:
            raise DataError(f'{at}: empty name')
        names[key_item] = row['name']
    return names


def words(identifier: str) -> str:
    """CLOISTER_OF_FLAMES -> Cloister of Flames, THE_ASHU_TALIF -> The Ashu Talif."""
    return ' '.join(word if n and word in SMALL_WORDS else word.capitalize()
                    for n, word in enumerate(identifier.lower().split('_')))


def purpose(family: str, call: str):
    """A system call in words, or None when it says nothing of what an NPC is for (SYSTEM_RULES)."""
    if family not in SYSTEM_RULES:
        raise DataError(f'system {family} has no rule in SYSTEM_RULES ({call})')
    for pattern, label in SYSTEM_RULES[family]:
        found = re.search(pattern, call)
        if found:
            return label.format(*(words(group) for group in found.groups())) if label else None
    raise DataError(f'no rule of {family} matches {call}')


def parse_acts(text: str, source: str = ACTS_NAME) -> list:
    acts, last = [], 0
    for line, row in table_rows(text, ACT_COLUMNS, source):
        at = f'{source}:{line}'
        npc = number(row['npc'], 1, 0xFFFFFFFF, f'{at}: npc')
        if npc < last:
            raise DataError(f"{at}: an NPC's acts must be together, in ID order")
        last = npc
        if row['act'] not in ACT_KINDS:
            raise DataError(f'{at}: act must be one of {", ".join(ACT_KINDS)}, got {row["act"]!r}')
        items = []
        for part in row['items'].split(',') if row['items'] else []:
            item, _, qty = part.partition('*')
            items.append((number(item, 1, 0xFFFF, f'{at}: items'), number(qty or '1', 1, 0xFFFF, f'{at}: items')))
        if [i for i, _ in items] != sorted({i for i, _ in items}):
            raise DataError(f'{at}: items must be increasing')
        when = row['when'].split(',') if row['when'] else []
        bad = [w for w in when if not WHEN.fullmatch(w)]
        if bad:
            raise DataError(f'{at}: when has {bad[0]!r}, which no condition token is')
        says = []
        for part in row['says'].split(',') if row['says'] else []:
            fields = part.split(':')
            says.append((number(fields[0], 1, 0xFFFF, f'{at}: says'),
                         [number(f, 0, 0xFFFFFFFF, f'{at}: says') for f in fields[1:]]))
        act = {'npc': npc, 'act': row['act'], 'items': items, 'when': when, 'says': says,
               'event': live_flag(row['event'], f'{at}: event')}
        for column, (lo, hi) in (('pops', (1, 0xFFFFFFFF)), ('gives', (1, 0xFFFF)), ('keyItems', (1, 0xFFFF))):
            act[column] = increasing(row[column], lo, hi, f'{at}: {column}')
        acts.append(act)
    return acts


def parse_texts(text: str, source: str = TEXTS_NAME) -> list:
    rows = []
    for line, row in table_rows(text, TEXT_COLUMNS, source):
        at = f'{source}:{line}'
        if not row['words']:
            raise DataError(f'{at}: empty words')
        rows.append((line, {'text': number(row['text'], 1, 0xFFFF, f'{at}: text'),
                            'zone': number(row['zone'], 0, 0xFFFF, f'{at}: zone'),
                            'message': number(row['message'], 0, 0xFFFF, f'{at}: message'), 'words': row['words']}))
    return numbered(rows, 'text', source)


def check_acts(npcs, links, key_items, acts, texts) -> None:
    """Every NPC, text, link and key item an act names is in its list, and every text is said."""
    ids = {npc['id'] for npc in npcs}
    used = set()
    for act in acts:
        where = f"{ACTS_NAME}: NPC {act['npc']}"
        if act['npc'] not in ids:
            raise DataError(f'{where} is not in {NPCS_NAME}')
        for text, _ in act['says']:
            if text > len(texts):
                raise DataError(f'{where} says text {text}, past the {len(texts)} in {TEXTS_NAME}')
            used.add(text)
        for token in act['when']:
            head, _, value = token.partition(':')
            if head == 'link' and (int(value) > len(links) or not links[int(value) - 1]['live']):
                raise DataError(f'{where} names link {value}, which {LINKS_NAME} does not have live')
            if head in ('keyItem', 'noKeyItem') and int(value) not in key_items:
                raise DataError(f'{where} names key item {value}, which {KEY_ITEMS_NAME} does not')
        unnamed = sorted(set(act['keyItems']) - set(key_items))
        if unnamed:
            raise DataError(f'{where} gives key item {unnamed[0]}, which {KEY_ITEMS_NAME} does not')
    unused = sorted(set(range(1, len(texts) + 1)) - used)
    if unused:
        raise DataError(f'{TEXTS_NAME}: {len(unused)} texts no act says, the first {unused[0]}')


def check(npcs, shops, links, systems, key_items, acts=(), texts=()) -> None:
    """Every number an NPC names is in its list, every key item it names has a name, and every list entry and name is
    named."""
    used_shops, used_links, used_systems = set(), set(), set()
    for npc in npcs:
        if npc['shop'] > len(shops) or any(n > len(links) for n in npc['links']) or \
                any(n > len(systems) for n in npc['systems']):
            raise DataError(f"NPC {npc['id']} names a shop, link or system past the {len(shops)}, {len(links)} and "
                            f'{len(systems)} in {SHOPS_NAME}, {LINKS_NAME} and {SYSTEMS_NAME}')
        used_shops.add(npc['shop'])
        used_links.update(npc['links'])
        used_systems.update(npc['systems'])
        unnamed = sorted(set(npc['keyItems'] + npc['needs']) - set(key_items))
        if unnamed:
            raise DataError(f"NPC {npc['id']} names key item {unnamed[0]}, which {KEY_ITEMS_NAME} does not")
    for name, count, used in ((SHOPS_NAME, len(shops), used_shops), (LINKS_NAME, len(links), used_links),
                              (SYSTEMS_NAME, len(systems), used_systems)):
        unused = sorted(set(range(1, count + 1)) - used)
        if unused:
            raise DataError(f'{name}: {len(unused)} entries no NPC names, the first {unused[0]}')
    for system in systems:
        purpose(system['family'], system['call'])
    named = {k for npc in npcs for k in npc['keyItems'] + npc['needs']} | {k for a in acts for k in a['keyItems']}
    named |= {int(w.split(':')[1]) for a in acts for w in a['when'] if w.startswith(('keyItem:', 'noKeyItem:'))}
    unused = sorted(set(key_items) - named)
    if unused:
        raise DataError(f'{KEY_ITEMS_NAME}: {len(unused)} key items no NPC names, the first {unused[0]}')
    check_acts(npcs, links, key_items, acts, texts)


def load(data: pathlib.Path):
    tables = [parse((data / name).read_text(encoding='utf-8'), str(data / name)) for parse, name in
              ((parse_npcs, NPCS_NAME), (parse_shops, SHOPS_NAME), (parse_links, LINKS_NAME),
               (parse_systems, SYSTEMS_NAME), (parse_key_items, KEY_ITEMS_NAME), (parse_acts, ACTS_NAME),
               (parse_texts, TEXTS_NAME))]
    check(*tables)
    return tables


# The lists of an NPC's row that npcdata.inc carries, each into the pool of its kind: NpcRecord's spans, in its order.
# links holds the NPC's live links by their number in kNpcLinks, which has only those; purposes what its systems make
# it, by their place in kNpcPurposes.
POOLS = {'trades': 'kNpcItems', 'gives': 'kNpcItems', 'keyItems': 'kNpcKeyItems', 'needs': 'kNpcKeyItems',
         'pops': 'kNpcMobs', 'links': 'kNpcLinkIds', 'purposes': 'kNpcPurposeIds'}
POOL_TYPES = {'kNpcItems': 'uint16_t', 'kNpcKeyItems': 'uint16_t', 'kNpcMobs': 'uint32_t', 'kNpcLinkIds': 'uint16_t',
              'kNpcPurposeIds': 'uint16_t'}
# The pools of an act's parts, each with its empty entry: an item and its count, a condition, and a message's
# parameters and the message (a text and where its parameters sit).
STRUCT_POOLS = {'kNpcActItems': '{0, 0}', 'kNpcWhen': '{0, 0, 0, ""}', 'kNpcSayParams': '0u', 'kNpcSays': '{0, {0, 0}}'}
STRUCT_POOLS_TYPES = {'kNpcActItems': 'NpcActItem', 'kNpcWhen': 'NpcWhenEntry', 'kNpcSayParams': 'uint32_t',
                      'kNpcSays': 'NpcSay'}
SHOWN_UNREAD = ['trade', 'gives', 'pops', 'shop']  # what the popup says the data cannot name
VALUES_A_LINE = 16


# The condition kinds of an act, in NpcWhen's order: hour holds from and to, link the live link, mob the mob's name.
WHEN_KINDS = ['hour', 'weather', 'job', 'day', 'nation', 'keyItem', 'noKeyItem', 'keyItems', 'quest', 'link', 'step',
              'primed', 'nmKilled', 'roll', 'mob', 'zoneStart']


def when_entry(token: str, numbers) -> str:
    """One condition token as an NpcWhen entry: {kind, a, b, name}."""
    head, _, value = token.partition(':')
    a = b = 0
    name = ''
    if head == 'hour':
        a, b = (int(x) for x in value.split('-'))
    elif head == 'link':
        a = numbers[int(value)]
    elif head == 'mob':
        name = value.replace('_', ' ')
    elif value:
        a = int(value)
    return f'{{{WHEN_KINDS.index(head)}, {a}, {b}, {c_string(name)}}}'


def act_tables(acts, texts, numbers, span) -> tuple:
    """kNpcActs and the pools its acts point into, and each NPC's acts as (first, count) in kNpcActs; span(pool,
    values) places a list in a pool, each distinct list once."""
    records, of_npc = ['    {0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, false},'], {}
    for act in acts:
        first, count = of_npc.get(act['npc'], (len(records), 0))
        of_npc[act['npc']] = (first, count + 1)
        says = tuple(f"{{{text}, {span('kNpcSayParams', tuple(params))}}}" for text, params in act['says'])
        records.append('    {' + ', '.join([
            str(ACT_KINDS.index(act['act'])),
            span('kNpcActItems', tuple(f'{{{item}, {qty}}}' for item, qty in act['items'])),
            span('kNpcWhen', tuple(when_entry(w, numbers) for w in act['when'])),
            span('kNpcSays', says), span('kNpcMobs', tuple(act['pops'])), span('kNpcItems', tuple(act['gives'])),
            span('kNpcKeyItems', tuple(act['keyItems'])), 'true' if act['event'] else 'false']) + '},')
    if len(records) > 0xFFFF:
        raise DataError('the acts do not fit NpcSpan')
    lines = ['', 'const NpcText kNpcTexts[] = {', '    {0, ""},']
    lines += [f"    {{{text['zone']}, {c_string(text['words'])}}}," for text in texts]
    lines += ['};', '', 'const NpcAct kNpcActs[] = {'] + records + ['};']
    return lines, of_npc


def shown(npc) -> bool:
    """Whether the popup has anything to say of an NPC, its links its live ones."""
    return any(npc[column] for column in POOLS) or npc['shop'] > 0 or \
        any(kind in SHOWN_UNREAD for kind in npc['unread'])


def shop_tables(shops) -> list:
    """kNpcStock, every list's items one after another after the empty entry at 0, and kNpcShops, each list at its
    number, where its items sit and whether fame moves its prices."""
    stock, starts = ['    {0, 0, 0u},'], ['    {{0, 0}, false},']
    for entries in shops:
        if len(entries) > 0xFF:
            raise DataError(f"shop {entries[0]['list']}: {len(entries)} items do not fit NpcSpan")
        starts.append(f"    {{{{{len(stock)}, {len(entries)}}}, {'true' if entries[0]['fame'] else 'false'}}},")
        stock += [f"    {{{e['item']}, {e['rank']}, {e['price']}u}}," for e in entries]
    if len(stock) > 0xFFFF:
        raise DataError('the shops do not fit NpcSpan')
    return ['', 'const NpcStockItem kNpcStock[] = {'] + stock + ['};', '', 'const NpcShop kNpcShops[] = {'] + starts + \
        ['};']


def link_tables(live) -> list:
    """kNpcLinks, the live links from 1 after the empty one at 0, and the log names a link's log reads."""
    lines = ['', 'const NpcLink kNpcLinks[] = {', '    {0, 0, ""},']
    lines += [f"    {{{LINK_KINDS.index(link['kind'])}, {link['log']}, {c_string(link['name'])}}}," for link in live]
    for table, names in (('kQuestLogNames', QUEST_LOGS), ('kMissionLogNames', MISSION_LOGS)):
        lines += ['};', '', f'const char* const {table}[] = {{'] + [f'    {c_string(name)},' for name in names]
    return lines + ['};']


def purpose_table(purposes) -> list:
    return ['', 'const char* const kNpcPurposes[] = {'] + [f'    {c_string(label)},' for label in purposes] + ['};']


def generate(npcs, shops, links, systems, key_items, commit: str, acts=(), texts=()) -> str:
    """npcdata.inc: kNpcs, each NPC shown() or with acts in ID order with where its lists sit in their pools, each pool
    holding each distinct list once after the empty entry at 0; the shops by number; the links live has, which alone
    the popup shows; what its systems make it in words; what the probe saw each ??? do, with the words it says; and
    kKeyItemNames in ID order."""
    live = [link for link in links if link['live']]
    numbers = {link['link']: n for n, link in enumerate(live, start=1)}
    labels = [purpose(s['family'], s['call']) for s in systems]
    purposes = {'': 0}
    for label in labels:
        if label is not None:
            purposes.setdefault(label, len(purposes))
    pools = {pool: [0] for pool in POOL_TYPES}
    starts = {pool: {(): 0} for pool in POOL_TYPES}
    for pool, empty in STRUCT_POOLS.items():
        pools[pool], starts[pool] = [empty], {(): 0}

    def span(pool, values):
        if len(values) > 0xFF:
            raise DataError(f'{len(values)} entries of {pool} do not fit NpcSpan')
        if values not in starts[pool]:
            starts[pool][values] = len(pools[pool])
            pools[pool] += values
        return f'{{{starts[pool][values]}, {len(values)}}}'

    act_lines, acts_of = act_tables(acts, texts, numbers, span)
    records = []
    for npc in npcs:
        said = [purposes[labels[n - 1]] for n in npc['systems'] if labels[n - 1] is not None]
        npc = dict(npc, links=[numbers[n] for n in npc['links'] if n in numbers],
                   purposes=[p for i, p in enumerate(said) if p not in said[:i]])
        if not shown(npc) and npc['id'] not in acts_of:
            continue
        spans = []
        for column, pool in POOLS.items():
            values = tuple(npc[column])
            if len(values) > 0xFF:
                raise DataError(f"NPC {npc['id']}: {len(values)} {column} do not fit NpcSpan")
            if values not in starts[pool]:
                starts[pool][values] = len(pools[pool])
                pools[pool] += values
            spans.append(f'{{{starts[pool][values]}, {len(values)}}}')
        unread = sum(1 << UNREAD.index(kind) for kind in npc['unread'])
        first, count = acts_of.get(npc['id'], (0, 0))
        records.append(f"    {{{npc['id']}u, {', '.join(spans)}, {{{first}, {count}}}, {npc['shop']}, {unread}}},")
    if any(len(pool) > 0xFFFF for pool in pools.values()):
        raise DataError('the NPC lists do not fit NpcSpan')
    lines = [f'// Generated by tools/gen_npcs.py from data/{NPCS_NAME}, data/{SHOPS_NAME}, data/{LINKS_NAME} and '
             f'data/{KEY_ITEMS_NAME} (Phoenix {commit}). Do not edit.',
             f'// {len(records)} NPCs with something to show of the {len(npcs)} live has, {len(shops)} shops, '
             f'{len(live)} links live has of {len(links)}, {len(key_items)} key items.']
    lines += [f'static_assert(kNpcUnread{kind.capitalize()} == {1 << n}, "NpcUnread must follow gen_npcs.UNREAD");'
              for n, kind in enumerate(UNREAD)]
    lines += [f'static_assert(kLink{kind.capitalize()} == {n}, "NpcLinkKind must follow gen_npcs.LINK_KINDS");'
              for n, kind in enumerate(LINK_KINDS)]
    lines += [f'static_assert(kAct{kind.capitalize()} == {n}, "NpcActKind must follow gen_npcs.ACT_KINDS");'
              for n, kind in enumerate(ACT_KINDS)]
    for pool, kind in POOL_TYPES.items():
        values = pools[pool]
        lines += ['', f'const {kind} {pool}[] = {{']
        lines += ['    ' + ', '.join(f'{v}u' if kind == 'uint32_t' else str(v) for v in values[n:n + VALUES_A_LINE]) +
                  ',' for n in range(0, len(values), VALUES_A_LINE)]
        lines.append('};')
    lines += [f'static_assert(kWhen{kind[0].upper() + kind[1:]} == {n}, "NpcWhen must follow gen_npcs.WHEN_KINDS");'
              for n, kind in enumerate(WHEN_KINDS)]
    for pool, kind in STRUCT_POOLS_TYPES.items():
        lines += ['', f'const {kind} {pool}[] = {{'] + [f'    {v},' for v in pools[pool]] + ['};']
    lines += shop_tables(shops) + link_tables(live) + purpose_table(purposes) + act_lines
    lines += ['', 'const NpcRecord kNpcs[] = {'] + records + ['};', '', 'const KeyItemName kKeyItemNames[] = {']
    lines += [f'    {{{key_item}, {c_string(name)}}},' for key_item, name in key_items.items()] + ['};', '']
    return '\n'.join(lines)


def main(argv) -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    if len(argv) == 3 and argv[1] == '--check':
        data = pathlib.Path(argv[2])
    elif len(argv) == 1:
        data = root / 'data'
    else:
        print('usage: gen_npcs.py [--check DATA_DIR]', file=sys.stderr)
        return 2
    try:
        npcs, shops, links, systems, key_items, acts, texts = load(data)
        text = generate(npcs, shops, links, systems, key_items, read_commit(root / 'data' / META_NAME), acts, texts) \
            if len(argv) == 1 else None
    except (OSError, DataError) as error:
        print(f'gen_npcs: {error}', file=sys.stderr)
        return 1
    if text is None:
        print(f'npcs: {len(npcs)} NPCs, {len(shops)} shops, {len(links)} links, {len(systems)} system calls, '
              f'{len(key_items)} key items, {len(acts)} acts and {len(texts)} texts in {data} pass')
        return 0
    out = root / 'src' / 'generated' / 'npcdata.inc'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding='utf-8')
    print(f'npcs: {len(npcs)} NPCs, {len(key_items)} key items -> {out}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
