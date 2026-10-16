#!/usr/bin/env python3
"""Turn the NPCs of headsup_dump JSON files into phoenix_npcs.tsv, phoenix_npc_shops.tsv, phoenix_npc_links.tsv,
phoenix_npc_systems.tsv and phoenix_key_items.tsv (tools/gen_npcs.py has the format).

Usage: npcs.py SERVER_DIR DUMP_JSON [DUMP_JSON ...] OUT_DIR

The dump says which NPCs live's settings leave on (each NPC's content tag names the ENABLE_ setting that turns it
off: an expansion, or a system like the Field Manuals), where each handler of their scripts and of the quests,
missions and battlefields that use them is defined (a module's override included), and the server's constant tables.
This reads those handlers
from the server's checkout (SERVER_DIR) for what they take in trade, give, check for, spawn and sell, and the shared
systems they call. It reads what a script spells out and nothing more: an item a handler works out while it runs is
left unread and named in the NPC's unread column. A quest or mission whose folder names an expansion live's settings
turn off is marked so, and so is one whose every step needs a setting live has off; what such a quest's steps do, and
what any step does whose own check needs a setting live has off, is left out of the NPC's row.
"""
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import gen_npcs as schema  # noqa: E402
from gen_mobdata import DataError  # noqa: E402

# The expansion a quest or mission folder (and the matching xi.quest.id or xi.mission.id key) belongs to; the city
# and hidden quest folders mix eras and name none.
QUEST_FOLDERS = {'abyssea': 'abyssea', 'adoulin': 'soa', 'coalition': 'soa', 'ahtUrhgan': 'toau', 'crystalWar': 'wotg'}
MISSION_FOLDERS = {'zilart': 'rotz', 'rotz': 'rotz', 'cop': 'cop', 'toau': 'toau', 'wotg': 'wotg', 'campaign': 'wotg',
                   'acp': 'acp', 'amk': 'amk', 'asa': 'asa', 'soa': 'soa', 'rov': 'rov', 'tvr': 'tvr'}
# Families of xi that are not a system an NPC uses: quests and missions are links, the rest constants.
NOT_SYSTEMS = {'quest', 'mission', 'settings', 'item', 'ki', 'keyItem', 'zone', 'effect', 'title', 'questLog',
               'questStatus', 'nation', 'job', 'skill', 'fameArea', 'weather', 'day', 'element', 'msg', 'status',
               'animation', 'mod', 'mobMod', 'race', 'slot', 'direction', 'emote'}
STOCK_SHOPS = {'general', 'nation'}  # shop calls whose second argument is the stock
LOWER_WORDS = {'of', 'the', 'and', 'a', 'an', 'in', 'on', 'to', 'for', 'with', 'from', 'at', 'by', 'into'}
UPPER_WORDS = {'ii', 'iii', 'iv', 'v', 'vi', 'vii', 'viii', 'ix', 'x', 'af1', 'af2', 'af3', 'dnc', 'sch', 'hp', 'mp'}
MOB_REF = re.compile(r'(\w*ID)\.mob\.([A-Za-z0-9_]+)(?:\s*\[\s*(\d+)\s*\])?(?:\s*\+\s*(\d+))?')


def title(identifier: str) -> str:
    """THE_ROAD_TO_DIVADOM -> The Road to Divadom; a file name keeps its own capitals (Storms_of_Fate)."""
    words = [w for w in identifier.split('_') if w]
    if not identifier.isupper():
        return ' '.join(words)
    out = []
    for n, word in enumerate(words):
        lower = word.lower()
        if lower in UPPER_WORDS:
            out.append(lower.upper())
        elif n and lower in LOWER_WORDS:
            out.append(lower)
        else:
            out.append(word.capitalize())
    return ' '.join(out)


def norm(name: str) -> str:
    return re.sub(r'[^a-z0-9]', '', name.lower())


def strip_comments(text: str) -> str:
    text = re.sub(r'--\[(=*)\[.*?\]\1\]', '', text, flags=re.S)
    out = []
    for line in text.splitlines():
        quote, cut = None, len(line)
        for i, c in enumerate(line):
            if quote:
                if c == quote and line[i - 1] != '\\':
                    quote = None
            elif c in '\'"':
                quote = c
            elif line.startswith('--', i):
                cut = i
                break
        out.append(line[:cut])
    return '\n'.join(out)


def args_of(text: str, at: int) -> str:
    """The text between the bracket opening at `at` and its match."""
    pairs = {'(': ')', '{': '}', '[': ']'}
    closing, depth, quote = pairs[text[at]], 0, None
    for i in range(at, len(text)):
        c = text[i]
        if quote:
            if c == quote and text[i - 1] != '\\':
                quote = None
        elif c in '\'"':
            quote = c
        elif c == text[at]:
            depth += 1
        elif c == closing:
            depth -= 1
            if depth == 0:
                return text[at + 1:i]
    return text[at + 1:]


def split_top(text: str) -> list:
    """Comma-separated parts at the top level of a call's arguments or a table's fields."""
    parts, depth, quote, start = [], 0, None, 0
    for i, c in enumerate(text):
        if quote:
            if c == quote and text[i - 1] != '\\':
                quote = None
        elif c in '\'"':
            quote = c
        elif c in '({[':
            depth += 1
        elif c in ')}]':
            depth -= 1
        elif c in ',;' and depth == 0:
            parts.append(text[start:i].strip())
            start = i + 1
    if text[start:].strip():
        parts.append(text[start:].strip())
    return parts


def value(text: str):
    """A Lua value as ('table', [positional], {named}), ('name', dotted), ('num', n), ('str', s) or ('expr', text)."""
    text = text.strip()
    if text.startswith('{') and args_of(text, 0) == text[1:-1]:
        positional, named = [], {}
        for field in split_top(text[1:-1]):
            key = re.match(r'([A-Za-z_][A-Za-z0-9_]*)\s*=(?!=)\s*(.*)$', field, re.S)
            index = re.match(r'\[\s*(\d+)\s*\]\s*=\s*(.*)$', field, re.S)
            if key:
                named[key.group(1)] = value(key.group(2))
            elif index:
                named[int(index.group(1))] = value(index.group(2))
            else:
                positional.append(value(field))
        return ('table', positional, named)
    if re.fullmatch(r'-?\d+', text):
        return ('num', int(text))
    if re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)+', text):
        return ('name', text)
    if re.fullmatch(r'"[^"]*"|\'[^\']*\'', text):
        return ('str', text[1:-1])
    return ('expr', text)


class Constants:
    """The dump's constant tables, and the live expansions from its settings."""

    def __init__(self, lua, settings):
        self.items, self.key_items = lua['items'], lua['keyItems']
        self.mobs = lua['mobs']
        self.quests, self.missions, self.battlefields = lua['quests'], lua['missions'], lua['battlefields']
        self.quest_logs, self.mission_logs = lua['questLogs'], lua['missionLogs']
        main = self.main = settings.get('main', {})
        restricted = main.get('RESTRICT_CONTENT') in (1, True)
        self.off_expansions = {e for e in schema.EXPANSIONS
                               if restricted and main.get('ENABLE_' + e.upper()) in (0, False)}
        self.item_ids = set(self.items.values())

    def item(self, v):
        """The item a value names: xi.item.X, or a number that is an item."""
        if v[0] == 'name' and v[1].startswith('xi.item.'):
            return self.items.get(v[1][len('xi.item.'):])
        if v[0] == 'num' and v[1] in self.item_ids:
            return v[1]
        return None

    def items_of(self, v) -> tuple:
        """(items, unread) of a trade or reward value: an item, {item, count}, or a table of those."""
        if v[0] == 'table':
            positional, named = v[1], v[2]
            if named.get('item') is not None:
                return self.items_of(named['item'])
            if len(positional) == 2 and positional[0][0] == 'str':
                return [], False  # { 'gil', 100 }
            if len(positional) == 2 and positional[1][0] == 'num' and self.item(positional[0]) is not None and \
                    not any(p[0] == 'table' for p in positional):
                return [self.item(positional[0])], False  # { item, count }
            found, unread = [], False
            for p in positional:
                got, bad = self.items_of(p)
                found += got
                unread |= bad
            return found, unread
        item = self.item(v)
        if item is not None:
            return [item], False
        return [], v[0] in ('name', 'expr')

    def key_items_in(self, text: str) -> list:
        return [self.key_items[n] for n in re.findall(r'xi\.(?:ki|keyItem)\.([A-Z0-9_]+)', text) if n in self.key_items]

    def mob_ids(self, text: str, zone: int):
        """(mob IDs, unread) of the mobs an argument names."""
        v = value(text)
        if v[0] == 'table':
            ids, unread = [], not v[1] or bool(v[2])
            for p in v[1]:
                got, bad = self.mob_ids(str(p[1]), zone) if p[0] in ('name', 'num', 'expr') else ([], True)
                ids += got
                unread |= bad
            return ids, unread
        if v[0] == 'num':
            return [v[1]], False
        ref = MOB_REF.fullmatch(text.strip())
        table = self.mobs.get(str(zone)) or {}
        if not ref or ref.group(2) not in table:
            return [], True
        found = table[ref.group(2)]
        if isinstance(found, dict):  # a keyed or nested table: its IDs, in no order an index or offset could use
            return (found['ids'], False) if not ref.group(3) and not ref.group(4) else ([], True)
        found = found if isinstance(found, list) else [found]
        if ref.group(3):
            index = int(ref.group(3)) - 1
            if not 0 <= index < len(found):
                return [], True
            found = [found[index]]
        offset = int(ref.group(4) or 0)
        return [i + offset for i in found], False


class Facts:
    def __init__(self):
        self.trades, self.gives, self.key_items, self.needs, self.pops = set(), set(), set(), set(), set()
        self.stock, self.systems, self.unread, self.legacy = None, set(), set(), set()
        self.fame = False  # whether its stock's prices move with fame

    def add(self, other) -> None:
        """What another handler does, beside this one's; its stock, when it has one, is the one kept."""
        for name in ('trades', 'gives', 'key_items', 'needs', 'pops', 'systems', 'unread', 'legacy'):
            getattr(self, name).update(getattr(other, name))
        if other.stock:
            self.stock, self.fame = other.stock, other.fame


CALL = re.compile(r'(npcUtil\.trade(?:Has|HasExactly|HasOnly|Matches)|trade:hasItemQty|npcUtil\.giveItem|'
                  r'player:addItem|npcUtil\.giveKeyItem|player:addKeyItem|player:hasKeyItem|npcUtil\.popFromQM|'
                  r'SpawnMob|GetMobByID|npcUtil\.completeQuest|npcUtil\.completeMission|'
                  r'xi\.([A-Za-z0-9]+)\.([A-Za-z0-9_]+))\s*\(')


def read_handler(text: str, whole_file: str, zone: int, c: Constants, facts: Facts, onTrade: bool) -> None:
    """What one handler's code does, into facts."""
    code = strip_comments(text)
    read_trade, read_system = False, False
    for m in CALL.finditer(code):
        call, family, function = m.group(1), m.group(2), m.group(3)
        args = split_top(args_of(code, m.end() - 1))
        if call.startswith('npcUtil.trade') or call == 'trade:hasItemQty':
            if len(args) > (0 if call == 'trade:hasItemQty' else 1):
                got, bad = c.items_of(value(args[0 if call == 'trade:hasItemQty' else 1]))
                facts.trades.update(got)
                read_trade |= bool(got) or not bad  # a trade of gil alone is read too
                if bad and not got:
                    facts.unread.add('trade')
        elif call in ('npcUtil.giveItem', 'player:addItem'):
            arg = args[1] if call == 'npcUtil.giveItem' and len(args) > 1 else (args[0] if args else '')
            got, bad = c.items_of(value(arg))
            facts.gives.update(got)
            if bad:
                facts.unread.add('gives')
        elif call in ('npcUtil.completeQuest', 'npcUtil.completeMission'):
            reward = value(args[3]) if len(args) > 3 else ('expr', '')
            if reward[0] == 'table':
                got, _ = c.items_of(reward[2]['item']) if 'item' in reward[2] else ([], False)
                facts.gives.update(got)
                key = re.search(r'\bkeyItem\s*=\s*(\{[^{}]*\}|xi\.(?:ki|keyItem)\.[A-Z0-9_]+)', args[3])
                facts.key_items.update(c.key_items_in(key.group(1)) if key else [])
        elif call in ('npcUtil.giveKeyItem', 'player:addKeyItem'):
            facts.key_items.update(c.key_items_in(','.join(args)))
        elif call == 'player:hasKeyItem':
            facts.needs.update(c.key_items_in(','.join(args)))
        elif call in ('npcUtil.popFromQM', 'SpawnMob', 'GetMobByID'):
            close = m.end() + len(args_of(code, m.end() - 1))  # the index of the call's closing bracket
            if call == 'GetMobByID' and not re.match(r'\)\s*:\s*spawn\s*\(', code[close:]):
                continue
            arg = args[2] if call == 'npcUtil.popFromQM' and len(args) > 2 else (args[0] if args else '')
            got, bad = c.mob_ids(arg, zone)
            facts.pops.update(got)
            if bad:
                facts.unread.add('pops')
        elif family == 'shop' and function in STOCK_SHOPS:
            stock = shop_stock(args[1] if len(args) > 1 else '', code, whole_file, c)
            if stock:
                # xi.shop.nation passes its nation as the fame area; xi.shop.general uses the one it is given.
                facts.stock = stock
                facts.fame = function == 'nation' or (len(args) > 2 and args[2].strip() != 'nil')
            else:
                facts.unread.add('shop')
        elif family and family not in NOT_SYSTEMS:
            facts.systems.add((family, f'{family}.{function}(' + re.sub(r'\s+', ' ', ', '.join(args)) + ')'))
            read_system = True
    for kind, pattern in (('quest', r'xi\.quest\.id\.([A-Za-z]+)\.([A-Z0-9_]+)'),
                          ('mission', r'xi\.mission\.id\.([A-Za-z]+)\.([A-Z0-9_]+)')):
        for area, name in re.findall(pattern, code):
            facts.legacy.add((kind, area, name))
    if onTrade and has_code(code) and not read_trade and not read_system:
        facts.unread.add('trade')


def has_code(code: str) -> bool:
    """Whether a function's text does anything between its parameters and its last end."""
    inner = re.sub(r'^.*?\bfunction\s*\([^)]*\)', '', code, count=1, flags=re.S).strip()
    return bool(re.sub(r'\bend\s*[,;]?\s*$', '', inner).strip())


def shop_stock(expr: str, code: str, whole_file: str, c: Constants):
    """The (item, price, rank) entries of a stock: a table, or a table a local of the handler or file holds."""
    v = value(expr)
    if v[0] != 'table' and re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', expr.strip()):
        for source in (code, strip_comments(whole_file)):
            m = re.search(r'(?:local\s+)?\b' + re.escape(expr.strip()) + r'\s*=\s*\{', source)
            if m:
                v = value('{' + args_of(source, m.end() - 1) + '}')
                break
    if v[0] != 'table' or not v[1]:
        return None
    entries = []
    for entry in v[1]:
        if entry[0] != 'table' or len(entry[1]) < 2 or entry[1][1][0] != 'num':
            return None
        item = c.item(entry[1][0])
        if item is None:
            return None
        rank = entry[1][2][1] if len(entry[1]) > 2 and entry[1][2][0] == 'num' and 1 <= entry[1][2][1] <= 3 else 0
        entries.append((item, entry[1][1][1], rank))
    return tuple(entries)


class Sources:
    def __init__(self, server):
        self.server, self.files = pathlib.Path(server), {}

    def text(self, path: str):
        if path not in self.files:
            p = self.server / path
            self.files[path] = p.read_text(encoding='utf-8', errors='replace') if p.is_file() else None
        return self.files[path]

    def span(self, where):
        """The code of a handler the dump placed at [file, first line, last line]."""
        path, first, last = where
        text = self.text(path)
        if text is None or first <= 0:
            return None, None
        return '\n'.join(text.splitlines()[first - 1:last]), text


CONTAINER_FOLDERS = ('scripts/quests/', 'scripts/missions/', 'scripts/battlefields/')


def container_file(entry):
    """The quest, mission or battlefield file an entry's check or handler is defined in, without .lua."""
    for where in (entry.get('check'), entry.get('handler')):
        if isinstance(where, list) and where[0].startswith(CONTAINER_FOLDERS) and where[0].endswith('.lua'):
            return where[0][:-len('.lua')]
    return None


def gated(check: str, c: Constants) -> bool:
    """Whether a step's check needs a setting live has off: a term of its return's top-level 'and' that is
    xi.settings.main.KEY == 1 (or true) for a KEY live has 0 (or false)."""
    code = strip_comments(check)
    m = re.search(r'\breturn\b(.*)', code, re.S)
    if not m:
        return False
    expression = re.sub(r'\bend\s*[,;]?\s*$', '', m.group(1).strip()).strip()
    depth, terms, start = 0, [], 0
    for i, ch in enumerate(expression):
        if ch in '({[':
            depth += 1
        elif ch in ')}]':
            depth -= 1
        elif depth == 0 and re.match(r'\bor\b', expression[i:]) and (i == 0 or not expression[i - 1].isalnum()):
            return False
        elif depth == 0 and re.match(r'\band\b', expression[i:]) and (i == 0 or not expression[i - 1].isalnum()):
            terms.append(expression[start:i])
            start = i + 3
    terms.append(expression[start:])
    for term in terms:
        need = re.fullmatch(r'\s*xi\.settings\.main\.([A-Z0-9_]+)\s*==\s*(1|true)\s*', term)
        if need and c.main.get(need.group(1)) in (0, False):
            return True
    return False


def link_of(container, c: Constants):
    """(key, row) of a container's link, or None for a kind gen_npcs has not."""
    cid, file = container.get('id') or '', container.get('file') or ''
    parts = file.split('/')
    folder = parts[2] if len(parts) > 3 else ''
    if container.get('quest') is not None:
        kind, log, ident = 'quest', container['log'], container['quest']
        expansion = QUEST_FOLDERS.get(folder, '')
        name = lookup_name(c.quests, c.quest_logs, log, ident)
    elif container.get('mission') is not None:
        kind, log, ident = 'mission', container['log'], container['mission']
        expansion = MISSION_FOLDERS.get(folder, '')
        name = lookup_name(c.missions, c.mission_logs, log, ident)
    elif cid.startswith('Battlefield['):
        kind, log, ident, expansion = 'battlefield', 0, int(re.search(r'\d+', cid).group(0)), ''
        name = next((n for n, i in c.battlefields.items() if i == ident), None)
    elif cid.startswith('HQuest['):
        kind, log, ident, expansion = 'hidden', 0, 0, ''
        name = container.get('name') or cid[len('HQuest['):-1]
    else:
        return None
    if not name:
        name = re.sub(r'^(?:\d+_)+', '', parts[-1]) if parts[-1] else cid
    key = (kind, log, ident, name if kind == 'hidden' else '')
    return key, {'kind': kind, 'log': log, 'id': ident, 'name': title(name), 'file': file, 'expansion': expansion}


def lookup_name(ids, logs, log, ident):
    for log_name, number in logs.items():
        if number == log:
            for area, names in ids.items():
                if norm(area) == norm(log_name):
                    return next((n for n, i in names.items() if i == ident), None)
    return None


def legacy_link(kind, area, name, c: Constants):
    ids, logs, folders = (c.quests, c.quest_logs, QUEST_FOLDERS) if kind == 'quest' else \
        (c.missions, c.mission_logs, MISSION_FOLDERS)
    if area not in ids or name not in ids[area]:
        return None
    log = next((n for k, n in logs.items() if norm(k) == norm(area)), None)
    if log is None:
        return None
    return (kind, log, ids[area][name], ''), {'kind': kind, 'log': log, 'id': ids[area][name], 'name': title(name),
                                              'file': '', 'expansion': folders.get(area, '')}


def rewards(file_text: str, c: Constants):
    """The items and key items a quest or mission file's reward table names."""
    code = strip_comments(file_text)
    m = re.search(r'\b(?:quest|mission)\.reward\s*=\s*\{', code)
    if not m:
        return [], []
    table = '{' + args_of(code, m.end() - 1) + '}'
    v = value(table)
    items = c.items_of(v[2]['item'])[0] if v[0] == 'table' and 'item' in v[2] else []
    key = re.search(r'\bkeyItem\s*=\s*(\{[^{}]*\}|xi\.(?:ki|keyItem)\.[A-Z0-9_]+)', table)
    return items, c.key_items_in(key.group(1)) if key else []


def load(paths):
    first = None
    for path in paths:
        try:
            dump = json.loads(pathlib.Path(path).read_text(encoding='utf-8'))
            npcs, lua, settings = dump['npcs'], dump['npcLua'], dump['settings']
        except (json.JSONDecodeError, KeyError, TypeError) as error:
            raise DataError(f'{path}: not a dump with NPCs this npcs.py reads ({error!r})') from error
        if first is None:
            first = (npcs, lua, settings)
        elif [n['id'] for n in npcs] != [n['id'] for n in first[0]] or lua != first[1]:
            raise DataError(f'{path}: its NPCs differ from those of {paths[0]}')
    return first


def build(npcs, lua, settings, server):
    c, sources = Constants(lua, settings), Sources(server)
    links, link_rows, shops, systems, rows = {}, [], {}, {}, []
    open_steps = {}  # link: whether any step an NPC takes part in can be open on live
    reward_cache = {}
    for npc in sorted(npcs, key=lambda n: n['id']):
        # What its own script does, and what each step of a quest, mission or battlefield it takes part in does:
        # (link, whether the step needs a setting live has off, facts), kept apart until every NPC says whether its
        # link is live.
        facts, steps, npc_links = Facts(), [], set()
        own = (lua['scripts'].get(npc['zoneName']) or {}).get(npc['script']) or {}
        handlers = [(name, where, None, False) for name, where in own.items()]
        for name, entries in ((lua['interactions'].get(str(npc['zone'])) or {}).get(npc['script']) or {}).items():
            for entry in entries:
                handler, container, step_off = entry['handler'], entry['container'], False
                if container and not container.get('file'):
                    # A module that rebuilds a container (xi.module.modifyInteractionEntry) leaves it no file name;
                    # its steps' checks are still the quest's own file's.
                    container = dict(container, file=container_file(entry))
                if container:
                    found = link_of(container, c)
                    if found:
                        check = sources.span(entry['check'])[0] if entry.get('check') else None
                        step_off = check is not None and gated(check, c)
                        open_steps.setdefault(found[0], False)
                        open_steps[found[0]] |= not step_off
                if isinstance(handler, list):
                    handlers.append((name, handler, container, step_off))
                elif container:
                    handlers.append((name, None, container, step_off))
        for name, where, container, step_off in handlers:
            key = None
            if container:
                found = link_of(container, c)
                if found:
                    key = found[0]
                    npc_links.add(key)
                    if key not in links or not links[key]['file']:
                        links[key] = found[1]  # the container's own row beats a script's mention
            if where is None:
                continue
            text, whole = sources.span(where)
            if text is None:
                continue
            into = facts if key is None else Facts()
            read_handler(text, whole, npc['zone'], c, into, name == 'onTrade')
            if container and container.get('file') and re.search(r'\b(?:quest|mission):complete\s*\(', text):
                file = container['file']
                if file not in reward_cache:
                    reward_cache[file] = rewards(sources.text(file + '.lua') or '', c)
                into.gives.update(reward_cache[file][0])
                into.key_items.update(reward_cache[file][1])
            if key is not None:
                steps.append((key, step_off, into))
        for kind, area, name in facts.legacy.union(*(step.legacy for _, _, step in steps)):
            found = legacy_link(kind, area, name, c)
            if found:
                npc_links.add(found[0])
                links.setdefault(found[0], found[1])
        rows.append((npc, facts, steps, npc_links))
    numbers, live = {}, {}
    for key in sorted(links, key=lambda k: (schema.LINK_KINDS.index(k[0]), k[1], k[2], k[3])):
        numbers[key] = len(numbers) + 1
        row = links[key]
        live[key] = row['live'] = int(row['expansion'] not in c.off_expansions and open_steps.get(key, True))
        link_rows.append(dict(row, link=numbers[key]))
    npc_rows = []
    for npc, facts, steps, npc_links in rows:
        # A step counts when its link is live and its own check needs nothing live has off: the trades, rewards and
        # pops of later content on an NPC that live has stay out.
        for key, step_off, step in steps:
            if live[key] and not step_off:
                facts.add(step)
        shop = shops.setdefault((facts.stock, facts.fame), len(shops) + 1) if facts.stock else 0
        npc_rows.append({'id': npc['id'], 'zone': npc['zone'], 'script': npc['script'], 'name': npc['name'],
                         'pos': '{:.3f},{:.3f},{:.3f},{}'.format(*npc['pos'][:3], int(npc['pos'][3])),
                         'trades': sorted(facts.trades), 'gives': sorted(facts.gives),
                         'keyItems': sorted(facts.key_items), 'needs': sorted(facts.needs), 'pops': sorted(facts.pops),
                         'shop': shop, 'links': sorted(numbers[k] for k in npc_links),
                         'systems': sorted(systems.setdefault(s, len(systems) + 1) for s in sorted(facts.systems)),
                         'unread': [k for k in schema.UNREAD if k in facts.unread]})
    system_rows = [{'system': n, 'family': family, 'call': call}
                   for (family, call), n in sorted(systems.items(), key=lambda kv: kv[1])]
    shop_rows = [(n, stock) for stock, n in sorted(shops.items(), key=lambda kv: kv[1])]
    identifiers = {}
    for identifier, key_item in sorted(c.key_items.items()):
        identifiers.setdefault(key_item, identifier)
    key_item_rows = {k: title(identifiers[k]) for k in sorted({k for r in npc_rows for k in r['keyItems'] + r['needs']})}
    return npc_rows, shop_rows, link_rows, system_rows, key_item_rows


def cell(v) -> str:
    if isinstance(v, list):
        return ','.join(str(x) for x in v)
    text = str(v)
    if '\t' in text or '\n' in text:
        raise DataError(f'a tab or newline in {text!r}')
    return text


def texts(npc_rows, shop_rows, link_rows, system_rows, key_item_rows):
    npcs = '\t'.join(schema.NPC_COLUMNS) + '\n' + ''.join(
        '\t'.join(cell(r[col]) for col in schema.NPC_COLUMNS) + '\n' for r in npc_rows)
    shops = '\t'.join(schema.SHOP_COLUMNS) + '\n' + ''.join(
        f'{n}\t{item}\t{price}\t{rank}\t{int(fame)}\n' for n, (stock, fame) in shop_rows for item, price, rank in stock)
    links = '\t'.join(schema.LINK_COLUMNS) + '\n' + ''.join(
        '\t'.join(cell(r[col]) for col in schema.LINK_COLUMNS) + '\n' for r in link_rows)
    systems = '\t'.join(schema.SYSTEM_COLUMNS) + '\n' + ''.join(
        '\t'.join(cell(r[col]) for col in schema.SYSTEM_COLUMNS) + '\n' for r in system_rows)
    key_items = '\t'.join(schema.KEY_ITEM_COLUMNS) + '\n' + ''.join(
        f'{k}\t{cell(name)}\n' for k, name in key_item_rows.items())
    return {schema.NPCS_NAME: npcs, schema.SHOPS_NAME: shops, schema.LINKS_NAME: links, schema.SYSTEMS_NAME: systems,
            schema.KEY_ITEMS_NAME: key_items}


def run(server, dump_paths, out_dir):
    npcs, lua, settings = load(dump_paths)
    files = texts(*build(npcs, lua, settings, server))
    tables = [schema.parse_npcs(files[schema.NPCS_NAME]), schema.parse_shops(files[schema.SHOPS_NAME]),
              schema.parse_links(files[schema.LINKS_NAME]), schema.parse_systems(files[schema.SYSTEMS_NAME]),
              schema.parse_key_items(files[schema.KEY_ITEMS_NAME])]
    schema.check(*tables)
    out_dir = pathlib.Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, text in files.items():
        (out_dir / name).write_text(text, encoding='utf-8')
    return tables


def main(argv) -> int:
    if len(argv) < 4:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    try:
        npcs, shops, links, systems, key_items = run(argv[1], argv[2:-1], argv[-1])
    except (OSError, DataError) as error:
        print(f'npcs: {error}', file=sys.stderr)
        return 1
    print(f'npcs: {len(npcs)} NPCs, {len(shops)} shops, {len(links)} links, {len(systems)} system calls, '
          f'{len(key_items)} key items')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
