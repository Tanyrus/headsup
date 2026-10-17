import json
import pathlib
import tempfile
import unittest

from tooling import load

npcs = load('tools/phoenix/npcs.py')
gen = load('tools/gen_npcs.py')
validate = load('tools/phoenix/validate.py')

# The server's constant tables as the dump writes them: Carpenters' Landing's mushrooms are the item numbers its
# qm_mycophile script names (Sleepshroom, Woozyshroom, Danceshroom), the rest made up for these tests.
ITEMS = {'SLEEPSHROOM': 4373, 'WOOZYSHROOM': 4374, 'DANCESHROOM': 4375, 'PINCH_OF_VALKURM_SUNSAND': 1416,
         'ANTLION_TRAP': 1594, 'POTION': 4112, 'ETHER': 4128, 'ANTIDOTE': 4148, 'CHAMOMILE': 949, 'SCROLL_OF_RAISE': 4609,
         'MIRATETES_MEMOIRS': 1590}
KEY_ITEMS = {'SILVER_BELL': 87, 'CORUSCANT_ROSARY': 88, 'MAP_OF_THE_CAVES': 400}
LUA = {'items': ITEMS, 'keyItems': KEY_ITEMS,
       'mobs': {'2': {'MYCOPHILE': 16785593, 'PEERIFOOL': [16793776, 16793777, 16793778],
                      'CHIGOES': {'ids': [16785610, 16785611]}}},
       'questLogs': {'SANDORIA': 0, 'JEUNO': 3, 'CRYSTAL_WAR': 7},
       'quests': {'sandoria': {'A_SQUIRES_TEST': 4}, 'jeuno': {'STORMS_OF_FATE': 86},
                  'crystalWar': {'ATELLOUNES_LAMENT': 12}},
       'missionLogs': {'COP': 6}, 'missions': {'cop': {'THE_RITES_OF_LIFE': 110}}, 'battlefields': {'SHATTERING_STARS': 32},
       'scripts': {}, 'interactions': {}}
SETTINGS = {'main': {'RESTRICT_CONTENT': 1, 'ENABLE_COP': 1, 'ENABLE_WOTG': 0}}


def constants(lua=None, settings=None):
    return npcs.Constants(dict(LUA, **(lua or {})), settings or SETTINGS)


def facts_of(code, on_trade=False, zone=2, whole_file=None):
    facts = npcs.Facts()
    npcs.read_handler(code, whole_file or code, zone, constants(), facts, on_trade)
    return facts


class LuaText(unittest.TestCase):
    def test_values(self):
        self.assertEqual(npcs.value('{ xi.item.POTION, { xi.item.ETHER, 2 }, item = 7 }'),
                         ('table', [('name', 'xi.item.POTION'), ('table', [('name', 'xi.item.ETHER'), ('num', 2)], {})],
                          {'item': ('num', 7)}))
        self.assertEqual(npcs.value("'gil'"), ('str', 'gil'))
        self.assertEqual(npcs.value('ID.mob.PEERIFOOL + i'), ('expr', 'ID.mob.PEERIFOOL + i'))

    def test_arguments_split_at_the_top_level_and_comments_go(self):
        self.assertEqual(npcs.split_top('player, { 1, 2 }, f(a, b), "x,y"'), ['player', '{ 1, 2 }', 'f(a, b)', '"x,y"'])
        self.assertEqual(npcs.strip_comments("a -- gone\nb = '--kept'\n--[[ block\n]] c"), "a \nb = '--kept'\n c")

    def test_a_function_with_nothing_in_it(self):
        self.assertFalse(npcs.has_code('entity.onTrade = function(player, npc, trade)\nend'))
        self.assertTrue(npcs.has_code('entity.onTrade = function(player, npc, trade)\n    helper(player)\nend'))

    def test_a_hidden_quests_name_in_words(self):
        # HiddenQuest:new('mimeoStone') in quests/hiddenQuests/Mimeo_Stone.lua, and a module's rebuilt Trust_Halver.
        self.assertEqual([npcs.hidden_name(n) for n in ('mimeoStone', 'CorArtifact', 'Trust_Halver', 'Tutorial')],
                         ['Mimeo Stone', 'Cor Artifact', 'Trust Halver', 'Tutorial'])

    def test_names_read_as_the_game_writes_them(self):
        self.assertEqual(npcs.title('THE_ROAD_TO_AHT_URHGAN'), 'The Road to Aht Urhgan')
        self.assertEqual(npcs.title('Storms_of_Fate'), 'Storms of Fate')
        self.assertEqual(npcs.title('DNC_AF1_The_Unfinished_Waltz'), 'DNC AF1 The Unfinished Waltz')


class Handlers(unittest.TestCase):
    def test_a_qm_that_pops_its_nm_on_a_trade(self):
        # Carpenters' Landing's qm_mycophile, as its script has it.
        facts = facts_of("""entity.onTrade = function(player, npc, trade)
    -- Sleepshroom, Woozyshroom, Danceshroom
    if
        npcUtil.tradeHas(trade, { 4373, 4374, 4375 }) and
        npcUtil.popFromQM(player, npc, ID.mob.MYCOPHILE)
    then
        player:confirmTrade()
    end
end""", on_trade=True)
        self.assertEqual((facts.trades, facts.pops, facts.unread), ({4373, 4374, 4375}, {16785593}, set()))

    def test_what_it_gives_and_the_key_items_it_gives_and_checks(self):
        facts = facts_of("""entity.onTrigger = function(player, npc)
    if player:hasKeyItem(xi.ki.SILVER_BELL) then
        npcUtil.giveKeyItem(player, xi.ki.CORUSCANT_ROSARY)
    elseif npcUtil.giveItem(player, xi.item.PINCH_OF_VALKURM_SUNSAND) then
        -- npcUtil.giveItem(player, xi.item.POTION) is a comment
        player:addItem(xi.item.ETHER, 3)
    end
end""")
        self.assertEqual((facts.gives, facts.key_items, facts.needs), ({1416, 4128}, {88}, {87}))

    def test_a_count_and_gil_in_a_trade(self):
        self.assertEqual(facts_of('npcUtil.tradeMatches(trade, { { xi.item.ANTLION_TRAP, 1 } })', True).trades, {1594})
        gil = facts_of("entity.onTrade = function(player, npc, trade)\n"
                       "    if npcUtil.tradeHasExactly(trade, { { 'gil', 200 } }) then end\nend", True)
        self.assertEqual((gil.trades, gil.unread), (set(), set()))

    def test_what_the_scripts_do_not_spell_out_is_unread(self):
        facts = facts_of("""entity.onTrade = function(player, npc, trade)
    if npcUtil.tradeHas(trade, wanted[npc:getID()]) then
        for i = 0, 2 do SpawnMob(ID.mob.PEERIFOOL + i) end
        npcUtil.giveItem(player, reward)
    end
end""", on_trade=True)
        self.assertEqual(facts.unread, {'trade', 'pops', 'gives'})
        self.assertEqual(facts_of('entity.onTrade = function(player, npc, trade)\n    helper(player)\nend', True).unread,
                         {'trade'})

    def test_a_mob_list_by_index_and_offset_and_a_spawn_by_id(self):
        self.assertEqual(facts_of('SpawnMob(ID.mob.PEERIFOOL[2])').pops, {16793777})
        self.assertEqual(facts_of('SpawnMob(ID.mob.MYCOPHILE + 1)').pops, {16785594})
        # A keyed table's IDs come sorted, so an index into it reads nothing.
        self.assertEqual(facts_of('SpawnMob(ID.mob.CHIGOES)').pops, {16785610, 16785611})
        self.assertEqual(facts_of('SpawnMob(ID.mob.CHIGOES[2])').unread, {'pops'})
        self.assertEqual(facts_of('GetMobByID(ID.mob.MYCOPHILE):spawn()').pops, {16785593})
        self.assertEqual(facts_of('local nm = GetMobByID(ID.mob.MYCOPHILE)\nif nm:isSpawned() then end').pops, set())

    def test_a_shared_system_call_reads_the_trade(self):
        facts = facts_of('entity.onTrade = function(player, npc, trade)\n'
                         '    xi.helm.onTrade(player, npc, trade, xi.helmType.HARVESTING,   12)\nend', True)
        self.assertEqual((facts.systems, facts.unread),
                         ({('helm', 'helm.onTrade(player, npc, trade, xi.helmType.HARVESTING, 12)')}, set()))

    def test_a_stock_in_the_file_with_ranks_or_item_numbers(self):
        file = """local stock =
{
    { xi.item.POTION,  910, 3 },
    { 4128,           4832 },  -- Ether
}

entity.onTrigger = function(player, npc)
    xi.shop.nation(player, stock, xi.nation.BASTOK)
end"""
        handler = 'entity.onTrigger = function(player, npc)\n    xi.shop.nation(player, stock, xi.nation.BASTOK)\nend'
        nation = facts_of(handler, whole_file=file)
        self.assertEqual((nation.stock, nation.fame), (((4112, 910, 3), (4128, 4832, 0)), True))
        # A general shop's prices move with fame only when it names a fame area, as xi.shop.general reads its log.
        for call, fame in (('xi.shop.general(player, stock, xi.fameArea.WINDURST)', True),
                           ('xi.shop.general(player, stock)', False)):
            self.assertEqual(facts_of(handler.replace('xi.shop.nation(player, stock, xi.nation.BASTOK)', call),
                                      whole_file=file).fame, fame)
        built = facts_of('local stock = {}\nfor _, f in ipairs(flowers) do table.insert(stock, f) end\n'
                         'xi.shop.general(player, stock)')
        self.assertEqual((built.stock, built.unread), (None, {'shop'}))

    def test_quests_and_missions_a_script_names(self):
        facts = facts_of('if player:getQuestStatus(xi.questLog.JEUNO, xi.quest.id.jeuno.STORMS_OF_FATE) == 2 and\n'
                         '    player:hasCompletedMission(xi.mission.log_id.COP, xi.mission.id.cop.THE_RITES_OF_LIFE) then end')
        self.assertEqual(facts.legacy, {('quest', 'jeuno', 'STORMS_OF_FATE'), ('mission', 'cop', 'THE_RITES_OF_LIFE')})


class Gates(unittest.TestCase):
    TRUST = '''check = function(player, questVars, vars)
            return xi.settings.main.ENABLE_TRUST_QUESTS == 1 and
                player:hasSpell(xi.magic.spell.HALVER)
        end'''

    def test_a_step_that_needs_a_setting_live_has_off(self):
        off = constants(settings={'main': {'ENABLE_TRUST_QUESTS': 0}})
        self.assertTrue(npcs.gated(self.TRUST, off))
        self.assertFalse(npcs.gated(self.TRUST, constants(settings={'main': {'ENABLE_TRUST_QUESTS': 1}})))
        self.assertFalse(npcs.gated(self.TRUST.replace(' and', ' or'), off))  # another way in
        self.assertFalse(npcs.gated('check = function(player) return status == 0 end', off))


# Two NPCs of zone 2 and their scripts in a server checkout. Rodrigue's script names A Squire's Test, read before the
# quest's own file is; Ganger sells what Rodrigue sells, completes A Squire's Test
# (a quest of the framework, whose reward is Miratete's Memoirs and the Map of the Caves) and names Storms of Fate and
# Atellounes Lament in its own script; the crystalWar quest is WotG's, which live's settings turn off.
SCRIPTS = {
    'scripts/zones/Somewhere/npcs/Rodrigue.lua': """local stock =
{
    { xi.item.POTION, 910 },
}

entity.onTrigger = function(player, npc)
    if player:getQuestStatus(xi.questLog.SANDORIA, xi.quest.id.sandoria.A_SQUIRES_TEST) ~= 1 then xi.shop.general(player, stock) end
end
""",
    'scripts/zones/Somewhere/npcs/Ganger.lua': """local stock = { { xi.item.POTION, 910 } }

entity.onTrigger = function(player, npc)
    if player:getQuestStatus(xi.questLog.JEUNO, xi.quest.id.jeuno.STORMS_OF_FATE) == 0 then
        xi.shop.general(player, stock)
    end
end

entity.onTrade = function(player, npc, trade)
    if npcUtil.tradeHas(trade, xi.item.ETHER) and xi.quest.id.crystalWar.ATELLOUNES_LAMENT then end
end
""",
    'scripts/quests/sandoria/A_Squires_Test.lua': """local quest = Quest:new(xi.questLog.SANDORIA, xi.quest.id.sandoria.A_SQUIRES_TEST)

quest.reward =
{
    item    = xi.item.MIRATETES_MEMOIRS,
    keyItem = xi.ki.MAP_OF_THE_CAVES,
    fame    = 30,
}

quest.sections =
{
    {
        [xi.zone.SOMEWHERE] =
        {
            ['Ganger'] =
            {
                onTrade = function(player, npc, trade)
                    if npcUtil.tradeHasExactly(trade, xi.item.ANTIDOTE) then
                        quest:complete(player)
                    end
                end,
            },
        },
    },
}
""",
}
DUMP_NPCS = [
    {'id': 16785600, 'zone': 2, 'zoneName': 'Somewhere', 'script': 'Ganger', 'name': 'Ganger', 'pos': [1.5, -2.25, 3, 7]},
    {'id': 16785599, 'zone': 2, 'zoneName': 'Somewhere', 'script': 'Rodrigue', 'name': 'Rodrigue', 'pos': [0, 0, 0, 0]},
    {'id': 16785601, 'zone': 2, 'zoneName': 'Somewhere', 'script': 'qm2', 'name': '???', 'pos': [10, 1, -10, 255]},
]
DUMP_LUA = dict(LUA, scripts={'Somewhere': {
    'Rodrigue': {'onTrigger': ['scripts/zones/Somewhere/npcs/Rodrigue.lua', 6, 8]},
    'Ganger': {'onTrigger': ['scripts/zones/Somewhere/npcs/Ganger.lua', 3, 7],
               'onTrade': ['scripts/zones/Somewhere/npcs/Ganger.lua', 9, 11]}}},
    interactions={'2': {'Ganger': {'onTrade': [
        {'container': {'id': 'Quest[0][4]', 'file': 'scripts/quests/sandoria/A_Squires_Test', 'log': 0, 'quest': 4,
                       'mission': None, 'name': None},
         'check': None, 'handler': ['scripts/quests/sandoria/A_Squires_Test.lua', 18, 22]}],
        'onTrigger': [{'container': None, 'check': None, 'handler': {'id': 384}}]}}})


class Build(unittest.TestCase):
    def run_npcs(self, dumps=None, scripts=None):
        with tempfile.TemporaryDirectory() as tmp:
            server = pathlib.Path(tmp) / 'server'
            for path, text in dict(SCRIPTS, **(scripts or {})).items():
                (server / path).parent.mkdir(parents=True, exist_ok=True)
                (server / path).write_text(text)
            paths = []
            for n, dump in enumerate(dumps or [{'npcs': DUMP_NPCS, 'npcLua': DUMP_LUA, 'settings': SETTINGS}]):
                paths.append(pathlib.Path(tmp) / f'dump-{n}.json')
                paths[-1].write_text(json.dumps(dump))
            tables = npcs.run(server, paths, pathlib.Path(tmp) / 'out')
            files = {p.name: p.read_text() for p in (pathlib.Path(tmp) / 'out').iterdir()}
        return tables, files

    def test_the_rows(self):
        (rows, shops, links, systems, key_items), _ = self.run_npcs()
        self.assertEqual([r['id'] for r in rows], [16785599, 16785600, 16785601])
        rodrigue, ganger, qm = rows
        self.assertEqual((rodrigue['shop'], ganger['shop'], qm['shop']), (1, 1, 0))
        self.assertEqual(shops, [[{'list': 1, 'item': 4112, 'price': 910, 'rank': 0, 'fame': 0}]])
        self.assertEqual((ganger['trades'], ganger['gives'], ganger['keyItems']), ([4128, 4148], [1590], [400]))
        self.assertEqual(ganger['pos'], [1.5, -2.25, 3.0, 7])
        self.assertEqual([(l['kind'], l['log'], l['id'], l['name'], l['file'], l['expansion'], l['live']) for l in links],
                         [('quest', 0, 4, 'A Squires Test', 'scripts/quests/sandoria/A_Squires_Test', '', 1),
                          ('quest', 3, 86, 'Storms of Fate', '', '', 1),
                          ('quest', 7, 12, 'Atellounes Lament', '', 'wotg', 0)])
        self.assertEqual((ganger['links'], rodrigue['links']), ([1, 2, 3], [1]))
        self.assertEqual((qm['name'], qm['links'], qm['unread'], systems), ('???', [], [], []))
        self.assertEqual(key_items, {400: 'Map of the Caves'})  # Ganger's reward, by its identifier in words

    def test_the_files_gen_npcs_reads(self):
        _, files = self.run_npcs()
        self.assertEqual(files['phoenix_npcs.tsv'].splitlines()[2],
                         '16785600\t2\tGanger\tGanger\t1.500,-2.250,3.000,7\t4128,4148\t1590\t400\t\t\t1\t1,2,3\t\t')
        self.assertEqual(files['phoenix_npc_links.tsv'].splitlines()[3],
                         '3\tquest\t7\t12\tAtellounes Lament\t\twotg\t0')
        self.assertEqual(files['phoenix_key_items.tsv'], 'id\tname\n400\tMap of the Caves\n')

    def test_a_quest_whose_every_step_needs_a_setting_live_has_off(self):
        # A trust quest on Rodrigue, its one step gated as scripts/quests/hiddenQuests/Trust_Halver.lua's.
        script = 'scripts/quests/hiddenQuests/Trust_Halver.lua'
        SCRIPTS[script] = 'quest.sections = { { check = function(player, questVars, vars)\n' \
            '    return xi.settings.main.ENABLE_TRUST_QUESTS == 1 and player:getRank() >= 3\nend, }, }\n'
        # A module rebuilt its container, which has no file name; its check is the quest file's.
        entry = {'container': {'id': 'HQuest[Trust_Halver]', 'file': None, 'log': None, 'quest': None,
                               'mission': None, 'name': 'Trust_Halver'},
                 'check': [script, 1, 3], 'handler': {'id': 433}}
        lua = dict(DUMP_LUA, interactions=dict(DUMP_LUA['interactions'], **{'2': dict(DUMP_LUA['interactions']['2'],
                   Rodrigue={'onTrigger': [entry]})}))
        settings = {'main': dict(SETTINGS['main'], ENABLE_TRUST_QUESTS=0)}
        try:
            (rows, _, links, _, _), _ = self.run_npcs([{'npcs': DUMP_NPCS, 'npcLua': lua, 'settings': settings}])
        finally:
            del SCRIPTS[script]
        trust = next(l for l in links if l['kind'] == 'hidden')
        self.assertEqual((trust['name'], trust['file'], trust['live']), ('Trust Halver', script[:-4], 0))
        self.assertEqual(rows[0]['links'], [1, trust['link']])

    def test_what_an_npc_does_only_for_content_live_has_off_is_left_out(self):
        # qm2 takes part in two quests: its step of A Squire's Test takes Valkurm sunsand, and its step of Atellounes
        # Lament (WotG's, which live turns off) takes Chamomile and pops the Mycophile. A trust quest has a step on qm2
        # that needs nothing, so it is live, and one on Rodrigue that needs ENABLE_TRUST_QUESTS and gives a Scroll of
        # Raise.
        squire, lament, trust = ('scripts/quests/sandoria/Squire_Steps.lua',
                                 'scripts/quests/crystalWar/Atellounes_Lament.lua',
                                 'scripts/quests/hiddenQuests/Trust_Halver.lua')
        scripts = {
            squire: 'onTrade = function(player, npc, trade)\n'
                    '    if npcUtil.tradeHasExactly(trade, xi.item.PINCH_OF_VALKURM_SUNSAND) then end\nend\n',
            lament: 'onTrade = function(player, npc, trade)\n'
                    '    if npcUtil.tradeHasExactly(trade, xi.item.CHAMOMILE) then npcUtil.popFromQM(player, npc, '
                    'ID.mob.MYCOPHILE) end\nend\n',
            trust: 'check = function(player)\n    return xi.settings.main.ENABLE_TRUST_QUESTS == 1\nend\n'
                   'onTrigger = function(player, npc)\n    npcUtil.giveItem(player, xi.item.SCROLL_OF_RAISE)\nend\n'}

        def step(cid, file, log, quest, handler, check=None, name=None):
            return {'container': {'id': cid, 'file': file, 'log': log, 'quest': quest, 'mission': None, 'name': name},
                    'check': check, 'handler': handler}
        interactions = dict(DUMP_LUA['interactions']['2'],
                            qm2={'onTrade': [step('Quest[0][4]', 'scripts/quests/sandoria/A_Squires_Test', 0, 4,
                                                  [squire, 1, 3]),
                                             step('Quest[7][12]', lament[:-4], 7, 12, [lament, 1, 3])],
                                 'onTrigger': [step('HQuest[TrustHalver]', trust[:-4], None, None, {'id': 433},
                                                    name='TrustHalver')]},
                            Rodrigue={'onTrigger': [step('HQuest[TrustHalver]', trust[:-4], None, None, [trust, 4, 6],
                                                         [trust, 1, 3], 'TrustHalver')]})
        lua = dict(DUMP_LUA, interactions={'2': interactions})
        settings = {'main': dict(SETTINGS['main'], ENABLE_TRUST_QUESTS=0)}
        (rows, _, links, _, _), _ = self.run_npcs([{'npcs': DUMP_NPCS, 'npcLua': lua, 'settings': settings}], scripts)
        rodrigue, _, qm = rows
        self.assertEqual([(l['name'], l['live']) for l in links],
                         [('A Squires Test', 1), ('Storms of Fate', 1), ('Atellounes Lament', 0), ('Trust Halver', 1)])
        self.assertEqual((qm['trades'], qm['pops'], qm['links'], qm['unread']), ([1416], [], [1, 3, 4], []))
        self.assertEqual((rodrigue['gives'], rodrigue['links']), ([], [1, 4]))

    def test_a_log_whose_server_name_is_not_the_one_gen_npcs_names_it_is_an_error(self):
        # gen_npcs names quest log 4 Other Areas; a server whose Jeuno log were 4 would show quests under the wrong log.
        lua = dict(DUMP_LUA, questLogs={'SANDORIA': 0, 'JEUNO': 4, 'CRYSTAL_WAR': 7})
        with self.assertRaises(npcs.DataError) as caught:
            self.run_npcs([{'npcs': DUMP_NPCS, 'npcLua': lua, 'settings': SETTINGS}])
        self.assertIn('quest log 4 is JEUNO on the server, but gen_npcs names it Other Areas', str(caught.exception))

    def test_dumps_with_other_npcs_are_an_error(self):
        dump = {'npcs': DUMP_NPCS, 'npcLua': DUMP_LUA, 'settings': SETTINGS}
        with self.assertRaises(npcs.DataError) as caught:
            self.run_npcs([dump, dict(dump, npcs=DUMP_NPCS[:2])])
        self.assertIn('its NPCs differ', str(caught.exception))


NPC_HEADER = '\t'.join(gen.NPC_COLUMNS)
NPC_ROW = '16785600\t2\tGanger\tGanger\t1.500,-2.250,3.000,7\t4148\t1590\t400\t\t\t1\t1\t1\t'


class Format(unittest.TestCase):
    def assertBad(self, row, message):
        with self.assertRaises(gen.DataError) as caught:
            gen.parse_npcs(NPC_HEADER + '\n' + row + '\n', 'n.tsv')
        self.assertIn('n.tsv:2:', str(caught.exception))
        self.assertIn(message, str(caught.exception))

    def test_bad_rows_name_their_line(self):
        self.assertEqual(len(gen.parse_npcs(NPC_HEADER + '\n' + NPC_ROW + '\n')), 1)
        self.assertBad(NPC_ROW.replace('1.500,', '1.5,'), 'pos must be x,y,z with three decimals')
        self.assertBad(NPC_ROW.replace(',7\t', ',256\t'), 'a rotation from 0 to 255')
        self.assertBad(NPC_ROW.replace('\t4148\t', '\t4148,12\t'), 'trades must be increasing')
        self.assertBad(NPC_ROW + 'chests', 'unread must name trade, gives, pops, shop')

    def test_lists_count_from_1_and_every_entry_is_named(self):
        links = gen.parse_links('\t'.join(gen.LINK_COLUMNS) + '\n1\tquest\t0\t4\tA Squires Test\t\t\t1\n')
        shops = gen.parse_shops('list\titem\tprice\trank\tfame\n1\t4112\t910\t0\t0\n')
        systems = gen.parse_systems('system\tfamily\tcall\n1\thelm\thelm.onTrade(player)\n')
        key_items = gen.parse_key_items('id\tname\n400\tMap of the Caves\n')
        row = gen.parse_npcs(NPC_HEADER + '\n' + NPC_ROW + '\n')
        gen.check(row, shops, links, systems, key_items)
        with self.assertRaises(gen.DataError) as caught:
            gen.check([dict(row[0], links=[])], shops, links, systems, key_items)
        self.assertIn('phoenix_npc_links.tsv: 1 entries no NPC names', str(caught.exception))
        with self.assertRaises(gen.DataError) as caught:
            gen.check([dict(row[0], shop=2)], shops, links, systems, key_items)
        self.assertIn('NPC 16785600 names a shop, link or system past', str(caught.exception))
        with self.assertRaises(gen.DataError) as caught:
            gen.check([dict(row[0], needs=[87])], shops, links, systems, key_items)
        self.assertIn('NPC 16785600 names key item 87, which phoenix_key_items.tsv does not', str(caught.exception))
        with self.assertRaises(gen.DataError) as caught:
            gen.check(row, shops, links, systems, {**key_items, 87: 'Silver Bell'})
        self.assertIn('phoenix_key_items.tsv: 1 key items no NPC names, the first 87', str(caught.exception))
        with self.assertRaises(gen.DataError) as caught:
            gen.parse_shops('list\titem\tprice\trank\tfame\n1\t4112\t910\t0\t0\n1\t4128\t4832\t0\t1\n', 's.tsv')
        self.assertIn("s.tsv:3: fame must be the same on each of a list's rows", str(caught.exception))
        with self.assertRaises(gen.DataError) as caught:
            gen.parse_key_items('id\tname\n400\tMap of the Caves\n87\tSilver Bell\n', 'k.tsv')
        self.assertIn('k.tsv:3: ids must be strictly increasing', str(caught.exception))
        with self.assertRaises(gen.DataError) as caught:
            gen.parse_links('\t'.join(gen.LINK_COLUMNS) + '\n2\tquest\t0\t4\tA\t\t\t1\n', 'l.tsv')
        self.assertIn('l.tsv:2: link must count from 1', str(caught.exception))
        with self.assertRaises(gen.DataError) as caught:
            gen.parse_systems('system\tfamily\tcall\n1\thelm\tdynamis.onTrade(player)\n', 's.tsv')
        self.assertIn('a call must start with its system', str(caught.exception))



def npc(npc_id, **fields):
    return dict({'id': npc_id, 'zone': 1, 'script': 'x', 'name': 'x', 'pos': [0.0, 0.0, 0.0, 0], 'trades': [],
                 'gives': [], 'keyItems': [], 'needs': [], 'pops': [], 'shop': 0, 'links': [], 'systems': [],
                 'unread': []}, **fields)


class ValidateNpcs(unittest.TestCase):
    # What the known NPCs' scripts say (validate.EXPECTED_NPCS and EXPECTED_STOCK, written down here again).
    NPCS = [npc(16785729, name='???', trades=[4373, 4374, 4375], pops=[16785722]), npc(17199699, gives=[503, 1000]),
            npc(17678362, shop=1)]
    SHOPS = [[{'list': 1, 'item': i, 'price': p, 'rank': 0, 'fame': 0} for i, p in
              ((4112, 910), (4128, 4832), (4148, 316), (4150, 2595), (4151, 800))]]

    KEY_ITEMS = {8: 'Airship Pass'}

    def check(self, npcs_=None, shops=None, key_items=None, **limits):
        return validate.check_npcs(npcs_ or self.NPCS, shops or self.SHOPS, [], [], key_items or self.KEY_ITEMS,
                                   min_npc=limits)

    def test_known_npcs_pass(self):
        self.assertEqual(self.check(), [])

    def test_a_lost_trade_pop_or_stock_fails(self):
        lost = [dict(n, pops=[]) if n['id'] == 16785729 else n for n in self.NPCS]
        self.assertEqual(len(self.check(lost)), 1)
        cheaper = [[dict(e, price=e['price'] - 1) for e in self.SHOPS[0]]]
        self.assertEqual(len(self.check(shops=cheaper)), 1)

    def test_a_key_item_under_another_name_fails(self):
        self.assertEqual(self.check(key_items={8: 'Airship Pas'}),
                         ["key item 8: expected the name 'Airship Pass', got 'Airship Pas'"])

    def test_an_npc_live_turns_off_fails(self):
        self.assertEqual(self.check(self.NPCS + [npc(17199755)]),
                         ["NPC 17199755 (Field_Manual) should be turned off by live's settings"])

    def test_too_few_fail(self):
        self.assertEqual(self.check(**{'??? that pop a mob': 2}),
                         ['only 1 ??? that pop a mob (expected at least 2)'])


if __name__ == '__main__':
    unittest.main()
