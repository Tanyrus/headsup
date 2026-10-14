import copy
import json
import unittest

from tooling import ROOT, load

gen = load('tools/gen_window.py')

# What a small window's code draws: two pages of its own and the five entity pages.
PIECES = gen.Pieces(
    pages={'home': 'Home', 'mobs': 'Mobs', 'players': 'Players', 'self': 'Self', 'npcs': 'Npcs', 'pets': 'Pets',
           'tools': 'Tools'},
    cards={'general': gen.Card(switch=True), 'extras': gen.Card(), 'looks': gen.Card()},
    settings={
        'size': gen.Setting(),
        'style': gen.Setting(options=3, nameless=frozenset({2})),
        'speed': gen.Setting(),
        'badge': gen.Setting(),
        'rows': gen.Setting(entity=True, unnamed=True, rows={'aggressive': ('mobs',), 'everyone': ('players',)},
                            words={'head': True}),
        'lines': gen.Setting(entity=True, unnamed=True, rows={'font': None, 'padding': None},
                             row_extra={'padding': 'tipBottomLine'}),
        'palette': gen.Setting(unnamed=True, rows={'calm': None, 'angry': None}, rows_fixed=True),
        'max-icons': gen.Setting(entity=True, kinds=('players', 'self')),
    },
    lists={'mode': gen.List(2, extra={1: 'tipOnClaimRows'})},
    words={'always': True, 'every-window': False},
)


def layout():
    return {
        'pages': [
            {'key': 'home', 'name': 'Home', 'cards': [
                {'key': 'general', 'name': 'General', 'description': 'The whole of it.', 'settings': [
                    {'key': 'style', 'name': 'Style', 'options': [{'name': 'Plain'}, {'name': 'Bold', 'tip': 'Thick.'},
                                                                  {'tip': 'A shape.'}]},
                    {'key': 'speed', 'name': 'Speed'},
                ]},
                {'key': 'looks', 'name': 'Looks', 'settings': [{'key': 'palette', 'rows': [
                    {'key': 'calm', 'name': 'Calm'}, {'key': 'angry', 'name': 'Angry', 'tip': 'Red.'}]}]},
            ]},
            {'key': 'mobs', 'name': 'Mobs', 'child': True},
            {'key': 'players', 'name': 'Players', 'child': True},
            {'key': 'self', 'name': 'Self', 'child': True},
            {'key': 'npcs', 'name': 'NPCs', 'child': True},
            {'key': 'pets', 'name': 'Pets', 'child': True},
            {'key': 'tools', 'name': 'Tools', 'cards': [
                {'key': 'extras', 'name': 'Extras', 'settings': [{'key': 'size', 'name': 'Size', 'tip': 'How big.'}]},
            ]},
        ],
        'entity': {'cards': [
            {'key': 'design', 'name': 'Design', 'settings': [
                {'key': 'rows', 'rows': [
                    {'key': 'aggressive', 'name': 'Aggressive', 'kinds': ['mobs']},
                    {'key': 'everyone', 'name': 'Everyone', 'kinds': ['players']}],
                 'words': [{'key': 'head', 'name': 'Nameplates'}]},
                {'key': 'lines', 'rows': [
                    {'key': 'font', 'name': 'Font', 'tip': {'mobs': 'A mob\'s.', 'players': 'A player\'s.',
                                                            'self': 'Yours.', 'npcs': 'An NPC\'s.', 'pets': 'A pet\'s.'}},
                    {'key': 'padding', 'name': 'Padding', 'tip': 'Under.', 'tipBottomLine': 'Over.'}]},
                {'key': 'max-icons', 'name': 'Max Icons', 'kinds': ['players', 'self']},
            ]},
            {'key': 'mob-badges', 'name': 'Badges', 'kinds': ['mobs'], 'settings': [{'key': 'badge', 'name': 'Badge'}]},
        ]},
        'lists': [{'key': 'mode', 'options': [{'name': 'Off', 'tip': 'None.'},
                                              {'name': 'On', 'tip': 'Some.', 'tipOnClaimRows': 'Claimed.'}]}],
        'words': [{'key': 'always', 'name': 'Always', 'tip': 'Whatever holds.'}, {'key': 'every-window', 'tip': 'All.'}],
    }


def fixture():
    return layout()


def parse(tree, pieces=PIECES):
    return gen.parse(json.dumps(tree), 'w.json', pieces)


def settings_of(parsed, page, card):
    found = next(p for p in parsed.pages if p.key == page)
    return [s.key for s in next(c for c in found.cards if c.key == card).settings]


class Parse(unittest.TestCase):
    def test_pages_cards_and_settings_read_back_in_the_files_order(self):
        tree = fixture()
        tree['pages'][0]['cards'].reverse()
        tree['entity']['cards'][0]['settings'].reverse()
        tree['pages'].insert(0, tree['pages'].pop())
        parsed = parse(tree)
        self.assertEqual([p.key for p in parsed.pages][:2], ['tools', 'home'])
        self.assertEqual([c.key for c in parsed.pages[1].cards], ['looks', 'general'])
        self.assertEqual([s.key for s in parsed.entity[0].settings], ['max-icons', 'lines', 'rows'])

    def test_a_setting_may_move_to_any_card_of_any_page(self):
        tree = fixture()
        tree['pages'][0]['cards'][1]['settings'].append(tree['pages'][-1]['cards'][0]['settings'].pop())
        tree['pages'][-1]['cards'][0]['settings'].append(tree['pages'][0]['cards'][0]['settings'].pop())
        parsed = parse(tree)
        self.assertEqual(settings_of(parsed, 'home', 'looks'), ['palette', 'size'])
        self.assertEqual(settings_of(parsed, 'tools', 'extras'), ['speed'])

    def test_a_card_of_ones_own_holds_settings_and_one_without_a_switch_may_go(self):
        tree = fixture()
        looks = tree['pages'][0]['cards'].pop(1)
        tree['pages'][-1]['cards'].append({'key': 'colors', 'name': 'Colors', 'settings': looks['settings']})
        parsed = parse(tree)
        self.assertEqual(settings_of(parsed, 'tools', 'colors'), ['palette'])
        self.assertIsNone(parsed.pages[-1].cards[1].piece)

    def test_a_tip_by_kind_and_the_kinds_of_a_row(self):
        parsed = parse(fixture())
        lines = parsed.entity[0].settings[1]
        self.assertEqual(lines.rows[0].tip['self'], 'Yours.')
        self.assertEqual(parsed.entity[0].settings[0].rows[1].kinds, ('players',))

    def assertBad(self, message, change, pieces=PIECES):
        tree = fixture()
        change(tree)
        with self.assertRaises(gen.DataError) as caught:
            parse(tree, pieces)
        self.assertIn('w.json', str(caught.exception))
        self.assertIn(message, str(caught.exception))

    def test_text_that_is_no_json(self):
        with self.assertRaises(gen.DataError) as caught:
            gen.parse('{"pages": [', 'w.json', PIECES)
        self.assertIn('not JSON', str(caught.exception))

    def test_a_top_level_that_lacks_a_part(self):
        self.assertBad('the top level must be an object with pages, entity, lists and words',
                       lambda t: t.pop('lists'))

    def test_an_unknown_field_names_its_entry(self):
        self.assertBad('the setting "size" has an unknown field "tooltip"',
                       lambda t: t['pages'][-1]['cards'][0]['settings'][0].update(tooltip='x'))
        self.assertBad('the card "general" has an unknown field "about"',
                       lambda t: t['pages'][0]['cards'][0].update(about='x'))

    def test_an_entry_without_its_key_or_name(self):
        self.assertBad('page 2 has no key', lambda t: t['pages'][1].pop('key'))
        self.assertBad('the page "tools" has no name', lambda t: t['pages'][-1].pop('name'))
        self.assertBad('the card "looks" has no name', lambda t: t['pages'][0]['cards'][1].pop('name'))
        self.assertBad('the setting "size" has no name', lambda t: t['pages'][-1]['cards'][0]['settings'][0].pop('name'))
        self.assertBad('option 1 of "style" has no name',
                       lambda t: t['pages'][0]['cards'][0]['settings'][0]['options'][0].pop('name'))
        self.assertBad('option 2 of the list "mode" has no name', lambda t: t['lists'][0]['options'][1].pop('name'))
        self.assertBad('the word "always" has no name', lambda t: t['words'][0].pop('name'))
        self.assertBad('the row "calm" of "palette" has no name',
                       lambda t: t['pages'][0]['cards'][1]['settings'][0]['rows'][0].pop('name'))

    def test_a_name_or_a_tip_that_is_no_text(self):
        self.assertBad('the name of the setting "size" must be text', lambda t: t['pages'][-1]['cards'][0]['settings'][0].update(name=3))
        self.assertBad('the name of the setting "size" must be text', lambda t: t['pages'][-1]['cards'][0]['settings'][0].update(name=''))

    def test_a_key_that_is_no_slug(self):
        self.assertBad('the key "Size" must be lower-case letters, digits and single hyphens',
                       lambda t: t['pages'][-1]['cards'][0]['settings'][0].update(key='Size'))
        self.assertBad('the key "a--b" must be', lambda t: t['pages'][0]['cards'][1].update(key='a--b'))

    def test_a_key_the_code_does_not_have(self):
        self.assertBad('"sise" is no key of any setting (card extras, page tools)',
                       lambda t: t['pages'][-1]['cards'][0]['settings'][0].update(key='sise'))
        self.assertBad('"hime" is no page', lambda t: t['pages'][0].update(key='hime'))
        self.assertBad('"fonts" is no row of "lines"',
                       lambda t: t['entity']['cards'][0]['settings'][1]['rows'][0].update(key='fonts'))
        self.assertBad('"heads" is no word of "rows"',
                       lambda t: t['entity']['cards'][0]['settings'][0]['words'][0].update(key='heads'))
        self.assertBad('"modes" is no list', lambda t: t['lists'][0].update(key='modes'))
        self.assertBad('"never" is no word', lambda t: t['words'][0].update(key='never'))

    def test_a_key_used_twice(self):
        self.assertBad('the setting "size" is used twice (cards general and extras)',
                       lambda t: t['pages'][0]['cards'][0]['settings'].append({'key': 'size', 'name': 'Size'}))
        self.assertBad('the card "looks" is used twice',
                       lambda t: t['pages'][-1]['cards'].append(copy.deepcopy(t['pages'][0]['cards'][1])))
        self.assertBad('the page "tools" is used twice', lambda t: t['pages'].append(copy.deepcopy(t['pages'][-1])))
        self.assertBad('the row "font" of "lines" is used twice',
                       lambda t: t['entity']['cards'][0]['settings'][1]['rows'].append({'key': 'font', 'name': 'F'}))
        self.assertBad('the word "always" is used twice', lambda t: t['words'].append({'key': 'always', 'name': 'A'}))
        self.assertBad('the list "mode" is used twice', lambda t: t['lists'].append(copy.deepcopy(t['lists'][0])))

    def test_what_the_code_draws_and_the_file_leaves_out(self):
        self.assertBad('the setting "speed" is left out', lambda t: t['pages'][0]['cards'][0]['settings'].pop())
        self.assertBad('the page "tools" is left out', lambda t: t['pages'].pop())
        self.assertBad('the row "padding" of "lines" is left out',
                       lambda t: t['entity']['cards'][0]['settings'][1]['rows'].pop())
        self.assertBad('the word "head" of "rows" is left out',
                       lambda t: t['entity']['cards'][0]['settings'][0].pop('words'))
        self.assertBad('the word "every-window" is left out', lambda t: t['words'].pop())
        self.assertBad('the list "mode" is left out', lambda t: t['lists'].pop())

    def test_a_card_with_a_switch_in_its_title_stays(self):
        def move_out(t):
            general = t['pages'][0]['cards'].pop(0)
            t['pages'][0]['cards'][0]['settings'] += general['settings']
        self.assertBad('the card "general" is left out: its switch is the only one of its setting', move_out)

    def test_an_entitys_design_stays_in_the_entity_cards(self):
        self.assertBad('"lines" is an entity\'s design and can only go in the entity cards (card extras, page tools)',
                       lambda t: t['pages'][-1]['cards'][0]['settings'].append(t['entity']['cards'][0]['settings'].pop(1)))

    def test_the_kinds_of_a_setting(self):
        max_icons = lambda t: t['entity']['cards'][0]['settings'][2]
        self.assertBad('"max-icons" must name the kinds the code has it on: players, self',
                       lambda t: max_icons(t).update(kinds=['players']))
        self.assertBad('"max-icons" must name the kinds the code has it on: players, self',
                       lambda t: max_icons(t).pop('kinds'))
        self.assertBad('the kinds of "badge" name "cats", which is none',
                       lambda t: t['entity']['cards'][1]['settings'][0].update(kinds=['cats']))
        self.assertBad('"badge" would show on 5 pages: a setting that is not of an entity\'s design needs one kind '
                       'in the entity cards, on it or on its card', lambda t: t['entity']['cards'][1].pop('kinds'))
        self.assertBad('"size" has kinds, which only the entity cards have',
                       lambda t: t['pages'][-1]['cards'][0]['settings'][0].update(kinds=['mobs']))
        self.assertBad('"max-icons" shows on no page: the kinds of its card leave none of its own',
                       lambda t: t['entity']['cards'][0].update(kinds=['mobs']))
        self.assertBad('the row "aggressive" of "rows" must name the kinds the code has it on: mobs',
                       lambda t: t['entity']['cards'][0]['settings'][0]['rows'][0].update(kinds=['pets']))

    def test_a_badge_may_move_to_another_kind_or_out_of_the_entity_cards(self):
        tree = fixture()
        tree['entity']['cards'][1]['kinds'] = ['pets']
        self.assertEqual(parse(tree).entity[1].kinds, ('pets',))
        tree = fixture()
        tree['pages'][-1]['cards'][0]['settings'].append(tree['entity']['cards'].pop(1)['settings'][0])
        self.assertEqual(settings_of(parse(tree), 'tools', 'extras'), ['size', 'badge'])

    def test_options_as_many_as_the_control_has(self):
        self.assertBad('"style" lists 2 options, and the control has 3',
                       lambda t: t['pages'][0]['cards'][0]['settings'][0]['options'].pop())
        self.assertBad('the list "mode" lists 3 options, and the control has 2',
                       lambda t: t['lists'][0]['options'].append({'name': 'More'}))
        self.assertBad('option 3 of "style" has a name, and the control shows its own',
                       lambda t: t['pages'][0]['cards'][0]['settings'][0]['options'][2].update(name='Shape'))

    def test_a_page_or_a_card_with_nothing_in_it(self):
        self.assertBad('the page "home" has no cards', lambda t: t['pages'][0].update(cards=[]))
        self.assertBad('the card "looks" has no settings', lambda t: t['pages'][0]['cards'][1].update(settings=[]))
        self.assertBad('the page "mobs" shares the entity cards and cannot have cards of its own',
                       lambda t: t['pages'][1].update(cards=[]))

    def test_a_name_that_would_name_an_id(self):
        self.assertBad('the name of the setting "size" holds "##"',
                       lambda t: t['pages'][-1]['cards'][0]['settings'][0].update(name='Size##other'))

    def test_rows_the_code_draws_in_its_own_order(self):
        self.assertBad('the rows of "palette" come in the code\'s order: calm, angry',
                       lambda t: t['pages'][0]['cards'][1]['settings'][0]['rows'].reverse())

    def test_a_text_by_kind_covers_every_kind_it_shows_on(self):
        self.assertBad('the tip of the row "font" of "lines" names no text for "self"',
                       lambda t: t['entity']['cards'][0]['settings'][1]['rows'][0]['tip'].pop('self'))
        self.assertBad('the tip of "size" is by kind, which only the entity cards have',
                       lambda t: t['pages'][-1]['cards'][0]['settings'][0].update(tip={'mobs': 'x'}))

    def test_a_second_tip_only_where_the_code_has_one(self):
        self.assertBad('"tipOnClaimRows" belongs only to option 2 of the list "mode"',
                       lambda t: t['lists'][0]['options'][0].update(tipOnClaimRows='x'))
        self.assertBad('the row "font" of "lines" has an unknown field "tipBottomLine"',
                       lambda t: t['entity']['cards'][0]['settings'][1]['rows'][0].update(tipBottomLine='x'))


class Generate(unittest.TestCase):
    def test_tables_follow_the_files_order_and_name_every_piece(self):
        tree = fixture()
        tree['pages'][0]['cards'].reverse()
        header, tables = gen.generate(parse(tree), PIECES)
        self.assertIn('enum class SettingPiece : uint16_t\n{\n    Size,\n    Style,\n    Speed,\n    Badge,\n    Rows,', header)
        self.assertIn('constexpr int kSettingPieceCount = 8;', header)
        self.assertIn('enum class LinesRow : uint16_t\n{\n    Font,\n    Padding,\n};', header)
        self.assertIn('    RowsHead,', header)  # a piece's words are named after it
        self.assertIn('    EveryWindow,', header)
        # Looks before General now, and their settings in that order.
        looks, general = tables.index('{CardPiece::Looks,'), tables.index('{CardPiece::General,')
        self.assertLess(looks, general)
        self.assertIn('{CardPiece::Looks, "looks", "Looks", nullptr, kEveryKind, 0, 1},', tables)
        self.assertIn('{CardPiece::General, "general", "General", "The whole of it.", kEveryKind, 1, 2},', tables)

    def test_kinds_are_bits_by_the_order_of_the_entity_pages(self):
        _, tables = gen.generate(parse(fixture()), PIECES)
        self.assertIn('{SettingPiece::MaxIcons, "max-icons", "Max Icons", nullptr, nullptr, nullptr, 6, true,', tables)
        self.assertIn('{CardPiece::Plain, "mob-badges", "Badges", nullptr, 1,', tables)

    def test_a_text_by_kind_is_an_array_in_the_kinds_order(self):
        _, tables = gen.generate(parse(fixture()), PIECES)
        self.assertIn('{"A mob\'s.", "A player\'s.", "Yours.", "An NPC\'s.", "A pet\'s."}', tables)

    def test_a_page_of_the_entity_cards_and_the_lists(self):
        _, tables = gen.generate(parse(fixture()), PIECES)
        self.assertIn('{Page::Mobs, "mobs", "Mobs", true, 3, 2},', tables)
        self.assertIn('{WindowList::Mode, "mode", 0, 2},', tables)
        self.assertIn('{"On", "Some.", "Claimed."},', tables)

    def test_quotes_and_letters_past_ascii_are_escaped(self):
        tree = fixture()
        tree['words'][0]['name'] = 'Say "hi" – now'
        _, tables = gen.generate(parse(tree), PIECES)
        self.assertIn('"Say \\042hi\\042 \\342\\200\\223 now"', tables)


class Format(unittest.TestCase):
    def test_the_written_form_reads_back_the_same(self):
        tree = fixture()
        written = gen.format_file(tree)
        self.assertEqual(json.loads(written), tree)
        self.assertEqual(gen.format_file(json.loads(written)), written)

    def test_fields_come_key_first_and_each_level_two_spaces_in(self):
        tree = fixture()
        tree['pages'][0] = {'cards': tree['pages'][0]['cards'], 'name': 'Home', 'key': 'home'}
        written = gen.format_file(tree)
        self.assertIn('\n    {\n      "key": "home",\n      "name": "Home",\n      "cards": [\n', written)
        self.assertIn('{"key": "size", "name": "Size", "tip": "How big."}', written)


class Shipped(unittest.TestCase):
    def test_the_shipped_file_fits_the_code(self):
        gen.parse((ROOT / 'data' / 'settings_window.json').read_text(encoding='utf-8'), 'settings_window.json')

    def test_a_palettes_colors_and_the_status_rows_take_any_order(self):
        tree = json.loads((ROOT / 'data' / 'settings_window.json').read_text(encoding='utf-8'))

        def reorder(key):
            changed = copy.deepcopy(tree)
            settings = [s for page in changed['pages'] for card in page.get('cards', []) for s in card['settings']]
            settings += [s for card in changed['entity']['cards'] for s in card['settings']]
            next(s for s in settings if s['key'] == key)['rows'].reverse()
            return json.dumps(changed)

        for key in ('aggression-colors', 'toughness-colors', 'claim-colors', 'targeting-colors', 'entity-type-colors',
                    'status-table'):
            with self.subTest(key):
                gen.parse(reorder(key), 'settings_window.json')
        with self.assertRaisesRegex(gen.DataError, 'the rows of "detection" come in the code\'s order'):
            gen.parse(reorder('detection'), 'settings_window.json')  # its senses are a group under the first two
