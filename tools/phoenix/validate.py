#!/usr/bin/env python3
"""Check phoenix_mobs.tsv and phoenix_drops.tsv against spawns whose values are known, and phoenix_rules.json against
rules whose values are known. Exit 1 on any failure.

Usage: validate.py PHOENIX_MOBS_TSV PHOENIX_RULES_JSON [PHOENIX_DROPS_TSV]

PHOENIX_DROPS_TSV defaults to the phoenix_drops.tsv beside PHOENIX_MOBS_TSV.
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
# Mobs with immunities, with damage taken other than 100%, and with a resistance rank of 4 or more: thousands each in
# Phoenix, so fewer means the dump read those columns before they were set.
MIN_RESISTING = 1000
STRONG_RANK = 4
# id: EXP_LVL_MOD, set by the mob's spawn script
EXPECTED_LEVEL_MODS = {
    17190918: -2,  # Wild Rabbit, scripts/zones/East_Ronfaure/mobs/Wild_Rabbit.lua
}
# placeholder id: its NM's, from the NM's script (entity.phList)
EXPECTED_PLACEHOLDERS = {
    17191194: 17191196,  # Carrion Worm for Bigmouth Billy, scripts/zones/East_Ronfaure/mobs/Bigmouth_Billy.lua
    17191195: 17191196,
}
Immunity = schema.Immunity
# (zone, name): immunity bits its spawn script adds, which only the spawned mob carries
EXPECTED_IMMUNITIES = {
    (103, 'Doman'): Immunity.DARK_SLEEP | Immunity.SILENCE | Immunity.STUN,  # scripts/zones/Valkurm_Dunes/mobs/Doman.lua
}
# Mobs with loot and distinct drop lists: tens of thousands and thousands in Phoenix, so fewer means the dump missed the
# lists of whole zones.
MIN_LOOTING, MIN_DROP_LISTS = 40000, 2000
# Lists with each kind of entry, and with groups: hundreds or more each.
MIN_LISTS_OF_A_KIND = 100
VERY_COMMON = 240  # Phoenix data/enums/drop_rate.yaml, per mille
IRON_ORE, MYTHRIL_ORE, GOLD_ORE, FIEND_BLOOD = 643, 644, 737, 924  # Phoenix sql/item_basic.sql
BIGMOUTH_BILLY, FLEDERMAUS_ZONE = 17191196, 106  # East Ronfaure's NM; North Gustaberg
# id: entries (kind, group, groupRate, item, rate) its list must have, from data/zones/<zone>/mobs.yaml
EXPECTED_DROPS = {
    BIGMOUTH_BILLY: {('drop', 0, 0, IRON_ORE, VERY_COMMON), ('drop', 0, 0, MYTHRIL_ORE, VERY_COMMON),
                     ('steal', 0, 0, GOLD_ORE, 0)},
}
# (zone, name): items a YAML module took out of its list (modules/era/data/abyssea/mob_droplist_removal)
EXPECTED_REMOVED = {
    (FLEDERMAUS_ZONE, 'Fledermaus'): {FIEND_BLOOD},
}
EVEN_MATCH_EXP = 100  # every bracket of the era table, which only content restricted to ToAU loads (200 without)
RESTING, SITTING = 33, 47  # xi::Animation Healing and Sit, which CBattleEntity::isSitting names


def load(path):
    """id -> the record gen_mobdata.parse reads, which also checks every column."""
    return {r['id']: r for r in schema.parse(pathlib.Path(path).read_text(encoding='utf-8'), str(path))}


def check(records, min_mobs=MIN_MOBS, min_zones=MIN_ZONES, min_resisting=MIN_RESISTING):
    problems = []
    if len(records) < min_mobs:
        problems.append(f'only {len(records)} mobs (expected at least {min_mobs})')
    zones = len({r['zone'] for r in records.values()})
    if zones < min_zones:
        problems.append(f'only {zones} zones (expected at least {min_zones})')
    for mob_id, (name, lo, hi, aggressive, links, detects) in EXPECTED.items():
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
    for mob_id, mod in EXPECTED_LEVEL_MODS.items():
        r = records.get(mob_id)
        if r is None or r['expLevelMod'] != mod:
            problems.append(f"{mob_id}: expected level mod {mod}, got {None if r is None else r['expLevelMod']}")
    for mob_id, nm in EXPECTED_PLACEHOLDERS.items():
        r = records.get(mob_id)
        if r is None or r['placeholderOf'] != nm:
            problems.append(f"{mob_id}: expected a placeholder of {nm}, got {None if r is None else r['placeholderOf']}")
    counts = {'immunities': sum(1 for r in records.values() if r['immunity']),
              'damage taken other than 100%': sum(1 for r in records.values() if any(r['sdt'])),
              f'a resistance rank of {STRONG_RANK} or more': sum(
                  1 for r in records.values() if any(v >= STRONG_RANK for v in r['elementRanks'] + r['statusRanks']))}
    for what, count in counts.items():
        if count < min_resisting:
            problems.append(f'only {count} mobs with {what} (expected at least {min_resisting})')
    for (zone, name), bits in EXPECTED_IMMUNITIES.items():
        found = [r for r in records.values() if r['zone'] == zone and r['name'] == name]
        if not found:
            problems.append(f'{name} (zone {zone}): missing')
        elif any(r['immunity'] != bits for r in found):
            got = ', '.join(str(r['immunity']) for r in found)
            problems.append(f'{name} (zone {zone}): expected immunity bits {int(bits)}, got {got}')
    return problems


def entries(entry_dicts) -> set:
    """Entries as (kind, group, groupRate, item, rate), as EXPECTED_DROPS gives them."""
    return {tuple(e[column] for column in schema.DROP_COLUMNS[1:]) for e in entry_dicts}


def check_drops(records, lists, min_looting=MIN_LOOTING, min_lists=MIN_DROP_LISTS, min_of_a_kind=MIN_LISTS_OF_A_KIND):
    problems = []
    looting = sum(1 for r in records.values() if r['drops'])
    if looting < min_looting:
        problems.append(f'only {looting} mobs with loot (expected at least {min_looting})')
    if len(lists) < min_lists:
        problems.append(f'only {len(lists)} drop lists (expected at least {min_lists})')
    unused = set(range(1, len(lists) + 1)) - {r['drops'] for r in records.values()}
    if unused:
        problems.append(f'{len(unused)} drop lists no mob has, the first {min(unused)}')
    counts = {f'{kind} entries': sum(1 for loot in lists if any(e['kind'] == kind for e in loot))
              for kind in schema.DROP_KINDS}
    counts['groups'] = sum(1 for loot in lists if any(e['group'] for e in loot))
    for what, count in counts.items():
        if count < min_of_a_kind:
            problems.append(f'only {count} drop lists with {what} (expected at least {min_of_a_kind})')

    def loot_of(r):
        return lists[r['drops'] - 1] if 0 < r['drops'] <= len(lists) else []
    for mob_id, expected in EXPECTED_DROPS.items():
        r = records.get(mob_id)
        missing = expected - (set() if r is None else entries(loot_of(r)))
        if missing:
            problems.append(f'{mob_id}: its drop list has no {sorted(missing)}')
    for (zone, name), items in EXPECTED_REMOVED.items():
        found = [r for r in records.values() if r['zone'] == zone and r['name'] == name]
        if not found:
            problems.append(f'{name} (zone {zone}): missing')
        elif any(e['item'] in items for r in found for e in loot_of(r)):
            problems.append(f'{name} (zone {zone}): still drops one of {sorted(items)}, which a module removes')
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
    if len(argv) not in (3, 4):
        print(__doc__.strip(), file=sys.stderr)
        return 2
    try:
        rules_path = pathlib.Path(argv[2])
        drops_path = pathlib.Path(argv[3] if len(argv) == 4 else pathlib.Path(argv[1]).parent / schema.DROPS_NAME)
        records = load(argv[1])
        lists = schema.parse_drops(drops_path.read_text(encoding='utf-8'), str(drops_path))
        problems = (check(records) + check_drops(records, lists) +
                    check_rules(gen_rules.parse(rules_path.read_text(encoding='utf-8'), str(rules_path))))
    except schema.DataError as error:
        problems = [str(error)]
    for problem in problems:
        print(f'validate: {problem}', file=sys.stderr)
    print(f'validate: {"FAILED" if problems else "ok"}')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
