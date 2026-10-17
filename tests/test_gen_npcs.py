import unittest

from tooling import load

gen = load('tools/gen_npcs.py')


def npc(npc_id, **fields):
    return dict({'id': npc_id, 'zone': 2, 'script': 'x', 'name': '???', 'pos': [0.0, 0.0, 0.0, 0], 'trades': [],
                 'gives': [], 'keyItems': [], 'needs': [], 'pops': [], 'shop': 0, 'links': [], 'systems': [],
                 'unread': []}, **fields)


# Carpenters' Landing's qm_mycophile (its mushrooms and the Mycophile), a blank NPC, one that only sells by a stock its
# script works out, one that shares the mushroom trade and has a little of everything else, a nation shop, and one in
# only a quest live has off.
NPCS = [npc(16785729, trades=[4373, 4374, 4375], pops=[16785722], links=[1, 2]), npc(16785730),
        npc(16785731, unread=['shop']),
        npc(16785800, trades=[4373, 4374, 4375], gives=[503], keyItems=[400], needs=[87], shop=1, links=[3],
            unread=['pops', 'shop']),
        npc(16785801, shop=2, systems=[1, 2]), npc(16785802, links=[2]), npc(16785803, systems=[1])]
LINKS = [{'link': 1, 'kind': 'quest', 'log': 0, 'id': 4, 'name': 'A Squires Test', 'file': '', 'expansion': '', 'live': 1},
         {'link': 2, 'kind': 'quest', 'log': 7, 'id': 12, 'name': 'Atellounes Lament', 'file': '', 'expansion': 'wotg',
          'live': 0},
         {'link': 3, 'kind': 'hidden', 'log': 0, 'id': 0, 'name': 'Mimeo Stone', 'file': '', 'expansion': '', 'live': 1}]
SHOPS = [[{'list': 1, 'item': 4112, 'price': 910, 'rank': 0, 'fame': 0}],
         [{'list': 2, 'item': 17395, 'price': 12, 'rank': 2, 'fame': 1},
          {'list': 2, 'item': 17389, 'price': 540, 'rank': 1, 'fame': 1}]]
KEY_ITEMS = {87: 'Silver Bell', 400: 'Map of the Caves'}
# A walking path, which says nothing of what the NPC is for, and a home point.
SYSTEMS = [{'system': 1, 'family': 'path', 'call': 'path.first(pathNodes)'},
           {'system': 2, 'family': 'homepoint', 'call': 'homepoint.onTrigger(player, hpEvent, hpIndex)'}]


class Generate(unittest.TestCase):
    def setUp(self):
        self.text = gen.generate(NPCS, SHOPS, LINKS, SYSTEMS, KEY_ITEMS, 'abc123')

    def test_only_npcs_with_something_to_show_and_a_list_shared_once(self):
        # The pools start with the empty entry at 0; the second NPC's trade is the first's, at 1 for 3. One whose stock
        # its script works out is kept to say so; one in only a quest live has off, or that only walks a path, is left
        # out.
        self.assertIn('const NpcRecord kNpcs[] = {\n'
                      '    {16785729u, {1, 3}, {0, 0}, {0, 0}, {0, 0}, {1, 1}, {1, 1}, {0, 0}, {0, 0}, 0, 0},\n'
                      '    {16785731u, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, 0, 8},\n'
                      '    {16785800u, {1, 3}, {4, 1}, {1, 1}, {2, 1}, {0, 0}, {2, 1}, {0, 0}, {0, 0}, 1, 12},\n'
                      '    {16785801u, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {1, 1}, {0, 0}, 2, 0},\n'
                      '};', self.text)
        self.assertIn('const uint16_t kNpcItems[] = {\n    0, 4373, 4374, 4375, 503,\n};', self.text)
        self.assertIn('const uint16_t kNpcKeyItems[] = {\n    0, 400, 87,\n};', self.text)
        self.assertIn('const uint32_t kNpcMobs[] = {\n    0u, 16785722u,\n};', self.text)

    def test_only_the_links_live_has_numbered_from_1_with_the_log_names(self):
        self.assertIn('const NpcLink kNpcLinks[] = {\n    {0, 0, ""},\n    {0, 0, "A Squires Test"},\n'
                      '    {3, 0, "Mimeo Stone"},\n};', self.text)
        self.assertIn('const uint16_t kNpcLinkIds[] = {\n    0, 1, 2,\n};', self.text)
        self.assertIn('const char* const kQuestLogNames[] = {\n    "San d\'Oria",\n    "Bastok",\n', self.text)
        self.assertIn('    "CoP",\n', self.text)
        self.assertIn('static_assert(kLinkHidden == 3, ', self.text)

    def test_what_its_systems_make_it_in_words_each_once(self):
        self.assertIn('const char* const kNpcPurposes[] = {\n    "",\n    "Home point",\n};', self.text)
        self.assertIn('const uint16_t kNpcPurposeIds[] = {\n    0, 1,\n};', self.text)

    def test_each_shop_in_its_numbers_place_with_its_stock(self):
        self.assertIn('const NpcStockItem kNpcStock[] = {\n'
                      '    {0, 0, 0u},\n    {4112, 0, 910u},\n    {17395, 2, 12u},\n    {17389, 1, 540u},\n};', self.text)
        self.assertIn('const NpcShop kNpcShops[] = {\n    {{0, 0}, false},\n    {{1, 1}, false},\n    {{2, 2}, true},\n};',
                      self.text)

    def test_key_item_names_in_id_order_and_the_unread_bits_checked(self):
        self.assertIn('const KeyItemName kKeyItemNames[] = {\n    {87, "Silver Bell"},\n    {400, "Map of the Caves"},\n};',
                      self.text)
        self.assertIn('static_assert(kNpcUnreadPops == 4, ', self.text)
        self.assertIn('(Phoenix abc123)', self.text)

    def test_a_list_too_long_for_its_span_is_an_error(self):
        with self.assertRaises(gen.DataError) as caught:
            gen.generate([npc(1, gives=list(range(1, 257)))], [], [], [], {}, 'abc123')
        self.assertIn('NPC 1: 256 gives do not fit NpcSpan', str(caught.exception))


# What the probe saw two ??? do: 16785730, which has nothing else to show, says a text at night in A Squires Test's
# step 2; 16785729 pops the Mycophile for its three mushrooms.
ACTS = [{'npc': 16785729, 'act': 'trade', 'items': [(4373, 1), (4374, 1), (4375, 1)], 'when': [], 'says': [],
         'pops': [16785722], 'gives': [], 'keyItems': [], 'event': 0},
        {'npc': 16785730, 'act': 'click', 'items': [], 'when': ['hour:22-3', 'link:1', 'step:2', 'mob:Shen_Aide'],
         'says': [(1, [4373, 7])], 'pops': [], 'gives': [], 'keyItems': [], 'event': 1}]
TEXTS = [{'text': 1, 'zone': 2, 'message': 7515, 'words': 'You find a <item> and <number> more.'}]


class Acts(unittest.TestCase):
    def setUp(self):
        self.text = gen.generate(NPCS, SHOPS, LINKS, SYSTEMS, KEY_ITEMS, 'abc123', ACTS, TEXTS)

    def test_an_npcs_acts_follow_one_another_and_one_with_only_acts_is_shown(self):
        self.assertIn('    {16785729u, {1, 3}, {0, 0}, {0, 0}, {0, 0}, {1, 1}, {1, 1}, {0, 0}, {1, 1}, 0, 0},\n'
                      '    {16785730u, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {2, 1}, 0, 0},\n',
                      self.text)
        # The trade's items and mob, then the click's conditions, message and cutscene; mobs share kNpcMobs.
        self.assertIn('const NpcAct kNpcActs[] = {\n    {0, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, false},\n'
                      '    {1, {1, 3}, {0, 0}, {0, 0}, {1, 1}, {0, 0}, {0, 0}, false},\n'
                      '    {0, {0, 0}, {1, 4}, {1, 1}, {0, 0}, {0, 0}, {0, 0}, true},\n};', self.text)
        self.assertIn('const NpcActItem kNpcActItems[] = {\n    {0, 0},\n    {4373, 1},\n    {4374, 1},\n    {4375, 1},\n};',
                      self.text)

    def test_conditions_messages_and_their_words(self):
        # link:1 is A Squires Test, the first live link; a mob's script name reads in words.
        self.assertIn('const NpcWhenEntry kNpcWhen[] = {\n    {0, 0, 0, ""},\n    {0, 22, 3, ""},\n    {9, 1, 0, ""},\n'
                      '    {10, 2, 0, ""},\n    {14, 0, 0, "Shen Aide"},\n};', self.text)
        self.assertIn('const uint32_t kNpcSayParams[] = {\n    0u,\n    4373,\n    7,\n};', self.text)
        self.assertIn('const NpcSay kNpcSays[] = {\n    {0, {0, 0}},\n    {1, {1, 2}},\n};', self.text)
        self.assertIn('const NpcText kNpcTexts[] = {\n    {0, ""},\n    {2, "You find a <item> and <number> more."},\n};',
                      self.text)
        self.assertIn('static_assert(kWhenZoneStart == 15, ', self.text)
        self.assertIn('static_assert(kActHides == 3, ', self.text)


class Purposes(unittest.TestCase):
    def test_a_systems_call_in_words(self):
        # Calls as Phoenix's NPC scripts make them (data/phoenix_npc_systems.tsv).
        for call, words in (('helm.onTrade(player, npc, trade, xi.helmType.LOGGING, 30)', 'Logging point'),
                            ('teleport.to(player, xi.teleport.id.CLOISTER_OF_FLAMES)', 'Teleports to Cloister of Flames'),
                            ('teleport.to(player, portalPick[option])', 'Teleports'),
                            ('crafting.guildPointOnTrigger(player, 690, xi.guild.LEATHERCRAFT)', 'Leathercraft guild points'),
                            ('crafting.getTotalSkill(player, xi.skill.FISHING)', None),
                            ('conquest.vendorOnTrigger(player, vendorRegion, vendorEvent)', 'Outpost vendor'),
                            ('instance.onTrigger(player, npc, xi.zone.THE_ASHU_TALIF)', 'Entrance to The Ashu Talif'),
                            ('dynamis.debugPrint(\'1. Trail markings on trade working\')', None),
                            ('path.first(pathNodes)', None)):
            self.assertEqual(gen.purpose(call.split('.')[0], call), words)

    def test_a_system_without_a_rule_fails_the_check(self):
        with self.assertRaises(gen.DataError) as caught:
            gen.purpose('voidwatch', 'voidwatch.onTrigger(player, npc)')
        self.assertIn('system voidwatch has no rule in SYSTEM_RULES (voidwatch.onTrigger(player, npc))',
                      str(caught.exception))
        with self.assertRaises(gen.DataError) as caught:
            gen.purpose('transport', 'transport.newCall(npc)')
        self.assertIn('no rule of transport matches transport.newCall(npc)', str(caught.exception))


if __name__ == '__main__':
    unittest.main()
