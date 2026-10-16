#!/usr/bin/env python3
"""Generate src/generated/mobdata.inc, mob_info.inc, item_magic.inc and mobflags.h from data/phoenix_mobs.tsv,
data/phoenix_drops.tsv, data/phoenix_items.tsv, data/phoenix_mob_info.tsv, data/phoenix_skills.tsv and
data/phoenix_spells.tsv (written by tools/phoenix/refresh.sh), with each distinct set of resistances stored once for the
mobs to point at, and each distinct drop list, run of stats, set of defenses and run of combat stats too.

This file owns the TSVs' format: their columns and ranges, the bits of the flags, detects and immunity columns, the order
of the list columns, the drop kinds, the names of the data files and the meta file's commit key. The Phoenix tools
import it, and the plugin gets the bits from mobflags.h.
"""
import collections
import enum
import pathlib
import re
import sys

TSV_NAME = 'phoenix_mobs.tsv'
DROPS_NAME = 'phoenix_drops.tsv'
ITEMS_NAME = 'phoenix_items.tsv'
INFO_NAME = 'phoenix_mob_info.tsv'
SKILLS_NAME = 'phoenix_skills.tsv'
SPELLS_NAME = 'phoenix_spells.tsv'
META_NAME = 'phoenix_mobs.meta'
META_COMMIT_KEY = 'phoenix_commit'
COLUMNS = ['id', 'zone', 'name', 'minLevel', 'maxLevel', 'flags', 'respawn', 'detects', 'expLevelMod', 'placeholderOf',
           'immunity', 'sdt', 'elementRanks', 'statusRanks', 'elementMeva', 'statusMeva', 'statusResist', 'stats', 'drops']
LIMITS = {'id': (1, 0xFFFFFFFF), 'zone': (0, 0xFFFF), 'minLevel': (0, 255), 'maxLevel': (0, 255), 'flags': (0, 0xFF),
          'respawn': (0, 0xFFFFFFFF), 'detects': (0, 0xFFFF), 'expLevelMod': (-0x8000, 0x7FFF),
          'placeholderOf': (0, 0xFFFFFFFF), 'immunity': (0, 0xFFFFFFFF), 'drops': (0, 0xFFFF)}
# The order of each list column, as headsup_dump.cpp writes it and the plugin reads it (mobflags.h).
SDT_ORDER = ['Slashing', 'Piercing', 'Impact', 'H2H', 'Fire', 'Ice', 'Wind', 'Earth', 'Lightning', 'Water', 'Light', 'Dark']
ELEMENT_ORDER = ['Fire', 'Ice', 'Wind', 'Earth', 'Lightning', 'Water', 'Light', 'Dark']
STATUS_RANK_ORDER = ['Paralyze', 'Bind', 'Silence', 'Slow', 'Poison', 'LightSleep', 'DarkSleep', 'Blind', 'Stun', 'Gravity']
# The stats a mob has at each level: STR to CHR as the magic hit rate reads them, then magic evasion.
STAT_ORDER = ['Str', 'Dex', 'Vit', 'Agi', 'Int', 'Mnd', 'Chr', 'MagicEvasion']
# Comma-separated lists: their length and each value's range, what the plugin stores (int16 SDT, magic evasion and
# resist traits, int8 ranks).
INT16 = (-0x8000, 0x7FFF)
LISTS = {'sdt': (len(SDT_ORDER), *INT16), 'elementRanks': (len(ELEMENT_ORDER), -0x80, 0x7F),
         'statusRanks': (len(STATUS_RANK_ORDER), -0x80, 0x7F), 'elementMeva': (len(ELEMENT_ORDER), *INT16),
         'statusMeva': (len(STATUS_RANK_ORDER), *INT16), 'statusResist': (len(STATUS_RANK_ORDER), *INT16)}
# The stats column: one list of STAT_ORDER a level from minLevel to maxLevel, separated by semicolons; empty when a
# script sets the mob's level.
LEVEL_SEPARATOR = ';'
# phoenix_items.tsv: every item with magic accuracy, an element's or an elemental staff bonus (each in ELEMENT_ORDER),
# or an instrument's skill (Phoenix's xi::SkillType, 0 for any other item).
ITEM_COLUMNS = ['id', 'macc', 'elementMacc', 'staffBonus', 'skill']
ITEM_LIMITS = {'id': (1, 0xFFFF), 'macc': INT16, 'skill': (0, 0xFF)}
ITEM_LISTS = {'elementMacc': (len(ELEMENT_ORDER), *INT16), 'staffBonus': (len(ELEMENT_ORDER), *INT16)}

# phoenix_drops.tsv holds each distinct drop list once, numbered from 1 in the order of the first mob that has it (a
# mob's drops column; 0 is none), one row per entry in the server's order: the ungrouped entries, then each group's.
# group is 0 for an ungrouped entry, else the entry's group in the list, numbered from 1, whose groupRate it shares.
DROP_COLUMNS = ['list', 'kind', 'group', 'groupRate', 'item', 'rate']
# The kinds of entry, in the order of the plugin's DropKind (mobflags.h).
DROP_KINDS = ['drop', 'steal', 'despoil']
PER_MILLE = 1000  # the rate of a drop or group that always drops
DROP_LIMITS = {'list': (1, 0xFFFF), 'group': (0, 0xFF), 'groupRate': (0, PER_MILLE), 'item': (0, 0xFFFF),
               'rate': (0, 0xFFFF)}

# phoenix_mob_info.tsv: a row for each mob of phoenix_mobs.tsv, in its order. jobs: main and support job (Phoenix's
# xi::Job). senses: sight and sound range in yalms. window: the Vana'diel hours it spawns from and until, empty when
# any. lottery: an NM's chance in percent to take its spawn slot's roll after a kill, and the seconds it sits out after
# its own kill; 0,0 for any other mob. traits: Trait bits. addEffect: what its melee hits add, empty for none. absorb:
# the percent chance it absorbs each element (ELEMENT_ORDER). damageTaken: damage of each DAMAGE_ORDER kind is
# multiplied by 1 + value / 10000. skills and spells: its TP move list and spell list, numbered from 1 (0 for none).
# combat: one list of COMBAT_ORDER a level, like phoenix_mobs.tsv's stats.
INFO_COLUMNS = ['id', 'jobs', 'senses', 'window', 'lottery', 'traits', 'addEffect', 'absorb', 'damageTaken', 'skills',
                'spells', 'combat']
INFO_LISTS = {'jobs': (2, 0, 0xFF), 'senses': (2, 0, 0xFF), 'absorb': (len(ELEMENT_ORDER), *INT16)}
INFO_LIMITS = {'id': (1, 0xFFFFFFFF), 'traits': (0, 0xFF), 'skills': (0, 0xFFFF), 'spells': (0, 0xFFFF)}
HOURS = 24
INT32 = (-0x80000000, 0x7FFFFFFF)
DAMAGE_ORDER = ['Physical', 'Magic', 'Ranged', 'Breath']
COMBAT_ORDER = ['Hp', 'Mp', 'Attack', 'Defense', 'Evasion', 'Accuracy']
UNKNOWN_ADD_EFFECT = 'Unknown'  # addEffect of a mob whose melee hits add something its script does not name
# phoenix_skills.tsv: each TP move list, numbered from 1 with its rows together: a move's ID and name.
SKILL_COLUMNS = ['list', 'skill', 'name']
# phoenix_spells.tsv: each spell list the same way: a spell's ID (the game's) and the levels it is cast from and to.
SPELL_COLUMNS = ['list', 'spell', 'minLevel', 'maxLevel']
SPELL_LIMITS = {'list': (1, 0xFFFF), 'spell': (1, 0xFFFF), 'minLevel': (0, 0xFF), 'maxLevel': (0, 0xFF)}


class MobFlag(enum.IntFlag):
    AGGRESSIVE = 1       # m_Aggro
    ALWAYS_AGGRO = 2     # MobMod AlwaysAggro: aggros even when Too Weak
    NO_AGGRO = 4         # MobMod NoAggro
    NOTORIOUS = 8        # MobType Notorious
    CHARMABLE = 16       # MobMod Charmable: a beastmaster can charm it
    LINK = 32            # m_Link: links with its family
    TRUE_DETECTION = 64  # m_TrueDetection: true sight or true sound
    FOLLOWS = 128        # RoamFlag Follow: follows players instead of aggroing


class Detect(enum.IntFlag):
    """Phoenix's MobMod Detection (data/enums/detects.yaml)."""
    SIGHT = 0x001
    HEARING = 0x002
    LOW_HP = 0x004
    MAGIC = 0x020
    ABILITY = 0x040  # job abilities and weapon skills
    SCENT = 0x100


class Trait(enum.IntFlag):
    """What a mob does by itself: Phoenix's mods REGEN, REFRESH, REGAIN, BLINK or UTSUSEMI, and COUNTER."""
    REGEN = 1
    REFRESH = 2
    REGAIN = 4
    SHADOWS = 8
    COUNTER = 16


class Immunity(enum.IntFlag):
    """Phoenix's xi::Immunity (data/enums/immunity.yaml)."""
    ADDLE = 0x00001
    GRAVITY = 0x00002
    BIND = 0x00004
    STUN = 0x00008
    SILENCE = 0x00010
    PARALYZE = 0x00020
    BLIND = 0x00040
    SLOW = 0x00080
    POISON = 0x00100
    ELEGY = 0x00200
    REQUIEM = 0x00400
    LIGHT_SLEEP = 0x00800
    DARK_SLEEP = 0x01000
    ASPIR = 0x02000
    TERROR = 0x04000
    DISPEL = 0x08000
    PETRIFY = 0x10000
    PLAGUE = 0x20000
    CURSE = 0x40000


class DataError(ValueError):
    pass


def whole_numbers(value: str, count: int, lo: int, hi: int, what: str) -> list:
    parts = value.split(',')
    if len(parts) != count or not all(re.fullmatch(r'-?[0-9]+', p) and lo <= int(p) <= hi for p in parts):
        raise DataError(f'{what} must be {count} whole numbers from {lo} to {hi} separated by commas, got {value!r}')
    return [int(p) for p in parts]


def parse(text: str, source: str = TSV_NAME):
    lines = text.splitlines()
    if not lines or lines[0].split('\t') != COLUMNS:
        raise DataError(f'{source}:1: expected the header ' + ' '.join(COLUMNS))
    records, last_id = [], 0
    for number, line in enumerate(lines[1:], start=2):
        fields = line.split('\t')
        if len(fields) != len(COLUMNS):
            raise DataError(f'{source}:{number}: expected {len(COLUMNS)} columns, got {len(fields)}')
        record = {}
        for column, value in zip(COLUMNS, fields):
            if column == 'name':
                if not value:
                    raise DataError(f'{source}:{number}: empty name')
                record[column] = value
                continue
            if column == 'stats':
                record[column] = [whole_numbers(level, len(STAT_ORDER), *INT16, f'{source}:{number}: stats')
                                  for level in value.split(LEVEL_SEPARATOR)] if value else []
                continue
            if column in LISTS:
                record[column] = whole_numbers(value, *LISTS[column], f'{source}:{number}: {column}')
                continue
            lo, hi = LIMITS[column]
            if not re.fullmatch(r'-?[0-9]+', value) or not lo <= int(value) <= hi:
                raise DataError(f'{source}:{number}: {column} must be a whole number from {lo} to {hi}, got {value!r}')
            record[column] = int(value)
        if record['minLevel'] > record['maxLevel']:
            raise DataError(f'{source}:{number}: minLevel is above maxLevel')
        levels = record['maxLevel'] - record['minLevel'] + 1
        if record['stats'] and (record['minLevel'] == 0 or len(record['stats']) != levels):
            raise DataError(f'{source}:{number}: stats must have a list a level from minLevel to maxLevel, '
                            f'{levels}, got {len(record["stats"])}')
        if record['id'] <= last_id:
            raise DataError(f'{source}:{number}: ids must be strictly increasing')
        last_id = record['id']
        records.append(record)
    return records


def parse_drops(text: str, source: str = DROPS_NAME):
    """The drop lists in order, list n at index n - 1, each a list of its entries."""
    lines = text.splitlines()
    if not lines or lines[0].split('\t') != DROP_COLUMNS:
        raise DataError(f'{source}:1: expected the header ' + ' '.join(DROP_COLUMNS))
    lists, groups = [], []
    for number, line in enumerate(lines[1:], start=2):
        fields = line.split('\t')
        if len(fields) != len(DROP_COLUMNS):
            raise DataError(f'{source}:{number}: expected {len(DROP_COLUMNS)} columns, got {len(fields)}')
        entry = {}
        for column, value in zip(DROP_COLUMNS, fields):
            if column == 'kind':
                if value not in DROP_KINDS:
                    raise DataError(f'{source}:{number}: kind must be one of {", ".join(DROP_KINDS)}, got {value!r}')
                entry[column] = value
                continue
            lo, hi = DROP_LIMITS[column]
            if not re.fullmatch(r'[0-9]+', value) or not lo <= int(value) <= hi:
                raise DataError(f'{source}:{number}: {column} must be a whole number from {lo} to {hi}, got {value!r}')
            entry[column] = int(value)
        if entry['list'] == len(lists) + 1:
            lists.append([])
            groups = []
        elif entry['list'] != len(lists):
            raise DataError(f'{source}:{number}: lists must be numbered from 1 with each list\'s rows together')
        group = entry['group']
        if group == 0:
            if entry['groupRate'] != 0:
                raise DataError(f'{source}:{number}: an ungrouped entry must have groupRate 0')
            if entry['kind'] == 'drop' and entry['rate'] > PER_MILLE:
                raise DataError(f'{source}:{number}: a drop\'s rate must be at most {PER_MILLE}')
        else:
            if entry['kind'] != 'drop':
                raise DataError(f'{source}:{number}: only a drop can be in a group')
            if group == len(groups) + 1:
                groups.append(entry['groupRate'])
            elif group != len(groups) or lists[-1][-1]['group'] != group:
                raise DataError(f'{source}:{number}: groups must be numbered from 1 with each group\'s rows together')
            elif entry['groupRate'] != groups[-1]:
                raise DataError(f'{source}:{number}: every entry of group {group} must have its groupRate')
        lists[-1].append(entry)
    return lists


def parse_items(text: str, source: str = ITEMS_NAME):
    """The items in increasing id order, each a dict of ITEM_COLUMNS."""
    lines = text.splitlines()
    if not lines or lines[0].split('\t') != ITEM_COLUMNS:
        raise DataError(f'{source}:1: expected the header ' + ' '.join(ITEM_COLUMNS))
    items, last_id = [], 0
    for number, line in enumerate(lines[1:], start=2):
        fields = line.split('\t')
        if len(fields) != len(ITEM_COLUMNS):
            raise DataError(f'{source}:{number}: expected {len(ITEM_COLUMNS)} columns, got {len(fields)}')
        item = {}
        for column, value in zip(ITEM_COLUMNS, fields):
            if column in ITEM_LISTS:
                item[column] = whole_numbers(value, *ITEM_LISTS[column], f'{source}:{number}: {column}')
                continue
            lo, hi = ITEM_LIMITS[column]
            if not re.fullmatch(r'-?[0-9]+', value) or not lo <= int(value) <= hi:
                raise DataError(f'{source}:{number}: {column} must be a whole number from {lo} to {hi}, got {value!r}')
            item[column] = int(value)
        if item['id'] <= last_id:
            raise DataError(f'{source}:{number}: ids must be strictly increasing')
        last_id = item['id']
        items.append(item)
    return items


def number(value: str, lo: int, hi: int, what: str) -> int:
    if not re.fullmatch(r'-?[0-9]+', value) or not lo <= int(value) <= hi:
        raise DataError(f'{what} must be a whole number from {lo} to {hi}, got {value!r}')
    return int(value)


def table_rows(text: str, columns, source: str):
    """Each row after the header as (line number, {column: text})."""
    lines = text.splitlines()
    if not lines or lines[0].split('\t') != columns:
        raise DataError(f'{source}:1: expected the header ' + ' '.join(columns))
    for line, row in enumerate(lines[1:], start=2):
        fields = row.split('\t')
        if len(fields) != len(columns):
            raise DataError(f'{source}:{line}: expected {len(columns)} columns, got {len(fields)}')
        yield line, dict(zip(columns, fields))


def levels_of(value: str, count: int, lo: int, hi: int, what: str) -> list:
    """A list a level separated by semicolons, or none for an empty value."""
    return [whole_numbers(level, count, lo, hi, what) for level in value.split(LEVEL_SEPARATOR)] if value else []


def parse_info(text: str, source: str = INFO_NAME):
    """A dict of INFO_COLUMNS for each mob, in increasing ID order."""
    info, last_id = [], 0
    for line, row in table_rows(text, INFO_COLUMNS, source):
        at = f'{source}:{line}'
        record = {column: number(row[column], *limits, f'{at}: {column}') for column, limits in INFO_LIMITS.items()}
        for column, spec in INFO_LISTS.items():
            record[column] = whole_numbers(row[column], *spec, f'{at}: {column}')
        record['window'] = whole_numbers(row['window'], 2, 0, HOURS - 1, f'{at}: window') if row['window'] else None
        record['lottery'] = whole_numbers(row['lottery'], 2, 0, 0xFFFFFFFF, f'{at}: lottery')
        if record['lottery'][0] > 100:
            raise DataError(f'{at}: a lottery chance is a percent, got {record["lottery"][0]}')
        record['addEffect'] = row['addEffect']
        record['damageTaken'] = whole_numbers(row['damageTaken'], len(DAMAGE_ORDER), *INT16, f'{at}: damageTaken')
        record['combat'] = levels_of(row['combat'], len(COMBAT_ORDER), *INT32, f'{at}: combat')
        if record['id'] <= last_id:
            raise DataError(f'{at}: ids must be strictly increasing')
        last_id = record['id']
        info.append(record)
    return info


def numbered_lists(rows, source: str) -> list:
    """Rows whose list column numbers them from 1 with each list's rows together, as a list of lists."""
    lists = []
    for line, entry in rows:
        if entry['list'] == len(lists) + 1:
            lists.append([])
        elif entry['list'] != len(lists):
            raise DataError(f'{source}:{line}: lists must be numbered from 1 with each list\'s rows together')
        lists[-1].append(entry)
    return lists


def parse_skills(text: str, source: str = SKILLS_NAME):
    """The TP move lists in order, list n at index n - 1, each a list of {list, skill, name}."""
    def rows():
        for line, row in table_rows(text, SKILL_COLUMNS, source):
            if not row['name']:
                raise DataError(f'{source}:{line}: empty name')
            yield line, {'list': number(row['list'], 1, 0xFFFF, f'{source}:{line}: list'),
                         'skill': number(row['skill'], 1, 0xFFFF, f'{source}:{line}: skill'), 'name': row['name']}
    return numbered_lists(rows(), source)


def parse_spells(text: str, source: str = SPELLS_NAME):
    """The spell lists in order, list n at index n - 1, each a list of {list, spell, minLevel, maxLevel}."""
    def rows():
        for line, row in table_rows(text, SPELL_COLUMNS, source):
            yield line, {column: number(row[column], *limits, f'{source}:{line}: {column}')
                         for column, limits in SPELL_LIMITS.items()}
    return numbered_lists(rows(), source)


def group_weights(entries) -> list:
    """Each group's summed rates: the total one roll of the group picks among."""
    weights = []
    for entry in entries:
        if entry['group'] > len(weights):
            weights.append(0)
        if entry['group']:
            weights[entry['group'] - 1] += entry['rate']
    return weights


def c_string(text: str) -> str:
    out = []
    for byte in text.encode('utf-8'):
        char = chr(byte)
        out.append(char if 0x20 <= byte < 0x7F and char not in '"\\' else f'\\{byte:03o}')
    return '"' + ''.join(out) + '"'


RESISTANCE_LISTS = ['sdt', 'elementRanks', 'statusRanks', 'elementMeva', 'statusMeva', 'statusResist']
NEUTRAL = (0,) + tuple((0,) * LISTS[column][0] for column in RESISTANCE_LISTS)


def resistances_key(r):
    return (r['immunity'],) + tuple(tuple(r[column]) for column in RESISTANCE_LISTS)


def c_list(values) -> str:
    return '{' + ', '.join(str(v) for v in values) + '}'


def c_resistances(key) -> str:
    immunity, *lists = key
    return f'    {{{immunity}u, ' + ', '.join(c_list(values) for values in lists) + '},'


def c_record(r, resistances: int, placeholders: int, placeholder_respawn: int, stats: int) -> str:
    return (f"    {{{r['id']}u, {c_string(r['name'])}, {r['minLevel']}, {r['maxLevel']}, {r['flags']}, {r['respawn']}u, "
            f"{r['detects']}, {r['expLevelMod']}, {r['placeholderOf']}u, {placeholders}, {resistances}, {r['drops']}, "
            f"{placeholder_respawn}u, {stats}}},")


def run_tables(levels, struct: str, table: str, runs_table: str, field: str):
    """The table of struct, each distinct run of levels' lists once, a run to a line; runs_table, where each run
    starts, after the none at 0 a mob without any reads; and each record's run. levels holds a record's lists."""
    runs, blocks, starts, indices, first = {(): 0}, [], ['    0u,'], [], 0
    for record_levels in levels:
        key = tuple(tuple(level) for level in record_levels)
        if key not in runs:
            runs[key] = len(runs)
            starts.append(f'    {first}u,')
            blocks.append('    ' + ' '.join(f'{{{c_list(level)}}},' for level in key))
            first += len(key)
        indices.append(runs[key])
    if len(runs) > 0xFFFF:
        raise DataError(f'{len(runs)} distinct runs of {field.split("::")[1]} do not fit {field}')
    return ([f'const {struct} {table}[] = {{'] + blocks + ['};', '', f'const uint32_t {runs_table}[] = {{'] + starts +
            ['};', ''], indices, len(runs))


def stat_tables(records):
    """kMobStats and kMobStatRuns (run_tables) for each record's stats."""
    return run_tables((r['stats'] for r in records), 'MobStats', 'kMobStats', 'kMobStatRuns', 'MobRecord::stats')


def c_item(item) -> str:
    return (f"    {{{item['id']}, {item['macc']}, {c_list(item['elementMacc'])}, {c_list(item['staffBonus'])}, "
            f"{item['skill']}}},")


def generate_items(items, commit: str) -> str:
    if not items:
        raise DataError(f'{ITEMS_NAME} has no items: the dump read none')
    return '\n'.join([f'// Generated by tools/gen_mobdata.py from data/{ITEMS_NAME} (Phoenix {commit}). Do not edit.',
                      f'// {len(items)} items, sorted by ID.', 'const ItemMagic kItemMagic[] = {'] +
                     [c_item(item) for item in items] + ['};', ''])


def c_drop(entry) -> str:
    return f"{{{entry['item']}, {entry['rate']}, DropKind::{entry['kind'].capitalize()}, {entry['group']}}}"


def drop_tables(lists) -> list:
    """kMobDrops, every list's entries one list to a line; kMobDropGroups, each list's groups after the none at 0; and
    kMobDropLists, where each list's entries and groups start, after the empty list at 0 that a mob without loot reads."""
    drops, groups, spans = [], ['    {0, 0},'], ['    {0u, 0, 0, 0},']
    first = 0
    for entries in lists:
        weights = group_weights(entries)
        if any(weight > 0xFFFF for weight in weights):
            raise DataError(f'a group of drop list {len(spans)} weighs more than MobDropGroup::weight holds')
        rates = {entry['group']: entry['groupRate'] for entry in entries if entry['group']}
        spans.append(f'    {{{first}u, {len(entries)}, {len(groups)}, {len(weights)}}},')
        drops.append('    ' + ', '.join(c_drop(entry) for entry in entries) + ',')
        groups += [f'    {{{rates[number]}, {weight}}},' for number, weight in enumerate(weights, start=1)]
        first += len(entries)
    if not drops:
        raise DataError('no drop lists: the dump read no loot')
    if len(groups) > 0xFFFF or any(len(entries) > 0xFFFF for entries in lists):
        raise DataError('the drop lists do not fit MobDropList')
    return (['const MobDrop kMobDrops[] = {'] + drops + ['};', '', 'const MobDropGroup kMobDropGroups[] = {'] + groups +
            ['};', '', 'const MobDropList kMobDropLists[] = {'] + spans + ['};', ''])


def generate(records, lists, commit: str) -> str:
    # Most mobs share their family's values, so each distinct set is stored once; 0 is the neutral one.
    table = {NEUTRAL: 0}
    indices = [table.setdefault(resistances_key(r), len(table)) for r in records]
    if len(table) > 0xFFFF:
        raise DataError(f'{len(table)} distinct resistances do not fit MobRecord::resistances')
    placeholders = collections.Counter(r['placeholderOf'] for r in records if r['placeholderOf'] != 0)
    for nm, count in placeholders.items():
        if count > 0xFF:
            raise DataError(f'{nm} has {count} placeholders, more than MobRecord::placeholders holds')
    # An NM's placeholders share one respawn but in a few odd lots: the shortest is the time not to miss.
    returns = {}
    for r in records:
        if r['placeholderOf'] != 0 and r['respawn'] > 0:
            returns[r['placeholderOf']] = min(returns.get(r['placeholderOf'], r['respawn']), r['respawn'])
    for r in records:
        if r['drops'] > len(lists):
            raise DataError(f"mob {r['id']} has drop list {r['drops']}, past the {len(lists)} in {DROPS_NAME}")
    stats_lines, stats, runs = stat_tables(records)
    lines = [f'// Generated by tools/gen_mobdata.py from data/{TSV_NAME} and data/{DROPS_NAME} (Phoenix {commit}). '
             'Do not edit.',
             f'// {len(records)} mobs, sorted by server ID, {len(table)} distinct resistances, {len(lists)} distinct '
             f'drop lists and {runs - 1} distinct runs of stats.']
    lines += drop_tables(lists)
    lines += stats_lines
    lines += ['const MobResistances kMobResistances[] = {']
    lines += [c_resistances(key) for key in table]
    lines += ['};', '', 'const MobRecord kMobs[] = {']
    lines += [c_record(r, index, placeholders[r['id']], returns.get(r['id'], 0), run)
              for r, index, run in zip(records, indices, stats)]
    lines += ['};', '']
    return '\n'.join(lines)


NO_WINDOW = 0xFF  # MobInfo::windowStart and windowEnd of a mob that spawns at any hour
INDICES_A_LINE = 32
NEUTRAL_DEFENSES = (0,) * (len(ELEMENT_ORDER) + len(DAMAGE_ORDER))


def spans(groups) -> tuple:
    """Every group's entries one group to a line, and where each group starts, after the empty one at 0."""
    entries, starts, first = [], ['    {0u, 0},'], 0
    for group in groups:
        starts.append(f'    {{{first}u, {len(group)}}},')
        entries.append('    ' + ' '.join(group))
        first += len(group)
    if len(starts) > 0xFFFF or any(len(group) > 0xFFFF for group in groups):
        raise DataError('the lists do not fit MobSpan')
    return entries, starts


def check_info(records, info, skills, spells):
    if [r['id'] for r in records] != [i['id'] for i in info]:
        raise DataError(f'{INFO_NAME} must have a row for each mob of {TSV_NAME}, in its order')
    for r, i in zip(records, info):
        levels = r['maxLevel'] - r['minLevel'] + 1
        if i['combat'] and (r['minLevel'] == 0 or len(i['combat']) != levels):
            raise DataError(f"mob {r['id']}: combat must have a list a level from minLevel to maxLevel, {levels}, "
                            f"got {len(i['combat'])}")
        if i['skills'] > len(skills) or i['spells'] > len(spells):
            raise DataError(f"mob {r['id']} has TP move list {i['skills']} and spell list {i['spells']}, past the "
                            f"{len(skills)} in {SKILLS_NAME} or the {len(spells)} in {SPELLS_NAME}")


def generate_info(records, info, skills, spells, commit: str) -> str:
    """mob_info.inc: kMobInfo, each distinct row once, kMobInfoOf beside kMobs, each mob's row, and the tables the rows
    point into."""
    check_info(records, info, skills, spells)
    defenses = {NEUTRAL_DEFENSES: 0}
    defense_indices = [defenses.setdefault(tuple(i['absorb']) + tuple(i['damageTaken']), len(defenses)) for i in info]
    if len(defenses) > 0xFFFF:
        raise DataError(f'{len(defenses)} distinct defenses do not fit MobInfo::defenses')
    combat_lines, combat, runs = run_tables((i['combat'] for i in info), 'MobCombat', 'kMobCombat', 'kMobCombatRuns',
                                            'MobInfo::combat')
    skill_entries, skill_starts = spans([f"{{{e['skill']}, {c_string(e['name'])}}}," for e in s] for s in skills)
    spell_entries, spell_starts = spans([f"{{{e['spell']}, {e['minLevel']}, {e['maxLevel']}}}," for e in s] for s in spells)
    lines = [f'// Generated by tools/gen_mobdata.py from data/{INFO_NAME}, data/{SKILLS_NAME} and data/{SPELLS_NAME} '
             f'(Phoenix {commit}). Do not edit.',
             f'// {len(info)} mobs, {len(defenses)} distinct defenses, {runs - 1} distinct runs of combat stats, '
             f'{len(skills)} TP move lists and {len(spells)} spell lists.']
    lines += ['const MobSkill kMobSkills[] = {'] + skill_entries + ['};', '', 'const MobSpan kMobSkillLists[] = {']
    lines += skill_starts + ['};', '', 'const MobSpell kMobSpells[] = {'] + spell_entries + ['};', '']
    lines += ['const MobSpan kMobSpellLists[] = {'] + spell_starts + ['};', ''] + combat_lines
    lines += ['const MobDefenses kMobDefenses[] = {']
    lines += [f'    {{{c_list(key[:len(ELEMENT_ORDER)])}, {c_list(key[len(ELEMENT_ORDER):])}}},' for key in defenses]
    rows, row_indices = {}, []
    for i, defense, run in zip(info, defense_indices, combat):
        start, end = i['window'] or (NO_WINDOW, NO_WINDOW)
        row = (f"    {{{i['jobs'][0]}, {i['jobs'][1]}, {i['senses'][0]}, {i['senses'][1]}, {start}, {end}, "
               f"{i['lottery'][0]}, {i['lottery'][1]}u, {i['traits']}, {c_string(i['addEffect'])}, {defense}, {run}, "
               f"{i['skills']}, {i['spells']}}},")
        row_indices.append(rows.setdefault(row, len(rows)))
    if len(rows) > 0xFFFF:
        raise DataError(f'{len(rows)} distinct rows of mob info do not fit kMobInfoOf')
    lines += ['};', '', 'const MobInfo kMobInfo[] = {'] + list(rows) + ['};', '', 'const uint16_t kMobInfoOf[] = {']
    lines += ['    ' + ', '.join(str(index) for index in row_indices[n:n + INDICES_A_LINE]) + ','
              for n in range(0, len(row_indices), INDICES_A_LINE)]
    lines += ['};', '']
    return '\n'.join(lines)


def cpp_enum(name: str, prefix: str, underlying: str, flags) -> list:
    def words(member):
        return ''.join(part.capitalize() for part in member.name.split('_'))
    return [f'    enum {name} : {underlying}', '    {'] + [f'        {prefix}{words(m)} = {m.value},' for m in flags] + ['    };']


def cpp_order(name: str, members) -> list:
    return ([f'    enum class {name} : uint8_t', '    {'] + [f'        {m},' for m in members] +
            ['    };', f'    constexpr int k{name}Count = {len(members)};'])


def generate_flags() -> str:
    lines = ['// Generated by tools/gen_mobdata.py. Do not edit.', '#pragma once', '', '#include <cstdint>', '',
             'namespace headsup', '{', '    // Bits of MobRecord::flags.']
    lines += cpp_enum('MobFlag', 'kMob', 'uint8_t', MobFlag)
    lines += ['', "    // Bits of MobRecord::detects: Phoenix's MobMod Detection."]
    lines += cpp_enum('MobDetect', 'kDetect', 'uint16_t', Detect)
    lines += ['', "    // Bits of MobResistances::immunity: Phoenix's xi::Immunity."]
    lines += cpp_enum('MobImmunity', 'kImmune', 'uint32_t', Immunity)
    lines += ['', '    // Bits of MobInfo::traits.']
    lines += cpp_enum('MobTrait', 'kTrait', 'uint8_t', Trait)
    lines += ['', '    // The order of MobResistances::sdt, its element lists, its status lists and MobStats::value.']
    lines += (cpp_order('Sdt', SDT_ORDER) + cpp_order('Element', ELEMENT_ORDER) + cpp_order('StatusRank', STATUS_RANK_ORDER)
              + cpp_order('Stat', STAT_ORDER))
    lines += ['', '    // MobDrop::kind.'] + cpp_order('DropKind', [kind.capitalize() for kind in DROP_KINDS])
    lines += ['', '    // The order of MobDefenses::damageTaken and MobCombat::value.']
    lines += cpp_order('DamageKind', DAMAGE_ORDER) + cpp_order('Combat', COMBAT_ORDER)
    lines += [f'    constexpr uint8_t kNoWindow = {NO_WINDOW};',
              f'    constexpr const char* kUnknownAddEffect = {c_string(UNKNOWN_ADD_EFFECT)};']
    lines += ['}', '']
    return '\n'.join(lines)


def read_commit(meta: pathlib.Path) -> str:
    for line in meta.read_text(encoding='utf-8').splitlines():
        key, _, value = line.partition('\t')
        if key == META_COMMIT_KEY:
            return value
    raise DataError(f'{meta}: no {META_COMMIT_KEY} line')


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    generated = root / 'src' / 'generated'
    try:
        records = parse((root / 'data' / TSV_NAME).read_text(encoding='utf-8'), str(root / 'data' / TSV_NAME))
        lists = parse_drops((root / 'data' / DROPS_NAME).read_text(encoding='utf-8'), str(root / 'data' / DROPS_NAME))
        items = parse_items((root / 'data' / ITEMS_NAME).read_text(encoding='utf-8'), str(root / 'data' / ITEMS_NAME))
        tables = [parse((root / 'data' / name).read_text(encoding='utf-8'), str(root / 'data' / name))
                  for parse, name in ((parse_info, INFO_NAME), (parse_skills, SKILLS_NAME),
                                      (parse_spells, SPELLS_NAME))]
        commit = read_commit(root / 'data' / META_NAME)
        text, items_text = generate(records, lists, commit), generate_items(items, commit)
        info_text = generate_info(records, *tables, commit)
    except DataError as error:
        print(f'gen_mobdata: {error}', file=sys.stderr)
        return 1
    generated.mkdir(parents=True, exist_ok=True)
    (generated / 'mobdata.inc').write_text(text, encoding='utf-8')
    (generated / 'item_magic.inc').write_text(items_text, encoding='utf-8')
    (generated / 'mob_info.inc').write_text(info_text, encoding='utf-8')
    (generated / 'mobflags.h').write_text(generate_flags(), encoding='utf-8')
    print(f'mobdata: {len(records)} mobs, {len(lists)} drop lists, {len(items)} items -> {generated}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
