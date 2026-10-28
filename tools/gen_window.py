#!/usr/bin/env python3
"""Generate src/generated/window_pieces.h and window_layout.inc from data/settings_window.json.

The file holds the settings window's words and their order: its pages, the cards on each, the settings in each card,
and the names and tips of options, rows and words. What each key is belongs to the code (PIECES): the file orders
and words them, and is refused, by the entry that does not fit, where it asks for what the code cannot draw.
--format writes the file again in its usual layout."""
import dataclasses
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gen_mobdata import c_string  # noqa: E402

KINDS = ('mobs', 'players', 'self', 'npcs', 'pets')  # PlateKind's order; also the entity pages' keys
KEY = re.compile(r'[a-z0-9]+(?:-[a-z0-9]+)*\Z')
ID_MARK = '##'  # what a label holds before the name of its control's id
SOURCE = 'settings_window.json'
FIELDS = ('version', 'key', 'name', 'child', 'indent', 'description', 'tip', 'tipOnClaimRows', 'tipBottomLine', 'kinds', 'options',
          'rows', 'settings', 'cards', 'pages', 'entity', 'lists', 'words')  # the order --format writes them in


class DataError(ValueError):
    pass


@dataclasses.dataclass(frozen=True)
class List:
    options: int
    nameless: frozenset = frozenset()
    extra: dict = dataclasses.field(default_factory=dict)  # an option (from 0) with a second tip: that tip's field


@dataclasses.dataclass(frozen=True)
class Card:
    switch: bool = False  # a switch in its title, the one control of its setting: the card cannot go


@dataclasses.dataclass(frozen=True)
class Setting:
    entity: bool = False  # part of an entity's design: drawn for the kind of the page it is on
    kinds: tuple = None  # the kinds the code has it on, where not every one
    unnamed: bool = False  # a whole piece with no label of its own
    options: int = 0
    nameless: frozenset = frozenset()  # options that show a shape or a name of their own
    rows: dict = None  # its rows by key in the code's order, each with the kinds the code has it on or None
    rows_fixed: bool = False  # its rows come in the code's order
    row_extra: dict = dataclasses.field(default_factory=dict)  # a row with a second tip: that tip's field
    words: dict = dataclasses.field(default_factory=dict)  # its words by key: whether each has a name


@dataclasses.dataclass(frozen=True)
class Pieces:
    pages: dict  # key: the Page enumerator
    cards: dict
    settings: dict
    lists: dict
    words: dict  # key: whether it has a name


@dataclasses.dataclass
class Entry:
    """A page, card, setting, row, option or word as the file gives it."""
    key: str = None
    name: object = None  # text, text by kind, or None
    tip: object = None
    extra: str = None  # a second tip
    child: bool = False
    indent: bool = False
    description: str = None
    kinds: tuple = None
    piece: str = None  # a card's key in the code; None for a card of the file's own
    options: list = dataclasses.field(default_factory=list)
    rows: list = dataclasses.field(default_factory=list)
    words: list = dataclasses.field(default_factory=list)
    settings: list = dataclasses.field(default_factory=list)
    cards: list = dataclasses.field(default_factory=list)


@dataclasses.dataclass
class Layout:
    pages: list
    entity: list
    lists: list
    words: list
    version: str = None  # the plugin's version the file was made for


def rows(*keys, kinds=None):
    return {key: kinds for key in keys}


VISIBILITY_ROWS = {**rows('aggressive', 'passive', 'nm-aggressive', 'nm-passive', 'nm-placeholder', 'claimed-yours',
                          'claimed-others', 'dead', 'no-data', kinds=('mobs',)),
                   **rows('players', 'party-members', 'friends', 'linkshell-members', kinds=('players',)),
                   'self': ('self',), 'npcs': ('npcs',),
                   **rows('your-pets', 'party-pets', 'friend-pets', 'others-pets', kinds=('pets',))}

PIECES = Pieces(
    pages={'entities': 'Entities', 'mobs': 'Mobs', 'players': 'Players', 'self': 'Self', 'npcs': 'Npcs', 'pets': 'Pets',
           'userinterface': 'UserInterface', 'toppanel': 'TopPanel', 'widgets': 'Widgets', 'mousecursor': 'MouseCursor',
           'configmenu': 'ConfigMenu'},
    cards={
        'nameplates': Card(switch=True), 'outline-style': Card(switch=True), 'target-cursor': Card(),
        'colors-aggression': Card(), 'colors-toughness': Card(), 'colors-claim-status': Card(),
        'colors-targeting': Card(), 'colors-entity-type': Card(),
        'visibility': Card(), 'outline-design': Card(), 'nameplate-design': Card(), 'line-style': Card(),
        'ornament': Card(), 'nameplate-opacity': Card(), 'detection-icons': Card(), 'badges': Card(),
        'weaknesses': Card(), 'statuses': Card(), 'placeholder-timers': Card(),
        'screenshot-mode': Card(), 'window-groups': Card(), 'base-game-ui': Card(),
        'top-panel': Card(switch=True), 'theme-colors': Card(), 'help-bar': Card(switch=True), 'widgets': Card(), 'layout': Card(),
        'mouse-pointer': Card(), 'settings-window': Card(), 'reset': Card(), 'debug-mode': Card(switch=True),
    },
    settings={
        # Entities
        'base-font-size': Setting(), 'default-font': Setting(), 'base-icon-size': Setting(), 'linkpearl-outline': Setting(),
        'auto-check': Setting(), 'look-up-players': Setting(),
        'replace-dashes': Setting(options=4, nameless=frozenset({1, 2, 3})), 'scale-with-distance': Setting(),
        'min-size': Setting(), 'max-size': Setting(), 'stack-line-to-entity': Setting(), 'vertical-offset': Setting(),
        'stack-names': Setting(), 'hide-during-conversation': Setting(),
        'thickness': Setting(), 'opacity': Setting(), 'smoothness': Setting(), 'outline-range': Setting(),
        'outline-fade-in': Setting(), 'stencil-note': Setting(),
        'cursor-style': Setting(options=5, nameless=frozenset({4})), 'width': Setting(), 'cursor-height': Setting(),
        'outline-thickness': Setting(), 'scale': Setting(), 'float': Setting(), 'cursor-vertical-offset': Setting(),
        'color': Setting(), 'edge-color': Setting(),
        'aggression-colors': Setting(unnamed=True, rows=rows(
            'passive', 'aggressive', 'nm-passive', 'nm-aggressive', 'nm-placeholder', 'no-data')),
        'toughness-colors': Setting(unnamed=True, rows=rows(
            'unknown-level', 'too-weak', 'easy-prey', 'decent-challenge', 'even-match', 'tough', 'very-tough',
            'incredibly-tough')),
        'claim-colors': Setting(unnamed=True, rows=rows('unclaimed', 'claimed-others', 'claimed-yours', 'called-for-help')),
        'targeting-colors': Setting(unnamed=True, rows=rows(
            'not-targeted', 'targeted', 'locked-on', 'sub-target', 'out-of-range')),
        'entity-type-colors': Setting(unnamed=True, rows=rows(
            'mob', 'player', 'friend', 'party-member', 'linkshell-member', 'npc-or-object', 'pet')),
        # The entity pages
        'visibility': Setting(entity=True, unnamed=True, rows=VISIBILITY_ROWS,
                              words={'nameplates': True, 'outlines': True}),
        'outline-colors': Setting(entity=True, unnamed=True),
        'design': Setting(entity=True, unnamed=True, words={'move-up': False, 'move-down': False}),
        'max-icons-shown': Setting(entity=True, kinds=('players', 'self'), words={'all': True}),
        'unused-tags': Setting(entity=True, unnamed=True),
        'line-styles': Setting(entity=True, unnamed=True, rows=rows(
            'sync-line-style', 'line-visibility', 'alignment', 'font', 'font-style', 'text-size', 'icon-size', 'padding',
            'horizontal-offset', 'text-color', 'shadow-strength', 'shadow-color', 'glow-strength', 'glow-size',
            'glow-color', 'disable-glow-in-combat'), row_extra={'padding': 'tipBottomLine'},
            words={'do-not-sync': True, 'sync-with': True, 'uses-style': True}),
        'ornament': Setting(entity=True, unnamed=True, rows=rows('visibility', 'width', 'height', 'color')),
        'no-target': Setting(), 'not-targeted': Setting(), 'is-targeted': Setting(), 'fade-in': Setting(),
        'range': Setting(),
        'detection': Setting(unnamed=True, rows_fixed=True, rows=rows(
            'aggression', 'links', 'sight', 'true-sight', 'sound', 'scent', 'use-of-spells', 'use-of-abilities',
            'low-player-hp'),
            words={'senses': True, 'hide': True, 'show': True}),
        'badge-font': Setting(), 'badge-numbers': Setting(options=3), 'count-your-accuracy': Setting(),
        'weak-color': Setting(), 'resist-color': Setting(),
        'status-resist-color': Setting(), 'immune-color': Setting(),
        'weak-from': Setting(), 'resist-from': Setting(), 'max-elements': Setting(),
        'status-table': Setting(unnamed=True, rows=rows(
            'paralyze', 'bind', 'silence', 'slow', 'poison', 'light-sleep', 'dark-sleep', 'blind', 'stun', 'gravity'),
            words={'weak': True, 'resist': True}),
        'respawn-timers': Setting(), 'timer-size': Setting(), 'up-time': Setting(),
        # User Interface
        'key': Setting(), 'hide-game-windows': Setting(),
        'groups': Setting(unnamed=True, words={'note': True, 'help-on': True, 'help-off': True, 'starts-at': True,
                                               'row-tip': False}),
        'game-help': Setting(), 'auto-adjust': Setting(),
        'window-defaults': Setting(unnamed=True, words={'headsup-defaults': True, 'headsup-help': True,
                                                        'game-defaults': True}),
        'window-list': Setting(unnamed=True, words={'note': True, 'find': True, 'name-hint': True, 'all-windows': False,
                                                    'hide': True, 'keep': True, 'row-tip': False}),
        # Top Panel and Widgets
        'theme': Setting(), 'window-type': Setting(options=9), 'window-border': Setting(), 'separators': Setting(options=3), 'title-font': Setting(), 'text-font': Setting(),
        'height': Setting(), 'text-size': Setting(), 'one-line': Setting(options=3), 'popup-size': Setting(),
        'hide-readout': Setting(), 'shift-vanilla-ui': Setting(),
        'theme-shape': Setting(options=4), 'theme-background': Setting(), 'theme-gradient': Setting(),
        'theme-top': Setting(), 'theme-bottom': Setting(), 'theme-border-color': Setting(),
        'theme-border-thickness': Setting(),
        'theme-palette': Setting(unnamed=True, rows=rows(
            'under-line', 'glow-below', 'accent', 'accent-end', 'pill', 'pill-border', 'title', 'subtitle', 'divider',
            'panel-top', 'panel-bottom', 'panel-border', 'panel-outer', 'text', 'heading', 'tag-fill', 'tag-border',
            'tag-text', 'highlight')),
        'theme-files': Setting(unnamed=True, words={'save-as': True, 'name-hint': True, 'save': True, 'load': True}),
        'hide-game-help': Setting(), 'position': Setting(), 'background': Setting(),
        'widget-tabs': Setting(unnamed=True, rows=rows(
            'enabled', 'section', 'copies', 'second-line', 'icons', 'spacing', 'padding', 'title-font', 'text-font', 'text-size', 'details', 'border', 'jobs', 'currency', 'bag', 'nations', 'shows', 'ship', 'element', 'guild', 'popup', 'time-zone', 'server-time', '24-hour',
            'seconds', 'layout', 'exp-text', 'exp-bar', 'exp-earned', 'exp-to-next', 'exp-percent', 'exp-align', 'bar-width', 'bar-height',
            'job-names', 'percent', 'latency'), words={'add-copy': True, 'remove-copy': True}),
        'trays': Setting(unnamed=True), 'reset-layout': Setting(),
        # Mouse Cursor and HeadsUp Config
        'pointer-icon': Setting(options=3, nameless=frozenset({2})), 'pointer-size': Setting(), 'pointer-fix': Setting(), 'keep-pointer': Setting(),
        'enable-headsup': Setting(), 'font': Setting(), 'size': Setting(),
        'window-opacity': Setting(),
        'reset-all-settings': Setting(words={'note': True, 'reset-everything': True, 'keep-them': True}),
        'debug-report': Setting(words={'watch': True, 'stop': True, 'saved': True}),
        'logging': Setting(words={'file': True}),
        'mobs-outlined': Setting(), 'meshes-outlined': Setting(), 'stencil-buffer': Setting(), 'nameplates': Setting(),
        'drawn': Setting(), 'game-s-names': Setting(), 'frame-time': Setting(), 'last-save': Setting(),
        'your-level': Setting(),
    },
    lists={
        'plate-mode': List(4), 'outline-mode': List(4, extra={3: 'tipOnClaimRows'}), 'color-source': List(7),
        'show-condition': List(13), 'ornament-place': List(5), 'align': List(3), 'font-style': List(3),
        'smoothness': List(4), 'bar-theme': List(7), 'bar-sections': List(3),
        'bar-widgets': List(18), 'plate-lines': List(3), 'job-layout': List(3), 'exp-bar': List(3), 'exp-text': List(3), 'exp-place': List(5), 'nations': List(4), 'world-line': List(5), 'world-sections': List(3), 'tags': List(24),
    },
    words={'switched-off': True, 'switch-off': True, 'switch-on': True, 'switched-off-top-panel': True,
           'switched-off-headsup': True, 'key-add': True, 'key-waiting': True, 'always': True, 'never': True, 'conditions': True, 'local': True, 'themes-font': True,
           'default-font': True, 'unused-tags-these': True, 'unused-tags-name': True, 'or': True, 'slot-tip': False,
           'reload': True, 'export': True},
)


def kind_bits(kinds) -> int:
    return sum(1 << KINDS.index(kind) for kind in kinds)


def pascal(key: str) -> str:
    name = ''.join(part[:1].upper() + part[1:] for part in key.split('-'))
    return 'N' + name if name[:1].isdigit() else name


class Reader:
    def __init__(self, source: str, pieces: Pieces, version: str = None):
        self.source = source
        self.pieces = pieces
        self.version = version
        self.placed = {}  # setting key: the card it is in, for "used twice"
        self.seen_cards = set()

    def bad(self, message: str):
        raise DataError(f'{self.source}: {message}')

    def fields(self, value, what: str, allowed):
        if not isinstance(value, dict):
            self.bad(f'{what} must be an object')
        for field in value:
            if field not in allowed:
                self.bad(f'{what} has an unknown field "{field}"')

    def key(self, value, what: str) -> str:
        key = value.get('key')
        if key is None:
            self.bad(f'{what} has no key')
        if not isinstance(key, str) or not KEY.match(key):
            self.bad(f'the key "{key}" must be lower-case letters, digits and single hyphens')
        return key

    def text(self, value, field: str, owner: str, kinds=None):
        """A name or a tip: text, or where kinds are given, text by kind that covers them."""
        if field not in value:
            return None
        text = value[field]
        if isinstance(text, dict):
            if kinds is None:
                self.bad(f'the {self.field_name(field)} of {owner} is by kind, which only the entity cards have')
            for kind, part in text.items():
                if kind not in KINDS:
                    self.bad(f'the {self.field_name(field)} of {owner} names "{kind}", which is no kind')
                self.plain(part, field, owner)
            for kind in kinds:
                if kind not in text:
                    self.bad(f'the {self.field_name(field)} of {owner} names no text for "{kind}"')
            return {kind: text[kind] for kind in KINDS if kind in text}
        self.plain(text, field, owner)
        return text

    def plain(self, text, field: str, owner: str):
        if not isinstance(text, str) or not text:
            self.bad(f'the {self.field_name(field)} of {owner} must be text')
        if field == 'name' and ID_MARK in text:
            self.bad(f'the name of {owner} holds "{ID_MARK}"')

    @staticmethod
    def field_name(field: str) -> str:
        return {'tipOnClaimRows': 'claim rows\' tip', 'tipBottomLine': 'bottom line\'s tip'}.get(field, field)

    def kinds(self, value, owner: str):
        if 'kinds' not in value:
            return None
        kinds = value['kinds']
        if not isinstance(kinds, list) or not all(isinstance(kind, str) for kind in kinds):
            self.bad(f'the kinds of {owner} must be a list of kinds')
        for kind in kinds:
            if kind not in KINDS:
                self.bad(f'the kinds of {owner} name "{kind}", which is none')
        return tuple(kind for kind in KINDS if kind in kinds)

    def named(self, value, owner: str, has_name: bool, kinds=None):
        name = self.text(value, 'name', owner, kinds)
        if has_name and name is None:
            self.bad(f'{owner} has no name')
        return name

    def layout(self, top) -> Layout:
        if not isinstance(top, dict) or set(top) != {'version', 'pages', 'entity', 'lists', 'words'}:
            self.bad('the top level must be an object with version, pages, entity, lists and words')
        version = top['version']
        if not isinstance(version, str) or not version:
            self.bad('the version must be text')
        if self.version is not None and version != self.version:
            self.bad(f'version {version} is not the plugin\'s {self.version}: run tools/gen_window.py --format')
        self.fields(top['entity'], 'the entity part', ('cards',))
        pages = self.pages(top['pages'])
        entity = self.cards(top['entity'].get('cards'), 'the entity cards', in_entity=True)
        self.left_out()
        return Layout(pages, entity, self.lists(top['lists']), self.window_words(top['words']), version)

    def listed(self, value, what: str) -> list:
        if not isinstance(value, list):
            self.bad(f'{what} must be a list')
        return value

    def pages(self, value) -> list:
        pages, seen = [], set()
        for number, page in enumerate(self.listed(value, 'pages'), start=1):
            if isinstance(page, dict) and 'key' not in page:
                self.bad(f'page {number} has no key')
            self.fields(page, f'page {number}', ('key', 'name', 'child', 'cards'))
            key = self.key(page, f'page {number}')
            owner = f'the page "{key}"'
            if key not in self.pieces.pages:
                self.bad(f'"{key}" is no page')
            if key in seen:
                self.bad(f'{owner} is used twice')
            seen.add(key)
            child = page.get('child', False)
            if not isinstance(child, bool):
                self.bad(f'the child of {owner} must be true or false')
            entry = Entry(key=key, name=self.named(page, owner, True), child=child)
            if key in KINDS:
                if 'cards' in page:
                    self.bad(f'{owner} shares the entity cards and cannot have cards of its own')
            else:
                entry.cards = self.cards(page.get('cards'), owner, in_entity=False, page=key)
                if not entry.cards:
                    self.bad(f'{owner} has no cards')
            pages.append(entry)
        for key in self.pieces.pages:
            if key not in seen:
                self.bad(f'the page "{key}" is left out')
        return pages

    def cards(self, value, where: str, in_entity: bool, page: str = None) -> list:
        if value is None:
            return []
        cards = []
        for number, card in enumerate(self.listed(value, f'the cards of {where}'), start=1):
            if isinstance(card, dict) and 'key' not in card:
                self.bad(f'card {number} of {where} has no key')
            key = card.get('key') if isinstance(card, dict) else None
            self.fields(card, f'the card "{key}"', ('key', 'name', 'description', 'kinds', 'settings'))
            key = self.key(card, f'card {number} of {where}')
            owner = f'the card "{key}"'
            if key in self.seen_cards:
                self.bad(f'{owner} is used twice')
            self.seen_cards.add(key)
            kinds = self.kinds(card, f'"{key}"')
            if kinds is not None and not in_entity:
                self.bad(f'"{key}" has kinds, which only the entity cards have')
            description = card.get('description')
            if description is not None:
                self.plain(description, 'description', owner)
            entry = Entry(key=key, name=self.named(card, owner, True), description=description, kinds=kinds,
                          piece=key if key in self.pieces.cards else None)
            place = f'card {key}, ' + (f'page {page}' if page else 'the entity cards')
            entry.settings = [self.setting(setting, key, place, in_entity, kinds)
                              for setting in self.listed(card.get('settings', []), f'the settings of {owner}')]
            if not entry.settings:
                self.bad(f'{owner} has no settings')
            cards.append(entry)
        return cards

    def setting(self, value, card: str, place: str, in_entity: bool, card_kinds) -> Entry:
        key = value.get('key') if isinstance(value, dict) else None
        if isinstance(value, dict) and key is None:
            self.bad(f'a setting of {place} has no key')
        known = self.pieces.settings.get(key)
        allowed = ['key', 'name', 'tip', 'kinds', 'indent']
        if known is not None:
            allowed += (['options'] if known.options else []) + (['rows'] if known.rows else []) + (
                ['words'] if known.words else [])
        self.fields(value, f'the setting "{key}"', allowed)
        key = self.key(value, f'a setting of {place}')
        if known is None:
            self.bad(f'"{key}" is no key of any setting ({place})')
        if key in self.placed:
            self.bad(f'the setting "{key}" is used twice (cards {self.placed[key]} and {card})')
        self.placed[key] = card
        owner = f'"{key}"'
        if known.entity and not in_entity:
            self.bad(f'{owner} is an entity\'s design and can only go in the entity cards ({place})')
        kinds = self.kinds(value, owner)
        if kinds is not None and not in_entity:
            self.bad(f'{owner} has kinds, which only the entity cards have')
        if known.kinds is not None and kinds != known.kinds:
            self.bad(f'{owner} must name the kinds the code has it on: {", ".join(known.kinds)}')
        shown = None
        if in_entity:
            shown = tuple(kind for kind in KINDS if (card_kinds is None or kind in card_kinds) and (
                kinds is None or kind in kinds))
            if not shown:
                self.bad(f'{owner} shows on no page: the kinds of its card leave none of its own')
            if not known.entity and len(shown) != 1:
                self.bad(f'{owner} would show on {len(shown)} pages: a setting that is not of an entity\'s design needs '
                         'one kind in the entity cards, on it or on its card')
        indent = value.get('indent', False)
        if not isinstance(indent, bool):
            self.bad(f'the indent of {owner} must be true or false')
        entry = Entry(key=key, kinds=kinds, indent=indent, tip=self.text(value, 'tip', owner, shown))
        entry.name = self.named(value, f'the setting "{key}"', not known.unnamed, shown)
        if known.unnamed and entry.name is not None:
            self.bad(f'the setting "{key}" has a name, and the code shows none')
        entry.options = self.options(value.get('options', []), owner, known.options, known.nameless, {})
        if known.rows:
            entry.rows = self.rows(value.get('rows', []), key, known, shown)
        if known.words:
            entry.words = self.words(value.get('words', []), known.words, f' of "{key}"')
        return entry

    def options(self, value, owner: str, count: int, nameless, extra) -> list:
        listed = self.listed(value, f'the options of {owner}')
        if len(listed) != count:
            self.bad(f'{owner} lists {len(listed)} options, and the control has {count}')
        options = []
        for index, option in enumerate(listed):
            what = f'option {index + 1} of {owner}'
            if isinstance(option, dict):
                for field in option:
                    if field in extra.values() and extra.get(index) != field:
                        holder = next(i for i, f in extra.items() if f == field)
                        self.bad(f'"{field}" belongs only to option {holder + 1} of {owner}')
            self.fields(option, what, ('name', 'tip') + ((extra[index],) if index in extra else ()))
            name = self.named(option, what, index not in nameless)
            if index in nameless and name is not None:
                self.bad(f'{what} has a name, and the control shows its own')
            options.append(Entry(name=name, tip=self.text(option, 'tip', what),
                                 extra=self.text(option, extra[index], what) if index in extra else None))
        return options

    def rows(self, value, setting: str, known: Setting, shown) -> list:
        rows, seen = [], set()
        for number, row in enumerate(self.listed(value, f'the rows of "{setting}"'), start=1):
            key = row.get('key') if isinstance(row, dict) else None
            extra = known.row_extra.get(key)
            self.fields(row, f'the row "{key}" of "{setting}"', ('key', 'name', 'tip', 'kinds') + ((extra,) if extra else ()))
            key = self.key(row, f'row {number} of "{setting}"')
            owner = f'the row "{key}" of "{setting}"'
            if key not in known.rows:
                self.bad(f'"{key}" is no row of "{setting}"')
            if key in seen:
                self.bad(f'{owner} is used twice')
            seen.add(key)
            kinds = self.kinds(row, owner)
            if known.rows[key] is None and kinds is not None:
                self.bad(f'{owner} has kinds, and the code has it on every kind its setting is on')
            if known.rows[key] is not None and kinds != known.rows[key]:
                self.bad(f'{owner} must name the kinds the code has it on: {", ".join(known.rows[key])}')
            row_shown = None if shown is None else tuple(k for k in shown if kinds is None or k in kinds)
            entry = Entry(key=key, kinds=kinds, name=self.named(row, owner, True, row_shown),
                          tip=self.text(row, 'tip', owner, row_shown),
                          extra=self.text(row, extra, owner, row_shown) if extra else None)
            rows.append(entry)
        for key in known.rows:
            if key not in seen:
                self.bad(f'the row "{key}" of "{setting}" is left out')
        if known.rows_fixed and [row.key for row in rows] != list(known.rows):
            self.bad(f'the rows of "{setting}" come in the code\'s order: {", ".join(known.rows)}')
        return rows

    def words(self, value, known: dict, of: str) -> list:
        words, seen = [], set()
        for number, word in enumerate(self.listed(value, f'the words{of}'), start=1):
            key = word.get('key') if isinstance(word, dict) else None
            self.fields(word, f'the word "{key}"{of}', ('key', 'name', 'tip'))
            key = self.key(word, f'word {number}{of}')
            owner = f'the word "{key}"{of}'
            if key not in known:
                self.bad(f'"{key}" is no word{of}')
            if key in seen:
                self.bad(f'{owner} is used twice')
            seen.add(key)
            name = self.named(word, owner, known[key])
            if not known[key] and name is not None:
                self.bad(f'{owner} has a name, and the code shows only its tip')
            words.append(Entry(key=key, name=name, tip=self.text(word, 'tip', owner)))
        for key in known:
            if key not in seen:
                self.bad(f'the word "{key}"{of} is left out')
        return words

    def lists(self, value) -> list:
        lists, seen = [], set()
        for number, listed in enumerate(self.listed(value, 'lists'), start=1):
            key = listed.get('key') if isinstance(listed, dict) else None
            self.fields(listed, f'the list "{key}"', ('key', 'options'))
            key = self.key(listed, f'list {number}')
            if key not in self.pieces.lists:
                self.bad(f'"{key}" is no list')
            if key in seen:
                self.bad(f'the list "{key}" is used twice')
            seen.add(key)
            known = self.pieces.lists[key]
            lists.append(Entry(key=key, options=self.options(listed.get('options', []), f'the list "{key}"',
                                                             known.options, known.nameless, known.extra)))
        for key in self.pieces.lists:
            if key not in seen:
                self.bad(f'the list "{key}" is left out')
        return lists

    def window_words(self, value) -> list:
        return self.words(value, self.pieces.words, '')

    def left_out(self):
        for key in self.pieces.settings:
            if key not in self.placed:
                self.bad(f'the setting "{key}" is left out')
        for key, card in self.pieces.cards.items():
            if card.switch and key not in self.seen_cards:
                self.bad(f'the card "{key}" is left out: its switch is the only one of its setting')


def parse(text: str, source: str = SOURCE, pieces: Pieces = PIECES, version: str = None) -> Layout:
    """The file's layout; with a version, the file must be of it."""
    try:
        top = json.loads(text)
    except json.JSONDecodeError as error:
        raise DataError(f'{source}: not JSON ({error})') from None
    return Reader(source, pieces, version).layout(top)


PLUGIN_VERSION = re.compile(r'constexpr double kVersion\s*=\s*([0-9.]+);')


def plugin_version(source: str) -> str:
    """kVersion as src/plugin.cpp writes it."""
    found = PLUGIN_VERSION.search(source)
    if found is None:
        raise DataError('src/plugin.cpp: no kVersion')
    return found.group(1)


def c_text(text) -> str:
    return 'nullptr' if text is None else c_string(text)


def generate(layout: Layout, pieces: Pieces = PIECES, text: str = ''):
    """The enums of every piece the code draws, and the file's tables in its order with the file itself."""
    with_rows = [key for key, setting in pieces.settings.items() if setting.rows]
    words = [(pascal(key), key) for key in pieces.words] + [
        (pascal(setting) + pascal(word), word) for setting in pieces.settings for word in pieces.settings[setting].words]
    header = ['// Generated by tools/gen_window.py from data/settings_window.json. Do not edit.', '#pragma once', '',
              '#include <cstdint>', '', 'namespace headsup', '{']

    def enum(name, kind, values):
        header.extend([f'enum class {name} : {kind}', '{'] + [f'    {value},' for value in values] + ['};'])

    enum('CardPiece', 'uint8_t', ['Plain'] + [pascal(key) for key in pieces.cards])
    enum('SettingPiece', 'uint16_t', [pascal(key) for key in pieces.settings])
    header.append(f'constexpr int kSettingPieceCount = {len(pieces.settings)};')
    enum('WindowList', 'uint8_t', [pascal(key) for key in pieces.lists])
    enum('WindowWord', 'uint16_t', [name for name, _ in words])
    for setting in with_rows:
        enum(pascal(setting) + 'Row', 'uint16_t', [pascal(row) for row in pieces.settings[setting].rows])
    header += ['}', '']

    by_kind = []  # the arrays of text by kind

    def text_by_kind(text):
        if not isinstance(text, dict):
            return 'nullptr'
        by_kind.append('{' + ', '.join(c_text(text.get(kind)) for kind in KINDS) + '}')
        return f'kByKind{len(by_kind) - 1}'

    def shared(text):
        return 'nullptr' if isinstance(text, dict) else c_text(text)

    def bits(kinds):
        return 'kEveryKind' if kinds is None else str(kind_bits(kinds))

    options, rows, settings, cards, pages, lists, layout_words = [], [], [], [], [], [], []

    def add_options(entries):
        first = len(options)
        options.extend(f'    {{{c_text(o.name)}, {c_text(o.tip)}, {c_text(o.extra)}}},' for o in entries)
        return first

    for listed in layout.lists:
        first = add_options(listed.options)
        lists.append(f'    {{WindowList::{pascal(listed.key)}, "{listed.key}", {first}, {len(listed.options)}}},')
    all_cards = [card for page in layout.pages for card in page.cards] + layout.entity
    for card in all_cards:
        switched = card.piece is not None and pieces.cards[card.piece].switch
        cards.append(f'    {{CardPiece::{pascal(card.piece) if card.piece else "Plain"}, "{card.key}", '
                     f'{c_text(card.name)}, {c_text(card.description)}, {bits(card.kinds)}, {len(settings)}, '
                     f'{len(card.settings)}, {"true" if switched else "false"}}},')
        for s in card.settings:
            known = pieces.settings[s.key]
            first_option = add_options(s.options)
            first_row = len(rows)
            for row in s.rows:
                rows.append(f'    {{static_cast<uint16_t>({pascal(s.key)}Row::{pascal(row.key)}), "{row.key}", '
                            f'{shared(row.name)}, {shared(row.tip)}, {text_by_kind(row.name)}, {text_by_kind(row.tip)}, '
                            f'{c_text(row.extra)}, {bits(row.kinds)}}},')
            settings.append(f'    {{SettingPiece::{pascal(s.key)}, "{s.key}", {shared(s.name)}, {shared(s.tip)}, '
                            f'{text_by_kind(s.name)}, {text_by_kind(s.tip)}, {bits(s.kinds)}, '
                            f'{"true" if known.entity else "false"}, {first_option}, {len(s.options)}, {first_row}, '
                            f'{len(s.rows)}, {"true" if known.rows_fixed else "false"}, {bits(known.kinds)}, '
                            f'{"true" if s.indent else "false"}}},')
            for word in s.words:
                layout_words.append(f'    {{WindowWord::{pascal(s.key) + pascal(word.key)}, {c_text(word.name)}, '
                                    f'{c_text(word.tip)}, "{word.key}", "{s.key}"}},')
    entity_first = len(cards) - len(layout.entity)
    for page in layout.pages:
        first, count = (entity_first, len(layout.entity)) if page.key in KINDS else (
            sum(len(p.cards) for p in layout.pages[:layout.pages.index(page)]), len(page.cards))
        pages.append(f'    {{Page::{pieces.pages[page.key]}, "{page.key}", {c_text(page.name)}, '
                     f'{"true" if page.child else "false"}, {first}, {count}}},')
    for word in layout.words:
        layout_words.append(f'    {{WindowWord::{pascal(word.key)}, {c_text(word.name)}, {c_text(word.tip)}, "{word.key}", '
                            'nullptr},')

    tables = ['// Generated by tools/gen_window.py from data/settings_window.json. Do not edit.']
    tables += [f'constexpr const char* kByKind{i}[] = {array};' for i, array in enumerate(by_kind)]
    for name, kind, lines in (('kLayoutOptions', 'LayoutOption', options), ('kLayoutRows', 'LayoutRow', rows),
                              ('kLayoutSettings', 'LayoutSetting', settings), ('kLayoutCards', 'LayoutCard', cards),
                              ('kLayoutPages', 'LayoutPage', pages), ('kLayoutLists', 'LayoutList', lists),
                              ('kLayoutWords', 'LayoutWord', layout_words)):
        tables += [f'constexpr {kind} {name}[] = {{'] + lines + ['};']
    tables += [f'constexpr const char* kLayoutVersion = {c_text(layout.version)};',
               f'constexpr const char* kLayoutText = {c_string(text)};']
    return '\n'.join(header) + '\n', '\n'.join(tables) + '\n'


def format_value(value, indent: int) -> str:
    pad, inner = '  ' * indent, '  ' * (indent + 1)

    def ordered(entry: dict):
        known = [field for field in FIELDS if field in entry] + [kind for kind in KINDS if kind in entry]
        return known + sorted(field for field in entry if field not in known)

    def leaf(item) -> bool:
        return not isinstance(item, (dict, list)) or (
            isinstance(item, list) and all(not isinstance(i, (dict, list)) for i in item))

    if isinstance(value, dict):
        fields = ordered(value)
        if all(leaf(value[field]) for field in fields):
            return '{' + ', '.join(f'{json.dumps(f)}: {json.dumps(value[f], ensure_ascii=False)}' for f in fields) + '}'
        lines = [f'{inner}{json.dumps(f)}: {format_value(value[f], indent + 1)}' for f in fields]
        return '{\n' + ',\n'.join(lines) + f'\n{pad}}}'
    if isinstance(value, list) and not leaf(value):
        return '[\n' + ',\n'.join(inner + format_value(item, indent + 1) for item in value) + f'\n{pad}]'
    return json.dumps(value, ensure_ascii=False)


def format_file(tree) -> str:
    return format_value(tree, 0) + '\n'


def main(argv) -> int:
    if len(argv) > 2 or (len(argv) == 2 and argv[1] != '--format'):
        print('usage: gen_window.py [--format]', file=sys.stderr)
        return 2
    root = pathlib.Path(__file__).resolve().parent.parent
    path = root / 'data' / 'settings_window.json'
    try:
        version = plugin_version((root / 'src' / 'plugin.cpp').read_text(encoding='utf-8'))
        text = path.read_text(encoding='utf-8')
        if len(argv) == 2:
            try:
                tree = json.loads(text)
            except json.JSONDecodeError as error:
                raise DataError(f'{SOURCE}: not JSON ({error})') from None
            if isinstance(tree, dict):
                tree['version'] = version
            path.write_text(format_file(tree), encoding='utf-8')
            text = path.read_text(encoding='utf-8')
        layout = parse(text, version=version)
    except (OSError, DataError) as error:
        print(f'gen_window: {error}', file=sys.stderr)
        return 1
    header, tables = generate(layout, PIECES, text)
    out = root / 'src' / 'generated'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'window_pieces.h').write_text(header, encoding='utf-8')
    (out / 'window_layout.inc').write_text(tables, encoding='utf-8')
    cards = sum(len(page.cards) for page in layout.pages) + len(layout.entity)
    print(f'window: {len(layout.pages)} pages, {cards} cards, {len(PIECES.settings)} settings -> {out}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
