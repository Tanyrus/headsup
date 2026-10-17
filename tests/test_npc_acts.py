import json
import pathlib
import tempfile
import unittest

from tooling import load

acts = load('tools/phoenix/npc_acts.py')
gen = load('tools/gen_npcs.py')
test_npcs = load('tests/test_npcs.py')


class Conditions(unittest.TestCase):
    def test_the_plainest_scenario_it_happens_in(self):
        # Happening by default takes no condition, whatever else it also happened under.
        self.assertEqual(acts.conditions(['base', 'keyItems', 'questAccepted, hour 3']), [])
        self.assertEqual(acts.conditions(['questAccepted', 'accepted+var2']), ['quest'])
        self.assertEqual(acts.conditions(['keyItems', 'questAccepted']), ['keyItems'])

    def test_hours_in_ranges_through_midnight_and_a_dimension_it_never_depends_on_dropped(self):
        night = [f'try 1, hour {h}' for h in (22, 23, 0, 1, 2, 3)] + ['try 1+accepted, hour 0']
        self.assertEqual(acts.conditions(night), ['hour:22-3'])
        self.assertEqual(acts.conditions([f'base, job {j}' for j in range(1, 23)]), [])
        self.assertEqual(acts.conditions(['base, weather 3', 'base, weather 4']), ['weather:3', 'weather:4'])

    def test_quest_steps_key_items_and_the_states_of_the_spot_and_player(self):
        self.assertEqual(acts.conditions(['step progress 4', 'step progress 4+keyItems']), ['step:4'])
        self.assertEqual(acts.conditions(['step progress 4, playerVar 1']), ['nmKilled', 'step:4'])
        self.assertEqual(acts.conditions(['base, keyItems only 345']), ['keyItem:345'])
        self.assertEqual(acts.conditions(['questAccepted, keyItems all but 344']), ['noKeyItem:344', 'quest'])
        self.assertEqual(acts.conditions(['base, npcVar 1', 'keyItems, npcVar 1']), ['primed'])
        self.assertEqual(acts.conditions(['base, random 2']), ['roll'])


class Effects(unittest.TestCase):
    def test_messages_mobs_items_and_key_items(self):
        done = [['message', 7636, 1823, 'SHEN_SPAWN'], ['popFromQM', 16793929, {'hide': 0}], ['tradeCheck', 1, 1823],
                ['gives', [[4112, 2], 99]], ['givesKeyItem', [400, 87]], ['event', 12, 0], ['confirms', 1823, 1]]
        says, pops, gives, key_items, event = acts.effects(done, {4112, 1823}, {400})
        self.assertEqual((says, pops, gives, key_items, event), ([(7636, (1823,))], [16793929], [4112], [400], True))


# A ??? as the probe writes it: a click that says a message, a trade that pops the Mycophile for Ether, a step of a
# live quest that gives a key item, a step of a quest live has off, and its zone's clock showing it at night.
TARGET = {'id': 16785601, 'zone': 2, 'script': 'qm2', 'hasFile': {'onTrigger': True, 'onTrade': True},
          'trigger': {'outcomes': [{'done': [['message', 7001, 4148, 'THING'], ['message', 7002, '']],
                                    'scenarios': ['base', 'base#2', 'keyItems']}], 'asked': []},
          'trade': {'outcomes': [{'done': [['tradeCheck', 1, 4128, 'has']], 'scenarios': ['empty']},
                                 {'done': [['tradeCheck', 1, 4128, 'has'], ['popFromQM', 16785593]],
                                  'scenarios': ['try 1', 'try 1+keyItems']}],
                    'checks': [4128], 'tries': [[[4128, 1]]]},
          'steps': [{'id': 'Quest[0][4]', 'log': 0, 'quest': 4, 'handler': 'onTrigger',
                     'trigger': {'outcomes': [{'done': [['givesKeyItem', 400]], 'scenarios': ['step progress 2']}]}},
                    {'id': 'Quest[7][12]', 'log': 7, 'quest': 12, 'handler': 'onTrigger',
                     'trigger': {'outcomes': [{'done': [['pops', 16785610]], 'scenarios': ['step']}]}}]}
SWEEP = {'zone': 2, 'name': 'Somewhere', 'results': [
    {'name': f'hour {h}', 'done': [['npc:setStatus', 16785601, 0 if h >= 20 else 2]]} for h in (7, 20, 21)]}
IDS = 'zones[xi.zone.SOMEWHERE] =\n{\n    text =\n    {\n        THING = 7001, -- You find a <item> here.\n' \
      '        NO_WORDS = 7002,\n    },\n}\n'


class Acts(unittest.TestCase):
    def test_each_thing_the_spot_does_with_steps_of_content_live_has_off_left_out(self):
        live = {('quest', 0, 4, ''): True, ('quest', 7, 12, ''): False}
        found = acts.acts_of(TARGET, acts.sweeps_by_npc([SWEEP]), {4128, 4148}, {400}, {('quest', 0, 4, ''): 1}, live)
        self.assertEqual([(a['act'], a['items'], a['when']) for a in found],
                         [('click', [], []), ('trade', [[4128, 1]], []), ('click', [], ['link:1', 'step:2']),
                          ('hides', [], ['hour:7-7']), ('shows', [], ['hour:20-21'])])
        self.assertEqual(found[0]['says'], [(7001, (4148,)), (7002, ())])
        self.assertEqual((found[1]['pops'], found[2]['keyItems']), ([16785593], [400]))

    def test_the_words_of_a_zones_messages(self):
        self.assertEqual(acts.texts_in_ids(IDS), {7001: 'You find a <item> here.'})


class Build(unittest.TestCase):
    def test_what_the_probe_found_joins_the_row_and_its_acts_say_the_zones_words(self):
        scripts = {'scripts/zones/Somewhere/IDs.lua': IDS}
        (rows, _, links, _, key_items, found, texts), files = test_npcs.Build().run_npcs(
            scripts=scripts, probe=[{'target': TARGET}, {'sweep': SWEEP}])
        qm = rows[2]
        self.assertEqual((qm['id'], qm['trades'], qm['pops'], qm['keyItems']), (16785601, [4128], [16785593], [400]))
        self.assertEqual(texts, [{'text': 1, 'zone': 2, 'message': 7001, 'words': 'You find a <item> here.'}])
        # The click's message without words in IDs.lua is left out; the step of the WotG quest is not there at all.
        self.assertEqual([(a['act'], a['says'], a['when']) for a in found],
                         [('click', [(1, [4148])], []), ('trade', [], []), ('click', [], ['link:1', 'step:2']),
                          ('hides', [], ['hour:7-7']), ('shows', [], ['hour:20-21'])])
        self.assertEqual(files['phoenix_npc_acts.tsv'].splitlines()[2], '16785601\ttrade\t4128\t\t\t16785593\t\t\t0')
        self.assertIn(400, key_items)


class Format(unittest.TestCase):
    HEADER = '\t'.join(gen.ACT_COLUMNS)

    def test_bad_acts_name_their_line(self):
        for row, message in (('1\twave\t\t\t\t\t\t\t0', 'act must be one of'),
                             ('1\tclick\t\thour:3\t\t\t\t\t0', "when has 'hour:3'"),
                             ('1\ttrade\t4374,4373\t\t\t\t\t\t0', 'items must be increasing')):
            with self.assertRaises(gen.DataError) as caught:
                gen.parse_acts(self.HEADER + '\n' + row + '\n', 'a.tsv')
            self.assertIn('a.tsv:2:', str(caught.exception))
            self.assertIn(message, str(caught.exception))

    def test_an_act_must_say_a_text_there_is_and_every_text_is_said(self):
        npcs = [test_npcs.npc(1)]
        links = [{'link': 1, 'kind': 'quest', 'log': 7, 'id': 12, 'name': 'A', 'file': '', 'expansion': 'wotg',
                  'live': 0}]
        texts = gen.parse_texts('text\tzone\tmessage\twords\n1\t2\t7001\tYou find it.\n')
        said = gen.parse_acts(self.HEADER + '\n1\tclick\t\t\t1\t\t\t\t0\n')
        gen.check_acts(npcs, links, {}, said, texts)
        with self.assertRaises(gen.DataError) as caught:
            gen.check_acts(npcs, links, {}, [dict(said[0], says=[])], texts)
        self.assertIn('phoenix_npc_texts.tsv: 1 texts no act says', str(caught.exception))
        with self.assertRaises(gen.DataError) as caught:
            gen.check_acts(npcs, links, {}, [dict(said[0], when=['link:1'])], texts)
        self.assertIn('names link 1, which phoenix_npc_links.tsv does not have live', str(caught.exception))


if __name__ == '__main__':
    unittest.main()
