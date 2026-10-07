#!/usr/bin/env python3
"""Turn one or more headsup_dump JSON files of the same server into phoenix_mobs.tsv, phoenix_mobs.meta and
phoenix_rules.json.

Usage: compact.py DUMP_JSON [DUMP_JSON ...] OUT_DIR PHOENIX_COMMIT

Some mobs change aggression while the server runs (elementals, the Ghrah forms), so one dump is one snapshot. With
several dumps a mob counts as aggressive if any of them saw it attack, so an aggressive mob is never painted safe.
Every dump must carry the same rules.
"""
import datetime
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import gen_mobdata as schema  # noqa: E402
import gen_rules  # noqa: E402

MobFlag = schema.MobFlag
MOBTYPE_NOTORIOUS = 0x02  # Phoenix data/enums/mob_type.yaml
MOB_KEYS = {'id', 'zone', 'name', 'minLevel', 'maxLevel', 'respawn', 'aggro', 'alwaysAggro', 'noAggro', 'type', 'link',
            'detects', 'trueDetection', 'expLevelMod', 'follows', 'placeholderOf'}
AGGRO_FIELDS = ('aggro', 'alwaysAggro', 'noAggro')


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
    return int(value)


def display_name(name: str) -> str:
    """The client shows packet names with spaces instead of underscores."""
    if any(c in name for c in '\t\r\n'):
        raise schema.DataError(f'mob name contains a tab or newline: {name!r}')
    return name.replace('_', ' ')


def attacks(mob) -> bool:
    """The part of the plugin's rule (src/mobdata.cpp) that changes while the server runs: aggressive or always-aggro,
    and not no-aggro."""
    return (bool(mob['aggro']) or mob['alwaysAggro'] > 0) and not mob['noAggro'] > 0


def merge(snapshots):
    """One record per mob ID. The level range spans every snapshot, the aggro fields come from the first snapshot in
    which the mob attacks, as dumped or as the dump saw it at some moment of its run (seenAttacking), and the rest
    comes from the first snapshot that has the mob."""
    merged = {}
    for snapshot in snapshots:
        for mob in snapshot:
            first = merged.get(mob['id'])
            if first is None:
                first = merged[mob['id']] = dict(mob)
            else:
                first['minLevel'] = min(first['minLevel'], mob['minLevel'])
                first['maxLevel'] = max(first['maxLevel'], mob['maxLevel'])
            # The dumped state first: seenAttacking can be from before the spawn script set AlwaysAggro (Lioumere).
            for sample in (mob, mob.get('seenAttacking')):
                if sample is not None and not attacks(first) and attacks(sample):
                    for field in AGGRO_FIELDS:
                        first[field] = sample[field]
    return list(merged.values())


def row(mob) -> str:
    values = dict(mob, name=display_name(mob['name']), flags=flags(mob))
    return '\t'.join(str(values[column]) for column in schema.COLUMNS)


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


def load(path):
    try:
        dump = json.loads(pathlib.Path(path).read_text(encoding='utf-8'))
    except json.JSONDecodeError as error:
        raise schema.DataError(f'{path}: not a complete dump ({error}); delete it and refresh again') from error
    try:
        mobs, rules = dump['mobs'], dump_rules(dump['rules'], path)
    except (KeyError, TypeError) as error:
        raise schema.DataError(f'{path}: not a dump this compact.py reads ({error!r})') from error
    for mob in mobs:
        missing = MOB_KEYS - mob.keys()
        if missing:
            raise schema.DataError(f"{path}: mob {mob.get('id')} has no {', '.join(sorted(missing))}")
    return mobs, rules


def agreed_rules(dump_paths, rules):
    for path, other in zip(dump_paths[1:], rules[1:]):
        if other != rules[0]:
            raise schema.DataError(f'{path}: its rules differ from those of {dump_paths[0]}')
    return rules[0]


def compact(dump_paths, out_dir, commit):
    snapshots = [load(path) for path in dump_paths]
    mobs = sorted(merge(dump_mobs for dump_mobs, _ in snapshots), key=lambda m: m['id'])
    text = '\t'.join(schema.COLUMNS) + '\n' + '\n'.join(row(m) for m in mobs) + '\n'
    schema.parse(text, schema.TSV_NAME)
    rules_text = gen_rules.format_rules(agreed_rules(dump_paths, [rules for _, rules in snapshots]))
    gen_rules.parse(rules_text, gen_rules.RULES_NAME)
    out_dir = pathlib.Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / schema.TSV_NAME).write_text(text, encoding='utf-8')
    (out_dir / gen_rules.RULES_NAME).write_text(rules_text, encoding='utf-8')
    zones = len({m['zone'] for m in mobs})
    generated = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')
    (out_dir / schema.META_NAME).write_text(
        f'{schema.META_COMMIT_KEY}\t{commit}\ngenerated\t{generated}\ndumps\t{len(dump_paths)}\nmobs\t{len(mobs)}\n'
        f'zones\t{zones}\n', encoding='utf-8')
    return len(mobs), zones


def main(argv) -> int:
    if len(argv) < 4:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    try:
        count, zones = compact(argv[1:-2], argv[-2], argv[-1])
    except schema.DataError as error:
        print(f'compact: {error}', file=sys.stderr)
        return 1
    print(f'compact: {count} mobs in {zones} zones')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
