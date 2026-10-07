#!/usr/bin/env python3
"""Check phoenix_mobs.tsv against spawns whose values are known, and phoenix_rules.json against rules whose values are
known. Exit 1 on any failure.

Usage: validate.py PHOENIX_MOBS_TSV PHOENIX_RULES_JSON
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import gen_mobdata as schema  # noqa: E402
import gen_rules  # noqa: E402

SIGHT, HEARING = schema.Detect.SIGHT, schema.Detect.HEARING
# id: (name, minLevel, maxLevel, aggressive, links, detects); None where not checked
EXPECTED = {
    17199105: ('Stag Crab', 15, 17, None, None, None),          # base data/zones/valkurm_dunes/mobs.yaml
    17272838: ('Guardian Treant', 32, 32, None, None, None),    # modules/era/data/abyssea/advanced_job_quest_mob_stats
    17199322: ('Snipper', 19, 20, False, False, HEARING),       # seen live in Valkurm Dunes
    17199648: ('Goblin Bounty Hunter', 17, 20, True, True, SIGHT),
}
MIN_MOBS, MIN_ZONES = 60000, 200
# id: EXP_LVL_MOD, set by the mob's spawn script
EXPECTED_LEVEL_MODS = {
    17190918: -2,  # Wild Rabbit, scripts/zones/East_Ronfaure/mobs/Wild_Rabbit.lua
}
# placeholder id: its NM's, from the NM's script (entity.phList)
EXPECTED_PLACEHOLDERS = {
    17191194: 17191196,  # Carrion Worm for Bigmouth Billy, scripts/zones/East_Ronfaure/mobs/Bigmouth_Billy.lua
    17191195: 17191196,
}
EVEN_MATCH_EXP = 100  # every bracket of every experience table Phoenix ships
RESTING, SITTING = 33, 47  # xi::Animation Healing and Sit, which CBattleEntity::isSitting names


def load(path):
    """id -> the record gen_mobdata.parse reads, which also checks every column."""
    return {r['id']: r for r in schema.parse(pathlib.Path(path).read_text(encoding='utf-8'), str(path))}


def check(records, expected=EXPECTED, min_mobs=MIN_MOBS, min_zones=MIN_ZONES, level_mods=EXPECTED_LEVEL_MODS,
          placeholders=EXPECTED_PLACEHOLDERS):
    problems = []
    if len(records) < min_mobs:
        problems.append(f'only {len(records)} mobs (expected at least {min_mobs})')
    zones = len({r['zone'] for r in records.values()})
    if zones < min_zones:
        problems.append(f'only {zones} zones (expected at least {min_zones})')
    if not any(r['detects'] for r in records.values()):
        problems.append('no mob has any detection: the dumps predate link and detection')
    for mob_id, (name, lo, hi, aggressive, links, detects) in expected.items():
        r = records.get(mob_id)
        if r is None:
            problems.append(f'{mob_id} {name}: missing')
            continue
        if (r['name'], r['minLevel'], r['maxLevel']) != (name, lo, hi):
            problems.append(f"{mob_id}: expected {name} {lo}-{hi}, got {r['name']} {r['minLevel']}-{r['maxLevel']}")
        if aggressive is not None and bool(r['flags'] & schema.MobFlag.AGGRESSIVE) != aggressive:
            problems.append(f'{mob_id} {name}: expected aggressive={aggressive}')
        if links is not None and bool(r['flags'] & schema.MobFlag.LINK) != links:
            problems.append(f'{mob_id} {name}: expected links={links}')
        if detects is not None and r['detects'] != detects:
            problems.append(f"{mob_id} {name}: expected detects {int(detects)}, got {r['detects']}")
    for mob_id, mod in level_mods.items():
        r = records.get(mob_id)
        if r is None or r['expLevelMod'] != mod:
            problems.append(f"{mob_id}: expected level mod {mod}, got {None if r is None else r['expLevelMod']}")
    for mob_id, nm in placeholders.items():
        r = records.get(mob_id)
        if r is None or r['placeholderOf'] != nm:
            problems.append(f"{mob_id}: expected a placeholder of {nm}, got {None if r is None else r['placeholderOf']}")
    return problems


def check_rules(rules):
    problems = []
    even = rules['baseExp'][-rules['firstDifference']] if 0 <= -rules['firstDifference'] < len(rules['baseExp']) else None
    if even is None or any(exp != EVEN_MATCH_EXP for exp in even):
        problems.append(f'an even match should give {EVEN_MATCH_EXP} experience in every bracket, got {even}')
    if not {RESTING, SITTING} <= set(rules['sittingAnimations']):
        problems.append(f"resting ({RESTING}) and /sit ({SITTING}) should count as sitting, got {rules['sittingAnimations']}")
    return problems


def main(argv) -> int:
    if len(argv) != 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    try:
        rules_path = pathlib.Path(argv[2])
        problems = check(load(argv[1])) + check_rules(gen_rules.parse(rules_path.read_text(encoding='utf-8'), str(rules_path)))
    except schema.DataError as error:
        problems = [str(error)]
    for problem in problems:
        print(f'validate: {problem}', file=sys.stderr)
    print(f'validate: {"FAILED" if problems else "ok"}')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
