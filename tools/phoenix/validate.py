#!/usr/bin/env python3
"""Check phoenix_mobs.tsv against spawns whose values are known (spec section 1, step 9). Exit 1 on any failure.

Usage: validate.py PHOENIX_MOBS_TSV
"""
import pathlib
import sys

FLAG_AGGRO = 1
# id: (name, minLevel, maxLevel, aggressive or None when not checked)
EXPECTED = {
    17199105: ('Stag Crab', 15, 17, None),             # base data/zones/valkurm_dunes/mobs.yaml
    17272838: ('Guardian Treant', 32, 32, None),       # modules/era/data/abyssea/advanced_job_quest_mob_stats
    17199322: ('Snipper', 19, 20, False),              # seen live in Valkurm Dunes
    17199648: ('Goblin Bounty Hunter', 17, 20, True),  # seen live in Valkurm Dunes
}
MIN_MOBS, MIN_ZONES = 60000, 200


def load(path):
    """id -> (zone, name, minLevel, maxLevel, flags, respawn, detects)"""
    records = {}
    for line in pathlib.Path(path).read_text(encoding='utf-8').splitlines()[1:]:
        mob_id, zone, name, lo, hi, flags, respawn, detects = line.split('\t')
        records[int(mob_id)] = (int(zone), name, int(lo), int(hi), int(flags), int(respawn), int(detects))
    return records


def check(records, expected=EXPECTED, min_mobs=MIN_MOBS, min_zones=MIN_ZONES):
    problems = []
    if len(records) < min_mobs:
        problems.append(f'only {len(records)} mobs (expected at least {min_mobs})')
    zones = len({r[0] for r in records.values()})
    if zones < min_zones:
        problems.append(f'only {zones} zones (expected at least {min_zones})')
    for mob_id, (name, lo, hi, aggressive) in expected.items():
        record = records.get(mob_id)
        if record is None:
            problems.append(f'{mob_id} {name}: missing')
            continue
        _, got_name, got_lo, got_hi, flags = record[:5]
        if (got_name, got_lo, got_hi) != (name, lo, hi):
            problems.append(f'{mob_id}: expected {name} {lo}-{hi}, got {got_name} {got_lo}-{got_hi}')
        if aggressive is not None and bool(flags & FLAG_AGGRO) != aggressive:
            problems.append(f'{mob_id} {name}: expected aggressive={aggressive}')
    return problems


def main(argv) -> int:
    if len(argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    problems = check(load(argv[1]))
    for problem in problems:
        print(f'validate: {problem}', file=sys.stderr)
    print(f'validate: {"FAILED" if problems else "ok"}')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
