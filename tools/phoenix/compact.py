#!/usr/bin/env python3
"""Turn one or more headsup_dump JSON files of the same server into phoenix_mobs.tsv, phoenix_drops.tsv,
phoenix_items.tsv, phoenix_mob_info.tsv, phoenix_skills.tsv, phoenix_spells.tsv, phoenix_regimes.tsv,
phoenix_mobs.meta and phoenix_rules.json. Each mob's script in the server's checkout says which training regimes it
counts for and what its melee hits add.

Usage: compact.py LIVE_SETTINGS SERVER_DIR DUMP_JSON [DUMP_JSON ...] OUT_DIR PHOENIX_COMMIT

Some mobs change aggression while the server runs (elementals, the Ghrah forms), so one dump is one snapshot. With
several dumps a mob counts as aggressive if any of them saw it attack, so an aggressive mob is never painted safe.
Every dump must carry the same rules and items, and the server must have run with every value LIVE_SETTINGS (refresh.sh's
live_settings.lua) sets. A mob's drop list comes from the first dump that has the mob, like the rest of its
record, and compact names any mob whose list differs in a later dump.
"""
import datetime
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import gen_mobdata as schema  # noqa: E402
import gen_rules  # noqa: E402

MobFlag = schema.MobFlag
MOBTYPE_NOTORIOUS = 0x02  # Phoenix data/enums/mob_type.yaml
MOB_KEYS = {'id', 'zone', 'name', 'minLevel', 'maxLevel', 'respawn', 'aggro', 'alwaysAggro', 'noAggro', 'type', 'link',
            'detects', 'trueDetection', 'expLevelMod', 'follows', 'placeholderOf', 'immunity', 'sdt', 'elementRanks',
            'statusRanks', 'elementMeva', 'statusMeva', 'statusResist', 'stats', 'script', 'zoneName', 'jobs',
            'charmable', 'senses', 'addEffect', 'skillList', 'damageMods', 'absorb', 'traits'}
# headsup_dump.cpp's kDamageMods and kTraitMods, in its order.
DAMAGE_MODS = ['udmgphys', 'dmgphys', 'udmgmagic', 'dmgmagic', 'dmgmagic_ii', 'udmgrange', 'dmgrange', 'udmgbreath',
               'dmgbreath', 'dmg']
TRAIT_MODS = ['regen', 'refresh', 'regain', 'blink', 'utsusemi', 'counter', 'spikes', 'enspell']
PER_TEN_THOUSAND = 10000
REGIME_TYPE_NAMES = {'1': 'fields', '2': 'grounds'}  # scripts/globals/regimes.lua's xi.regime.type
# A kill counts for the regime by its ID alone: the type a script passes only picks the alliance rule.
CHECK_REGIME = re.compile(r'xi\.regime\.checkRegime\(\s*player\s*,\s*mob\s*,\s*(\d+)\s*,\s*(\d+)\s*,')
# What a script's onAdditionalEffect adds, in the order looked for: one of xi.mob.ae, a status effect, an element's damage.
ADD_EFFECTS = (re.compile(r'xi\.mob\.ae\.([A-Z_]+)'), re.compile(r'effectId\s*=\s*xi\.effect\.([A-Z_]+)'),
               re.compile(r'magicalElement\s*=\s*xi\.element\.([A-Z]+)'))
UPPER_WORDS = {'hp', 'mp', 'tp', 'ii', 'iii', 'iv', 'v', 'vi'}
LOWER_WORDS = {'of', 'the', 'and', 'a', 'an', 'in', 'on', 'to', 'for', 'with', 'from'}
EFFECT_NAMES = {'ENAMNESIA': 'Amnesia'}  # xi.mob.ae's name for what the game calls Amnesia
AGGRO_FIELDS = ('aggro', 'alwaysAggro', 'noAggro')
# Phoenix's DROP_TYPE (src/map/utils/itemutils.h): a grouped entry only appears inside a group, and is a drop.
DROP_KIND_OF_TYPE = {0x00: 'drop', 0x02: 'steal', 0x04: 'despoil'}
DROP_GROUPED = 0x01
DIFFERING_SHOWN = 10  # mob IDs named when drop lists differ between dumps
# live_settings.lua's lines: a key of one of xi.settings' tables set to a number or a boolean.
SETTING = re.compile(r'xi\.settings\.(\w+)\.(\w+)\s*=\s*(true|false|-?[0-9]+(?:\.[0-9]+)?)')


def flags(mob) -> int:
    value = MobFlag(0)
    if mob['aggro']:
        value |= MobFlag.AGGRESSIVE
    if mob['alwaysAggro'] > 0:
        value |= MobFlag.ALWAYS_AGGRO
    if mob['noAggro'] > 0:
        value |= MobFlag.NO_AGGRO
    if mob['type'] & MOBTYPE_NOTORIOUS:
        value |= MobFlag.NOTORIOUS
    if mob['link']:
        value |= MobFlag.LINK
    if mob['trueDetection']:
        value |= MobFlag.TRUE_DETECTION
    if mob['follows']:
        value |= MobFlag.FOLLOWS
    if mob['charmable'] > 0:
        value |= MobFlag.CHARMABLE
    return int(value)


def title(name: str) -> str:
    """'gates_of_hades' as the game writes it, 'Gates of Hades'; HP, MP, TP and roman numerals in capitals."""
    words = [w for w in name.lower().replace('_', ' ').split(' ') if w]
    def word(w, first):
        if w in UPPER_WORDS:
            return w.upper()
        if w in LOWER_WORDS and not first:
            return w
        return '-'.join(part.capitalize() for part in w.split('-'))
    return ' '.join(word(w, i == 0) for i, w in enumerate(words))


def damage_taken(mods) -> list:
    """Each kind's damage multiplier less 1, in ten-thousandths, as Phoenix's battleutils works it out from the mods."""
    m = dict(zip(DAMAGE_MODS, mods))
    def ratio(mod):
        return m[mod] / PER_TEN_THOUSAND
    def kind(uncapped, capped, low=0.5, high=float('inf')):
        return max(1 + ratio(uncapped), 0.0) * min(max(1 + ratio(capped) + ratio('dmg'), low), high)
    physical, ranged = kind('udmgphys', 'dmgphys'), kind('udmgrange', 'dmgrange')
    breath = kind('udmgbreath', 'dmgbreath', high=1.5)
    magic = max(1 + ratio('udmgmagic'), 0.0) * max(max(1 + ratio('dmgmagic') + ratio('dmg'), 0.5) + ratio('dmgmagic_ii'),
                                                   0.125)
    return [round((value - 1) * PER_TEN_THOUSAND) for value in (physical, magic, ranged, breath)]


def traits(values) -> int:
    v = dict(zip(TRAIT_MODS, values))
    bits = schema.Trait(0)
    for trait, on in ((schema.Trait.REGEN, v['regen']), (schema.Trait.REFRESH, v['refresh']),
                      (schema.Trait.REGAIN, v['regain']), (schema.Trait.SHADOWS, v['blink'] or v['utsusemi']),
                      (schema.Trait.COUNTER, v['counter'])):
        if on > 0:
            bits |= trait
    return int(bits)


class Scripts:
    """What each mob script of a server checkout says: the regimes it counts for and what its melee hits add."""

    def __init__(self, server):
        self.server, self.facts = pathlib.Path(server), {}

    def of(self, zone_name, script):
        key = (zone_name, script)
        if key not in self.facts:
            path = self.server / 'scripts' / 'zones' / zone_name / 'mobs' / f'{script}.lua'
            text = path.read_text(encoding='utf-8', errors='replace') if path.is_file() else ''
            regimes = [(int(regime), int(target)) for regime, target in CHECK_REGIME.findall(text)]
            effect = None
            body = text.split('onAdditionalEffect', 1)[1] if 'onAdditionalEffect' in text else ''
            for number, pattern in enumerate(ADD_EFFECTS):
                found = pattern.search(body)
                if found:
                    name = EFFECT_NAMES.get(found.group(1), title(found.group(1)))
                    effect = name + ' Damage' if number == len(ADD_EFFECTS) - 1 else name
                    break
            self.facts[key] = regimes, effect
        return self.facts[key]


def display_name(name: str) -> str:
    """The client shows packet names with spaces instead of underscores."""
    if any(c in name for c in '\t\r\n'):
        raise schema.DataError(f'mob name contains a tab or newline: {name!r}')
    return name.replace('_', ' ')


def attacks(mob) -> bool:
    """The part of the plugin's rule (src/mobdata.cpp) that changes while the server runs: aggressive or always-aggro,
    and not no-aggro."""
    return (bool(mob['aggro']) or mob['alwaysAggro'] > 0) and not mob['noAggro'] > 0


def merge(snapshots, differing=None):
    """One record per mob ID. The level range spans every snapshot, the stats of each level come from the first
    snapshot with that level (none at all unless every level of the range has some), the aggro fields come from the
    first snapshot in which the mob attacks, as dumped or as the dump saw it at some moment of its run (seenAttacking),
    and the rest comes from the first snapshot that has the mob. The IDs of mobs whose loot differs in a later snapshot
    go into differing."""
    merged, levels = {}, {}
    for snapshot in snapshots:
        for mob in snapshot:
            known = levels.setdefault(mob['id'], {})
            for level, stats, combat in zip(range(mob['minLevel'], mob['maxLevel'] + 1), mob['stats'], mob['combat']):
                known.setdefault(level, (stats, combat))
            first = merged.get(mob['id'])
            if first is None:
                first = merged[mob['id']] = dict(mob)
            else:
                first['minLevel'] = min(first['minLevel'], mob['minLevel'])
                first['maxLevel'] = max(first['maxLevel'], mob['maxLevel'])
                if differing is not None and first['loot'] != mob['loot']:
                    differing.add(mob['id'])
            # The dumped state first: seenAttacking can be from before the spawn script set AlwaysAggro (Lioumere).
            for sample in (mob, mob.get('seenAttacking')):
                if sample is not None and not attacks(first) and attacks(sample):
                    for field in AGGRO_FIELDS:
                        first[field] = sample[field]
    for mob_id, mob in merged.items():
        span = range(mob['minLevel'], mob['maxLevel'] + 1)
        whole = all(level in levels[mob_id] for level in span)
        mob['stats'] = [levels[mob_id][level][0] for level in span] if whole else []
        mob['combat'] = [levels[mob_id][level][1] for level in span] if whole else []
    return list(merged.values())


def drop_entry(raw, group, group_rate, source):
    """One entry as (kind, group, groupRate, item, rate), the columns of phoenix_drops.tsv after list."""
    try:
        drop_type, item, rate, fixed = raw
    except (TypeError, ValueError) as error:
        raise schema.DataError(f'{source}: {raw!r} is not a drop list entry') from error
    if group:
        kind = 'drop' if drop_type == DROP_GROUPED else None
    else:
        kind = DROP_KIND_OF_TYPE.get(drop_type)
    if kind is None:
        raise schema.DataError(f'{source}: drop type {drop_type!r} {"inside" if group else "outside"} a group')
    # Every list the server loads leaves Treasure Hunter on; only a script's own rolls fix their rates.
    if fixed:
        raise schema.DataError(f'{source}: item {item} has a fixed rate, which phoenix_drops.tsv cannot say')
    return kind, group, group_rate, item, rate


def drop_list(raw, source):
    """A dumped drop list as a tuple of entries: the ungrouped ones, then each group's, so equal lists compare equal."""
    entries = [drop_entry(item, 0, 0, source) for item in raw['items']]
    for number, group in enumerate(raw['groups'], start=1):
        if group['fixed']:
            raise schema.DataError(f'{source}: group {number} has a fixed rate, which phoenix_drops.tsv cannot say')
        entries += [drop_entry(item, number, group['rate'], source) for item in group['items']]
    return tuple(entries)


def drops_text(mobs) -> str:
    """phoenix_drops.tsv, each distinct list numbered in the order of the first mob that has it, and each mob's drops
    column set to its list's number, 0 when it has none."""
    numbers = {}
    for mob in mobs:
        mob['drops'] = numbers.setdefault(mob['loot'], len(numbers) + 1) if mob['loot'] else 0
    rows = ['\t'.join(str(v) for v in (number,) + entry) for loot, number in numbers.items() for entry in loot]
    return '\t'.join(schema.DROP_COLUMNS) + '\n' + ''.join(row + '\n' for row in rows)


def numbered(mobs, key, number_key) -> dict:
    """Each distinct non-empty list of mob[key] numbered from 1 in the order of the first mob that has it, and each
    mob's number_key set to its list's number, 0 when it has none."""
    numbers = {}
    for mob in mobs:
        mob[number_key] = numbers.setdefault(mob[key], len(numbers) + 1) if mob[key] else 0
    return numbers


def lists_text(columns, numbers) -> str:
    rows = ['\t'.join(str(v) for v in (number,) + entry) for entries, number in numbers.items() for entry in entries]
    return '\t'.join(columns) + '\n' + ''.join(row + '\n' for row in rows)


def regime_rows(raw, source) -> list:
    """The dump's {type: {zone: [page, ...]}} as a row a regime, in ID order: each page is its four counts, its level
    range, its experience and its regime, and its page number is its place in the zone's list."""
    rows = {}
    try:
        for kind, zones in raw.items():
            for zone, pages in zones.items():
                for page_number, page in enumerate(pages, start=1):
                    counts, low, high, exp, regime = page[:4], page[4], page[5], page[6], page[7]
                    if regime in rows:
                        raise schema.DataError(f'{source}: regime {regime} is on two pages')
                    rows[regime] = {'regime': regime, 'type': REGIME_TYPE_NAMES[kind], 'zone': int(zone),
                                    'page': page_number, 'counts': counts, 'minLevel': low, 'maxLevel': high, 'exp': exp}
    except (KeyError, TypeError, ValueError, IndexError, AttributeError) as error:
        raise schema.DataError(f'{source}: the regimes are not as headsup_dump.cpp writes them ({error!r})') from error
    return [rows[regime] for regime in sorted(rows)]


def regimes_text(regimes) -> str:
    return '\t'.join(schema.REGIME_COLUMNS) + '\n' + ''.join(
        '\t'.join(cell(regime[column]) for column in schema.REGIME_COLUMNS) + '\n' for regime in regimes)


def info_row(mob, scripts, regimes) -> str:
    targets, effect = scripts.of(mob['zoneName'], mob['script'])
    for regime, _ in targets:
        if regime not in regimes:
            raise schema.DataError(f"mob {mob['id']}'s script counts it for regime {regime}, which the dump has not")
    values = dict(mob, window=mob.get('window', ''), lottery=mob.get('lottery', [0, 0]), traits=traits(mob['traits']),
                  addEffect=(effect or schema.UNKNOWN_ADD_EFFECT) if mob['addEffect'] > 0 else '',
                  damageTaken=damage_taken(mob['damageMods']), skills=mob['skillsNumber'], spells=mob['spellsNumber'],
                  regimes=';'.join(f'{regime}:{target}' for regime, target in targets))
    return '\t'.join(cell(values[column]) for column in schema.INFO_COLUMNS)


def cell(value) -> str:
    if isinstance(value, list) and value and isinstance(value[0], list):
        return schema.LEVEL_SEPARATOR.join(cell(level) for level in value)
    return ','.join(str(v) for v in value) if isinstance(value, list) else str(value)


def items_text(items) -> str:
    return '\t'.join(schema.ITEM_COLUMNS) + '\n' + ''.join(
        '\t'.join(cell(item[column]) for column in schema.ITEM_COLUMNS) + '\n' for item in sorted(items, key=lambda i: i['id']))


def row(mob) -> str:
    values = dict(mob, name=display_name(mob['name']), flags=flags(mob))
    return '\t'.join(cell(values[column]) for column in schema.COLUMNS)


def trim(first_difference, rows):
    """GetBaseExp clamps the level difference to its table, so the dump's widest differences repeat the table's first
    and last rows: keep one of each."""
    start, end = 0, len(rows)
    while end - start > 1 and rows[start] == rows[start + 1]:
        start += 1
    while end - start > 1 and rows[end - 1] == rows[end - 2]:
        end -= 1
    return first_difference + start, rows[start:end]


def con_name(index, source):
    if type(index) is not int or not 0 <= index < len(gen_rules.CONS):
        raise schema.DataError(f'{source}: {index!r} is not a difficulty')
    return gen_rules.CONS[index]


def dump_rules(raw, source):
    first, rows = trim(raw['firstDifference'], raw['baseExp'])
    return {'firstDifference': first, 'baseExp': rows,
            'difficulty': [{'minExp': step['minExp'], 'con': con_name(step['con'], source)} for step in raw['difficulty']],
            'incrediblyEasyPrey': raw['incrediblyEasyPrey'], 'sittingAnimations': raw['sittingAnimations']}


def live_settings(text, source):
    """{(table, key): value} for each setting the file sets, as Lua reads the value."""
    settings = {}
    for number, line in enumerate(text.splitlines(), start=1):
        code = line.split('--', 1)[0].strip()
        if code in ('', 'xi = xi or {}', 'xi.settings = xi.settings or {}'):
            continue
        setting = SETTING.fullmatch(code)
        if setting is None:
            raise schema.DataError(f'{source}:{number}: not a setting of a number or a boolean: {line.strip()!r}')
        table, key, value = setting.groups()
        settings[(table, key)] = value == 'true' if value in ('true', 'false') else json.loads(value)
    return settings


def same_setting(got, want) -> bool:
    """Lua's equality: a boolean is never a number, and 1 is 1.0."""
    if isinstance(got, bool) or isinstance(want, bool):
        return type(got) is type(want) and got == want
    return isinstance(got, (int, float)) and got == want


def check_settings(ran, expected, source):
    """Every setting of expected as the server ran with it, a dump's {table: {key: value}}."""
    def value(table, key):
        values = ran.get(table) if isinstance(ran, dict) else None
        return values.get(key) if isinstance(values, dict) else None
    wrong = [(f'{table}.{key}', value(table, key), want) for (table, key), want in expected.items()
             if not same_setting(value(table, key), want)]
    if wrong:
        raise schema.DataError(f'{source}: the server ran with ' + ' and '.join(f'{name} {got!r}' for name, got, _ in wrong)
                               + ', where live_settings.lua has ' + ' and '.join(repr(want) for _, _, want in wrong))


def load(path, settings):
    try:
        dump = json.loads(pathlib.Path(path).read_text(encoding='utf-8'))
    except json.JSONDecodeError as error:
        raise schema.DataError(f'{path}: not a complete dump ({error}); delete it and refresh again') from error
    try:
        mobs, rules, items = dump['mobs'], dump_rules(dump['rules'], path), dump['items']
        lists = [drop_list(raw, f'{path}: drop list {index}') for index, raw in enumerate(dump['dropLists'])]
        names = {move['id']: move['name'] for move in dump['mobSkills']}
        skill_lists = {raw['id']: tuple((move, title(names[move])) for move in raw['skills'] if move in names)
                       for raw in dump['skillLists']}
        spell_lists = [tuple(tuple(spell) for spell in raw) for raw in dump['spellLists']]
        regimes = regime_rows(dump['regimes'], path)
    except (KeyError, TypeError) as error:
        raise schema.DataError(f'{path}: not a dump this compact.py reads ({error!r})') from error
    check_settings(dump.get('settings'), settings, path)
    width = len(schema.STAT_ORDER) + len(schema.COMBAT_ORDER)
    for mob in mobs:
        missing = MOB_KEYS - mob.keys()
        if missing:
            raise schema.DataError(f"{path}: mob {mob.get('id')} has no {', '.join(sorted(missing))}")
        index = mob.get('dropList')
        if index is not None and (type(index) is not int or not 0 <= index < len(lists)):
            raise schema.DataError(f"{path}: mob {mob['id']} has drop list {index!r}, past the {len(lists)} dumped")
        mob['loot'] = () if index is None else lists[index]
        if any(len(level) != width for level in mob['stats']):
            raise schema.DataError(f"{path}: mob {mob['id']} has a level of stats that is not the {width} values "
                                   'headsup_dump.cpp writes')
        mob['combat'] = [level[len(schema.STAT_ORDER):] for level in mob['stats']]
        mob['stats'] = [level[:len(schema.STAT_ORDER)] for level in mob['stats']]
        spells = mob.get('spellList', 0)
        if type(spells) is not int or not 0 <= spells <= len(spell_lists):
            raise schema.DataError(f"{path}: mob {mob['id']} has spell list {spells!r}, past the {len(spell_lists)} dumped")
        mob['skills'] = skill_lists.get(mob['skillList'], ())
        mob['spells'] = spell_lists[spells - 1] if spells else ()
    return mobs, rules, items, regimes


def agreed(dump_paths, values, what):
    for path, other in zip(dump_paths[1:], values[1:]):
        if other != values[0]:
            raise schema.DataError(f'{path}: its {what} differ from those of {dump_paths[0]}')
    return values[0]


def compact(dump_paths, out_dir, commit, settings_path, server_dir):
    settings_path = pathlib.Path(settings_path)
    settings = live_settings(settings_path.read_text(encoding='utf-8'), settings_path.name)
    snapshots = [load(path, settings) for path in dump_paths]
    differing = set()
    mobs = sorted(merge((dump_mobs for dump_mobs, _, _, _ in snapshots), differing), key=lambda m: m['id'])
    if differing:
        shown = ', '.join(str(i) for i in sorted(differing)[:DIFFERING_SHOWN])
        print(f'compact: {len(differing)} mobs have another drop list in a later dump, kept the first: {shown}'
              f'{", ..." if len(differing) > DIFFERING_SHOWN else ""}', file=sys.stderr)
    drops = drops_text(mobs)
    text = '\t'.join(schema.COLUMNS) + '\n' + '\n'.join(row(m) for m in mobs) + '\n'
    schema.generate(schema.parse(text, schema.TSV_NAME), schema.parse_drops(drops, schema.DROPS_NAME), commit)
    items = items_text(agreed(dump_paths, [items for _, _, items, _ in snapshots], 'items'))
    schema.generate_items(schema.parse_items(items, schema.ITEMS_NAME), commit)
    regimes = agreed(dump_paths, [regimes for _, _, _, regimes in snapshots], 'regimes')
    skills = lists_text(schema.SKILL_COLUMNS, numbered(mobs, 'skills', 'skillsNumber'))
    spells = lists_text(schema.SPELL_COLUMNS, numbered(mobs, 'spells', 'spellsNumber'))
    scripts, by_id = Scripts(server_dir), {regime['regime']: regime for regime in regimes}
    info = '\t'.join(schema.INFO_COLUMNS) + '\n' + ''.join(info_row(m, scripts, by_id) + '\n' for m in mobs)
    regime_table = regimes_text(regimes)
    schema.generate_info(schema.parse(text, schema.TSV_NAME), schema.parse_info(info), schema.parse_skills(skills),
                         schema.parse_spells(spells), schema.parse_regimes(regime_table), commit)
    rules_text = gen_rules.format_rules(agreed(dump_paths, [rules for _, rules, _, _ in snapshots], 'rules'))
    gen_rules.parse(rules_text, gen_rules.RULES_NAME)
    out_dir = pathlib.Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / schema.TSV_NAME).write_text(text, encoding='utf-8')
    (out_dir / schema.DROPS_NAME).write_text(drops, encoding='utf-8')
    (out_dir / schema.ITEMS_NAME).write_text(items, encoding='utf-8')
    for name, contents in ((schema.INFO_NAME, info), (schema.SKILLS_NAME, skills), (schema.SPELLS_NAME, spells),
                           (schema.REGIMES_NAME, regime_table)):
        (out_dir / name).write_text(contents, encoding='utf-8')
    (out_dir / gen_rules.RULES_NAME).write_text(rules_text, encoding='utf-8')
    zones = len({m['zone'] for m in mobs})
    generated = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')
    (out_dir / schema.META_NAME).write_text(
        f'{schema.META_COMMIT_KEY}\t{commit}\ngenerated\t{generated}\ndumps\t{len(dump_paths)}\nmobs\t{len(mobs)}\n'
        f'zones\t{zones}\ndropLists\t{max((m["drops"] for m in mobs), default=0)}\n', encoding='utf-8')
    return len(mobs), zones


def main(argv) -> int:
    if len(argv) < 6:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    try:
        count, zones = compact(argv[3:-2], argv[-2], argv[-1], argv[1], argv[2])
    except schema.DataError as error:
        print(f'compact: {error}', file=sys.stderr)
        return 1
    print(f'compact: {count} mobs in {zones} zones')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
