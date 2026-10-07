#!/usr/bin/env python3
"""Turn one or more headsup_dump JSON files of the same server into phoenix_mobs.tsv, phoenix_mobs.meta and
phoenix_rules.json.

Usage: compact.py DUMP_JSON [DUMP_JSON ...] OUT_DIR PHOENIX_COMMIT

Some mobs change aggression while the server runs (elementals, the Ghrah forms), so one dump is one snapshot. With
several dumps a mob counts as aggressive if any of them saw it attack: warn rather than paint an aggressive mob safe.
Every dump that carries the rules must carry the same rules.
"""
import datetime
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import gen_mobdata as schema  # noqa: E402
import gen_rules  # noqa: E402

MobFlag = schema.MobFlag
# Phoenix data/enums/mob_type.yaml.
MOBTYPE_NOTORIOUS, MOBTYPE_BATTLEFIELD = 0x02, 0x10


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
    if mob['type'] & MOBTYPE_BATTLEFIELD:
        value |= MobFlag.BATTLEFIELD
    # Dumps made before a field was exported read as none.
    if mob.get('link'):
        value |= MobFlag.LINK
    if mob.get('trueDetection'):
        value |= MobFlag.TRUE_DETECTION
    if mob.get('follows'):
        value |= MobFlag.FOLLOWS
    return int(value)


def display_name(name: str) -> str:
    """The client shows packet names with spaces instead of underscores."""
    if any(c in name for c in '\t\r\n'):
        raise schema.DataError(f'mob name contains a tab or newline: {name!r}')
    return name.replace('_', ' ')


AGGRO_FIELDS = ('aggro', 'alwaysAggro', 'noAggro')
# Fields later dump modules added, each group taken from the first snapshot that exported it.
ADDED_FIELDS = (('link', 'detects', 'trueDetection'), ('expLevelMod', 'follows'))


def attacks(mob) -> bool:
    """The part of the plugin's rule (src/mobdata.cpp) that changes while the server runs: aggressive or always-aggro,
    and not no-aggro. Older dumps also hold m_neutral, the AI's brief calm after a spawn or a fight, which is ignored."""
    return (bool(mob['aggro']) or mob['alwaysAggro'] > 0) and not mob['noAggro'] > 0


def merge(snapshots):
    """One record per mob ID. Identity, respawn and type come from the first snapshot that has the mob, added fields
    from the first that exported them, the level range spans every snapshot, and the aggro fields come from the first
    snapshot in which the mob attacks, as dumped or as the dump saw it at some moment of its run (seenAttacking)."""
    merged = {}
    for snapshot in snapshots:
        for mob in snapshot:
            first = merged.get(mob['id'])
            if first is None:
                first = merged[mob['id']] = dict(mob)
            else:
                first['minLevel'] = min(first['minLevel'], mob['minLevel'])
                first['maxLevel'] = max(first['maxLevel'], mob['maxLevel'])
                for group in ADDED_FIELDS:
                    if group[0] not in first and group[0] in mob:
                        for field in group:
                            first[field] = mob[field]
            for sample in (mob, mob.get('seenAttacking')):
                if sample is not None and not attacks(first) and attacks(sample):
                    for field in AGGRO_FIELDS:
                        first[field] = sample[field]
    return list(merged.values())


def row(mob) -> str:
    values = {'id': mob['id'], 'zone': mob['zone'], 'name': display_name(mob['name']), 'minLevel': mob['minLevel'],
              'maxLevel': mob['maxLevel'], 'flags': flags(mob), 'respawn': max(0, int(mob['respawn'])),
              'detects': int(mob.get('detects', 0)), 'expLevelMod': int(mob.get('expLevelMod', 0))}
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
    """A dump's mobs and rules; dumps made before the rules were exported are a bare list of mobs."""
    try:
        dump = json.loads(pathlib.Path(path).read_text(encoding='utf-8'))
    except json.JSONDecodeError as error:
        raise schema.DataError(f'{path}: not a complete dump ({error}); delete it and refresh again') from error
    if isinstance(dump, list):
        return dump, None
    try:
        return dump['mobs'], dump_rules(dump['rules'], path)
    except (KeyError, TypeError) as error:
        raise schema.DataError(f'{path}: not a dump this compact.py reads ({error!r})') from error


def agreed_rules(dump_paths, snapshots):
    carried = [(path, rules) for path, (_, rules) in zip(dump_paths, snapshots) if rules is not None]
    if not carried:
        raise schema.DataError('no dump carries the rules; refresh with the current dump module')
    first_path, first = carried[0]
    for path, rules in carried[1:]:
        if rules != first:
            raise schema.DataError(f'{path}: its rules differ from those of {first_path}')
    return first


def compact(dump_paths, out_dir, commit):
    snapshots = [load(path) for path in dump_paths]
    mobs = sorted(merge(dump_mobs for dump_mobs, _ in snapshots), key=lambda m: m['id'])
    text = '\t'.join(schema.COLUMNS) + '\n' + '\n'.join(row(m) for m in mobs) + '\n'
    schema.parse(text, schema.TSV_NAME)
    rules_text = gen_rules.format_rules(agreed_rules(dump_paths, snapshots))
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
