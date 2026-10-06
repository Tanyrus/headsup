#!/usr/bin/env python3
"""Turn one or more aggroglow_dump JSON files of the same server into phoenix_mobs.tsv and phoenix_mobs.meta.

Usage: compact.py DUMP_JSON [DUMP_JSON ...] OUT_DIR PHOENIX_COMMIT

Some mobs change aggression while the server runs (elementals, the Ghrah forms), so one dump is one snapshot. With
several dumps a mob counts as aggressive if any of them saw it attack: warn rather than paint an aggressive mob safe.
"""
import datetime
import json
import pathlib
import sys

# Bits of the `flags` column (spec section 1, "Committed data").
FLAG_AGGRO, FLAG_ALWAYS_AGGRO, FLAG_NO_AGGRO, FLAG_NEUTRAL, FLAG_NOTORIOUS, FLAG_BATTLEFIELD = 1, 2, 4, 8, 16, 32
FLAG_LINK, FLAG_TRUE_DETECTION = 64, 128
# Phoenix data/enums/mob_type.yaml.
MOBTYPE_NOTORIOUS, MOBTYPE_BATTLEFIELD = 0x02, 0x10
HEADER = 'id\tzone\tname\tminLevel\tmaxLevel\tflags\trespawn\tdetects'


def flags(mob) -> int:
    value = 0
    if mob['aggro']:
        value |= FLAG_AGGRO
    if mob['alwaysAggro'] > 0:
        value |= FLAG_ALWAYS_AGGRO
    if mob['noAggro'] > 0:
        value |= FLAG_NO_AGGRO
    if mob['neutral']:
        value |= FLAG_NEUTRAL
    if mob['type'] & MOBTYPE_NOTORIOUS:
        value |= FLAG_NOTORIOUS
    if mob['type'] & MOBTYPE_BATTLEFIELD:
        value |= FLAG_BATTLEFIELD
    # Dumps made before link and detection were exported have neither field.
    if mob.get('link'):
        value |= FLAG_LINK
    if mob.get('trueDetection'):
        value |= FLAG_TRUE_DETECTION
    return value


def display_name(name: str) -> str:
    """The client shows packet names with spaces instead of underscores."""
    if any(c in name for c in '\t\r\n'):
        raise ValueError(f'mob name contains a tab or newline: {name!r}')
    return name.replace('_', ' ')


AGGRO_FIELDS = ('aggro', 'alwaysAggro', 'noAggro', 'neutral')
DETECTION_FIELDS = ('link', 'detects', 'trueDetection')


def attacks(mob) -> bool:
    """The plugin's rule (src/classifier.cpp): aggressive or always-aggro, and neither no-aggro nor neutral."""
    return (bool(mob['aggro']) or mob['alwaysAggro'] > 0) and not mob['noAggro'] > 0 and not mob['neutral']


def merge(snapshots):
    """One record per mob ID. Identity, respawn and type come from the first snapshot that has the mob, link and
    detection from the first that exported them, the level range spans every snapshot, and the aggro fields come from
    the first snapshot in which the mob attacks."""
    merged = {}
    for snapshot in snapshots:
        for mob in snapshot:
            first = merged.get(mob['id'])
            if first is None:
                merged[mob['id']] = dict(mob)
                continue
            first['minLevel'] = min(first['minLevel'], mob['minLevel'])
            first['maxLevel'] = max(first['maxLevel'], mob['maxLevel'])
            if 'detects' not in first and 'detects' in mob:
                for field in DETECTION_FIELDS:
                    first[field] = mob[field]
            if not attacks(first) and attacks(mob):
                for field in AGGRO_FIELDS:
                    first[field] = mob[field]
    return list(merged.values())


def rows(mobs):
    return [f"{m['id']}\t{m['zone']}\t{display_name(m['name'])}\t{m['minLevel']}\t{m['maxLevel']}\t{flags(m)}\t"
            f"{max(0, int(m['respawn']))}\t{int(m.get('detects', 0)) & 0xFFFF}" for m in sorted(mobs, key=lambda m: m['id'])]


def compact(dump_paths, out_dir, commit):
    mobs = merge(json.loads(pathlib.Path(path).read_text(encoding='utf-8')) for path in dump_paths)
    out_dir = pathlib.Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / 'phoenix_mobs.tsv').write_text(HEADER + '\n' + '\n'.join(rows(mobs)) + '\n', encoding='utf-8')
    zones = len({m['zone'] for m in mobs})
    generated = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')
    (out_dir / 'phoenix_mobs.meta').write_text(
        f'phoenix_commit\t{commit}\ngenerated\t{generated}\ndumps\t{len(dump_paths)}\nmobs\t{len(mobs)}\nzones\t{zones}\n',
        encoding='utf-8')
    return len(mobs), zones


def main(argv) -> int:
    if len(argv) < 4:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    count, zones = compact(argv[1:-2], argv[-2], argv[-1])
    print(f'compact: {count} mobs in {zones} zones')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
