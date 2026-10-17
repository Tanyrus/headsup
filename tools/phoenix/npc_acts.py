"""What each ??? does, from the probe's run on the server (tools/phoenix/npc_probe.lua), for npcs.py.

The probe groups each ???'s runs by what they did, naming each run by its scenario: a player state ('base',
'keyItems', 'questAccepted', 'accepted+var2', a trade's 'try 3+accepted', a quest step's 'step progress 4'), then
what else it set (', weather 3', ', hour 22', ', keyItems only 345', ', npcVar 1'). An act is one group in words:
what the player does (clicks, trades a set of items), the simplest scenario it happens in as condition tokens, and
what follows (messages, mobs popped, items and key items given, a cutscene). Quest, mission and battlefield steps are
run apart and stay out when their link is off on live. A zone's clock, weather and mob deaths give the acts that show
or hide a ???.
"""
import json
import pathlib
import re

DIMENSIONS = ('weather', 'hour', 'job', 'weekday', 'nation', 'moon', 'playerVar', 'npcVar', 'random', 'keyItems',
              'vars')
# How many values a dimension has: an outcome under every one of them does not depend on it.
FULL = {'weather': 20, 'hour': 24, 'job': 22, 'weekday': 8, 'nation': 3}
# The probe's scenarios from the plainest up: an outcome's condition is that of the first one it happens in.
BASE_ORDER = ['base', 'step', 'keyItems', 'step+keyItems', 'questAccepted', 'questDone', 'items']
# Effects that are a message: the probe notes [kind, text ID, params..., the text's name].
MESSAGES = ('message', 'showText', 'messageText')
ACT_KINDS = ['click', 'trade', 'shows', 'hides']


def parse(name: str):
    """A scenario's name as (its player state, [(dimension, value)]), a cutscene's end and repeats dropped."""
    name = re.sub(r'#\d+$', '', name.split('>')[0])
    parts = name.split(', ')
    first, _, value = parts[0].partition(' ')
    if value and first in DIMENSIONS:  # 'hour 5' alone; a bare 'keyItems' is the player state
        parts = ['base'] + parts
    base = parts[0]
    base = re.sub(r'^try \d+\+keyItems$', 'keyItems', base)
    base = re.sub(r'^try \d+\+accepted$', 'questAccepted', base)
    base = re.sub(r'^try \d+$', 'base', base)
    dims = []
    for d in parts[1:]:
        head, _, value = d.partition(' ')
        dims.append((head, value))
    return base, dims


def base_rank(base: str) -> tuple:
    if base in BASE_ORDER:
        return 0, BASE_ORDER.index(base), 0
    m = re.fullmatch(r'step progress (\d+)(\+keyItems)?', base)
    if m:
        return 1, int(m.group(1)), bool(m.group(2))
    m = re.fullmatch(r'(?:var|accepted\+var)(\d+)', base)
    if m:
        return 2, int(m.group(1)), 0
    return 3, 0, 0


def hour_ranges(hours) -> list:
    """Hours as 'from-to' runs, a run through midnight joined: [22, 23, 0, 1] -> ['22-1']."""
    hours = sorted(set(hours))
    runs, start = [], None
    for h in hours:
        if start is None:
            start = prev = h
        elif h == prev + 1:
            prev = h
        else:
            runs.append([start, prev])
            start = prev = h
    if start is not None:
        runs.append([start, prev])
    if len(runs) > 1 and runs[0][0] == 0 and runs[-1][1] == 23:
        runs[0][0] = runs.pop()[0]
    return [f'{a}-{b}' for a, b in runs]


def conditions(names) -> list:
    """Condition tokens of an outcome group: those of its plainest player state, with the values of each dimension
    it needed there (none when it happens there without one)."""
    parsed = [parse(n) for n in names]
    best = min((p[0] for p in parsed), key=base_rank)
    dim_lists = [p[1] for p in parsed if p[0] == best]
    tokens = []
    if best in ('keyItems', 'step+keyItems'):
        tokens.append('keyItems')
    elif best in ('questAccepted', 'questDone', 'items') or best.startswith('var') or best.startswith('accepted+'):
        tokens.append('quest')
    m = re.fullmatch(r'step progress (\d+)(\+keyItems)?', best)
    if m:
        tokens.append(f'step:{m.group(1)}')
        if m.group(2):
            tokens.append('keyItems')
    if any(not dims for dims in dim_lists):
        return sorted(set(tokens))
    values = {}
    for dims in dim_lists:
        for head, value in dims:
            values.setdefault(head, set()).add(value)
    for head, vals in values.items():
        if head in FULL and len(vals) >= FULL[head]:
            continue
        if head == 'hour':
            tokens += [f'hour:{r}' for r in hour_ranges(int(v) for v in vals)]
        elif head in ('weather', 'job', 'weekday', 'nation'):
            tokens += [f"{'day' if head == 'weekday' else head}:{v}" for v in vals]
        elif head == 'keyItems':
            for v in vals:
                how, _, ki = v.rpartition(' ')
                tokens.append(('keyItem:' if how == 'only' else 'noKeyItem:') + ki)
        elif head == 'npcVar':
            tokens.append('primed')
        elif head == 'playerVar':
            tokens.append('nmKilled')
        elif head == 'random':
            tokens.append('roll')
        elif head == 'vars':
            tokens.append('quest')
    return sorted(set(tokens), key=token_order)


def token_order(token: str):
    head, _, value = token.partition(':')
    m = re.match(r'(\d+)', value)
    return head, int(m.group(1)) if m else 0, value


def ints(v) -> list:
    """Every whole number in a noted value: a number, a list, or a table of them."""
    if isinstance(v, bool):
        return []
    if isinstance(v, int):
        return [v]
    if isinstance(v, list):
        return [x for e in v for x in ints(e)]
    if isinstance(v, dict):
        return [x for e in v.values() for x in ints(e)]
    return []


def effects(done, item_ids, key_item_ids):
    """(messages as (text ID, params), pops, gives, key items, starts a cutscene) of a group's notes."""
    says, pops, gives, key_items, event = [], set(), set(), set(), False
    for e in done:
        kind = e[0]
        if kind in MESSAGES and len(e) > 1 and isinstance(e[1], int):
            message = (e[1], tuple(p for p in e[2:-1] if isinstance(p, int) and not isinstance(p, bool)))
            if message not in says:
                says.append(message)
        elif kind in ('pops', 'popFromQM') and len(e) > 1:
            pops.update(x for x in ints(e[1]) if x >= 0x1000000)
        elif kind == 'gives' and len(e) > 1:
            gives.update(x for x in ints(e[1:]) if x in item_ids)
        elif kind == 'givesKeyItem' and len(e) > 1:
            key_items.update(x for x in ints(e[1]) if x in key_item_ids)
        elif kind == 'event':
            event = True
    return says, sorted(pops), sorted(gives), sorted(key_items), event


def trade_items(tries, names) -> list:
    """The items of the try a trade group is named for (its first), as [item, qty] pairs."""
    for n in names:
        m = re.match(r'try (\d+)', n)
        if m and int(m.group(1)) <= len(tries):
            return [list(pair) for pair in tries[int(m.group(1)) - 1]]
    return []


def load(path):
    """The probe's targets by NPC ID, and its zone sweeps."""
    targets, sweeps = {}, []
    for line in pathlib.Path(path).read_text(encoding='utf-8').splitlines():
        record = json.loads(line)
        if 'target' in record:
            targets[record['target']['id']] = record['target']
        elif 'sweep' in record:
            sweeps.append(record['sweep'])
    return targets, sweeps


def step_key(step):
    """A step's link key, as npcs.py keys its links: (kind, log, ID, hidden quest name)."""
    cid = step.get('id') or ''
    if step.get('quest') is not None:
        return 'quest', step.get('log'), step['quest'], ''
    if step.get('mission') is not None:
        return 'mission', step.get('log'), step['mission'], ''
    m = re.match(r'Battlefield\[(\d+)\]', cid)
    if m:
        return 'battlefield', 0, int(m.group(1)), ''
    m = re.match(r'HQuest\[(.+)\]', cid)
    if m:
        return 'hidden', 0, 0, step.get('name') or m.group(1)
    return None


def acts_of(target, sweeps_of, item_ids, key_item_ids, link_numbers, live):
    """Every act of one ???: dicts of act, items, when (tokens), says, pops, gives, keyItems, event."""
    acts = []

    def add(kind, groups, tries=None, extra=()):
        for group in groups:
            says, pops, gives, key_items, event = effects(group['done'], item_ids, key_item_ids)
            if not (says or pops or gives or key_items or event):
                continue
            names = group['scenarios']
            items = trade_items(tries, names) if tries is not None else []
            if tries is not None and not items:
                continue
            act = {'act': kind, 'items': items, 'when': sorted(set(conditions(names)) | set(extra), key=token_order),
                   'says': says, 'pops': pops, 'gives': gives, 'keyItems': key_items, 'event': event}
            if act not in acts:
                acts.append(act)

    add('click', target['trigger']['outcomes'])
    add('trade', target['trade']['outcomes'], target['trade'].get('tries') or [])
    for step in target.get('steps') or []:
        key = step_key(step)
        if key is None or not live.get(key, False):
            continue
        link = [f'link:{link_numbers[key]}'] if key in link_numbers else []
        if step.get('trigger'):
            add('click', step['trigger']['outcomes'], extra=link)
        if step.get('trade'):
            add('trade', step['trade']['outcomes'], step['trade'].get('tries') or [], extra=link)
    for name, entry in sweeps_of.get(target['id'], []):
        if entry[0] != 'npc:setStatus' or len(entry) < 3:
            continue
        kind = 'shows' if entry[2] == 0 else 'hides'
        head, _, value = name.partition(' ')
        when = [f'hour:{value}-{value}'] if head == 'hour' else [f'weather:{value}'] if head == 'weather' else \
            [f'day:{value}'] if head == 'day' else ['zoneStart'] if name == 'initialize' else \
            [f"mob:{name.split(' ')[0]}"]
        act = {'act': kind, 'items': [], 'when': when, 'says': [], 'pops': [], 'gives': [], 'keyItems': [],
               'event': False}
        if act not in acts:
            acts.append(act)
    return merge_hours(acts)


def merge_hours(acts):
    """A ??? the clock shows or hides at several hours: one act each, its hours in ranges."""
    out, clock = [], {}
    for act in acts:
        if act['act'] in ('shows', 'hides') and len(act['when']) == 1 and act['when'][0].startswith('hour:'):
            clock.setdefault(act['act'], []).append(int(act['when'][0].split(':')[1].split('-')[0]))
        else:
            out.append(act)
    for kind, hours in sorted(clock.items()):
        out.append({'act': kind, 'items': [], 'when': [f'hour:{r}' for r in hour_ranges(hours)], 'says': [],
                    'pops': [], 'gives': [], 'keyItems': [], 'event': False})
    return out


def sweeps_by_npc(sweeps):
    """Each ???'s notes from the zone sweeps: (the sweep's run name, the note)."""
    found = {}
    for sweep in sweeps:
        for result in sweep.get('results') or []:
            for entry in result['done']:
                if len(entry) > 1 and isinstance(entry[1], int):
                    found.setdefault(entry[1], []).append((result['name'], entry))
    return found


def texts_in_ids(text: str) -> dict:
    """A zone IDs.lua's message texts: {text ID: the comment after it}, the game's own words."""
    return {int(m.group(1)): m.group(2).strip()
            for m in re.finditer(r'^\s*[A-Z0-9_]+\s*=\s*(\d+)\s*,\s*--\s*(.+?)\s*$', text, re.M)}
