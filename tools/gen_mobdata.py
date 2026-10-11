#!/usr/bin/env python3
"""Generate src/generated/mobdata.inc and mobflags.h from data/phoenix_mobs.tsv and data/phoenix_drops.tsv (written by
tools/phoenix/refresh.sh), with each distinct set of resistances stored once for the mobs to point at, and each distinct
drop list too.

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
META_NAME = 'phoenix_mobs.meta'
META_COMMIT_KEY = 'phoenix_commit'
COLUMNS = ['id', 'zone', 'name', 'minLevel', 'maxLevel', 'flags', 'respawn', 'detects', 'expLevelMod', 'placeholderOf',
           'immunity', 'sdt', 'elementRanks', 'statusRanks', 'drops']
LIMITS = {'id': (1, 0xFFFFFFFF), 'zone': (0, 0xFFFF), 'minLevel': (0, 255), 'maxLevel': (0, 255), 'flags': (0, 0xFF),
          'respawn': (0, 0xFFFFFFFF), 'detects': (0, 0xFFFF), 'expLevelMod': (-0x8000, 0x7FFF),
          'placeholderOf': (0, 0xFFFFFFFF), 'immunity': (0, 0xFFFFFFFF), 'drops': (0, 0xFFFF)}
# The order of each list column, as headsup_dump.cpp writes it and the plugin reads it (mobflags.h).
SDT_ORDER = ['Slashing', 'Piercing', 'Impact', 'H2H', 'Fire', 'Ice', 'Wind', 'Earth', 'Lightning', 'Water', 'Light', 'Dark']
ELEMENT_ORDER = ['Fire', 'Ice', 'Wind', 'Earth', 'Lightning', 'Water', 'Light', 'Dark']
STATUS_RANK_ORDER = ['Paralyze', 'Bind', 'Silence', 'Slow', 'Poison', 'LightSleep', 'DarkSleep', 'Blind', 'Stun', 'Gravity']
# Comma-separated lists: their length and each value's range, what the plugin stores (int16 SDT, int8 ranks).
LISTS = {'sdt': (len(SDT_ORDER), -0x8000, 0x7FFF), 'elementRanks': (len(ELEMENT_ORDER), -0x80, 0x7F),
         'statusRanks': (len(STATUS_RANK_ORDER), -0x80, 0x7F)}

# phoenix_drops.tsv holds each distinct drop list once, numbered from 1 in the order of the first mob that has it (a
# mob's drops column; 0 is none), one row per entry in the server's order: the ungrouped entries, then each group's.
# group is 0 for an ungrouped entry, else the entry's group in the list, numbered from 1, whose groupRate it shares.
DROP_COLUMNS = ['list', 'kind', 'group', 'groupRate', 'item', 'rate']
# The kinds of entry, in the order of the plugin's DropKind (mobflags.h).
DROP_KINDS = ['drop', 'steal', 'despoil']
PER_MILLE = 1000  # the rate of a drop or group that always drops
DROP_LIMITS = {'list': (1, 0xFFFF), 'group': (0, 0xFF), 'groupRate': (0, PER_MILLE), 'item': (0, 0xFFFF),
               'rate': (0, 0xFFFF)}


class MobFlag(enum.IntFlag):
    AGGRESSIVE = 1       # m_Aggro
    ALWAYS_AGGRO = 2     # MobMod AlwaysAggro: aggros even when Too Weak
    NO_AGGRO = 4         # MobMod NoAggro
    NOTORIOUS = 8        # MobType Notorious
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
            if column in LISTS:
                count, lo, hi = LISTS[column]
                parts = value.split(',')
                if len(parts) != count or not all(re.fullmatch(r'-?[0-9]+', p) and lo <= int(p) <= hi for p in parts):
                    raise DataError(f'{source}:{number}: {column} must be {count} whole numbers from {lo} to {hi} '
                                    f'separated by commas, got {value!r}')
                record[column] = [int(p) for p in parts]
                continue
            lo, hi = LIMITS[column]
            if not re.fullmatch(r'-?[0-9]+', value) or not lo <= int(value) <= hi:
                raise DataError(f'{source}:{number}: {column} must be a whole number from {lo} to {hi}, got {value!r}')
            record[column] = int(value)
        if record['minLevel'] > record['maxLevel']:
            raise DataError(f'{source}:{number}: minLevel is above maxLevel')
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


NEUTRAL = (0, (0,) * len(SDT_ORDER), (0,) * len(ELEMENT_ORDER), (0,) * len(STATUS_RANK_ORDER))


def resistances_key(r):
    return r['immunity'], tuple(r['sdt']), tuple(r['elementRanks']), tuple(r['statusRanks'])


def c_list(values) -> str:
    return '{' + ', '.join(str(v) for v in values) + '}'


def c_resistances(key) -> str:
    immunity, sdt, elements, statuses = key
    return f'    {{{immunity}u, {c_list(sdt)}, {c_list(elements)}, {c_list(statuses)}}},'


def c_record(r, resistances: int, placeholders: int) -> str:
    return (f"    {{{r['id']}u, {c_string(r['name'])}, {r['minLevel']}, {r['maxLevel']}, {r['flags']}, {r['respawn']}u, "
            f"{r['detects']}, {r['expLevelMod']}, {r['placeholderOf']}u, {placeholders}, {resistances}, {r['drops']}}},")


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
    for r in records:
        if r['drops'] > len(lists):
            raise DataError(f"mob {r['id']} has drop list {r['drops']}, past the {len(lists)} in {DROPS_NAME}")
    lines = [f'// Generated by tools/gen_mobdata.py from data/{TSV_NAME} and data/{DROPS_NAME} (Phoenix {commit}). '
             'Do not edit.',
             f'// {len(records)} mobs, sorted by server ID, {len(table)} distinct resistances and {len(lists)} distinct '
             'drop lists.']
    lines += drop_tables(lists)
    lines += ['const MobResistances kMobResistances[] = {']
    lines += [c_resistances(key) for key in table]
    lines += ['};', '', 'const MobRecord kMobs[] = {']
    lines += [c_record(r, index, placeholders[r['id']]) for r, index in zip(records, indices)]
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
    lines += ['', '    // The order of MobResistances::sdt, ::elementRanks and ::statusRanks.']
    lines += cpp_order('Sdt', SDT_ORDER) + cpp_order('Element', ELEMENT_ORDER) + cpp_order('StatusRank', STATUS_RANK_ORDER)
    lines += ['', '    // MobDrop::kind.'] + cpp_order('DropKind', [kind.capitalize() for kind in DROP_KINDS])
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
        text = generate(records, lists, read_commit(root / 'data' / META_NAME))
    except DataError as error:
        print(f'gen_mobdata: {error}', file=sys.stderr)
        return 1
    generated.mkdir(parents=True, exist_ok=True)
    (generated / 'mobdata.inc').write_text(text, encoding='utf-8')
    (generated / 'mobflags.h').write_text(generate_flags(), encoding='utf-8')
    print(f'mobdata: {len(records)} mobs, {len(lists)} drop lists -> {generated}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
