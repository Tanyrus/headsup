import pathlib
import tempfile
import unittest

from tooling import load

world = load('tools/phoenix/world.py')

SELBINA = ('id: 248\ntransport:\n  ship: 17793088\n  runs:\n    selbina_mhaura_boat:\n      door: _6ww\n'
           '      dock: [-1.000, -2.000, -3.000, 64]\n      voyage:\n        - ship_bound_for_selbina\n'
           '      every:     1152\n      disembark:   25\n      offset:     920\n      phases:\n'
           '        - state:     arriving\n          animation: animation_19\n          seconds:   41\n'
           '          moves:\n            - to: dock\n        - state:     docked\n          seconds:   192\n'
           '        - state:     closing\n          seconds:   1\n        - state:     departing\n'
           '          animation: animation_20\n'
           'npcs: {}\n')
# Two runs of one name, a decorative one without a door, and a phase left to take what is left of the cycle.
LANDING = ('transport:\n  ship: 16785748\n  runs:\n    barge_central_south_day:\n      door: _02t\n'
           '      every:     3456\n      offset:     647\n      phases:\n        - state:     arriving\n'
           '          seconds:   31\n        - state:     docked\n          seconds:   69\n        - state:     closing\n'
           '          seconds:   5\n        - state:     departing\n          seconds:   100\n'
           '        - state:     holding\n'
           '    barge_south_oos_day:\n      every:  3456 # out of service, carries nobody\n      offset: 1265\n'
           '      phases:\n        - state:     arriving\n\n'
           '    barge_central_south_night:\n      door: _02t\n      every:     3456\n      offset:    3000\n'
           '      phases:\n        - state:     arriving\n          seconds:   31\n        - state:     docked\n'
           '          seconds:   69\n        - state:     closing\n          seconds:   5\n        - state:     departing\n')
NAMES = {'selbina_mhaura_boat': ('Boats', 'Selbina to Mhaura')}
LANDING_NAMES = {'barge_central_south_day': ('Barge', 'Central to South Landing'),
                 'barge_central_south_night': ('Barge', 'Central to South Landing')}

GUILD_SHOPS = ("xi.data.guildShops =\n{\n    ['Chaupire'] =\n    {\n        hours   = { 6, 21 },\n"
               "        holiday = xi.day.FIRESDAY,\n        stock   =\n        {\n"
               "            { id = xi.item.ASH_LOG, initial = 36, maxStock = 60 },\n        },\n    },\n"
               "    ['Gathweeda'] = { sharedStock = 'Wahraga' },\n    ['Wahraga'] =\n    {\n"
               "        hours   = { 8, 23 },\n        holiday = xi.day.LIGHTNINGDAY,\n        stock   = {},\n    },\n"
               "    ['Dehbi_Moshal'] =\n    {\n        hours   = { 6, 21 },\n        holiday = xi.day.FIRESDAY,\n"
               "        stock   = {},\n    },\n}\n")
NPC_SCRIPTS = {
    'Northern_San_dOria/npcs/Chaupire.lua': "-- Area: Northern San d'Oria\n--  NPC: Chaupire\n-- Guild Merchant NPC: Woodworking Guild\n",
    'Aht_Urhgan_Whitegate/npcs/Wahraga.lua': '-- Area: Aht Urhgan Whitegate\n--  NPC: Wahraga\n-- Guild Merchant: Alchemist Guild\n',
    'Aht_Urhgan_Whitegate/npcs/Gathweeda.lua': '-- Area: Aht Urhgan Whitegate\n--  NPC: Gathweeda\n-- Guild Merchant: Alchemist Guild\n',
    'Al_Zahbi/npcs/Dehbi_Moshal.lua': '-- Area: Al Zahbi\n--  NPC: Dehbi Moshal\n-- Woodworking Guild\n',
}
REAL = pathlib.Path.home() / '.cache' / 'headsup-phoenix' / 'server'


def checkout(root: pathlib.Path, zones=None, shops: str = GUILD_SHOPS, scripts=None) -> pathlib.Path:
    files = {f'data/zones/{zone}/zone.yaml': text for zone, text in (zones or {'selbina': SELBINA}).items()}
    files['scripts/data/guild_shops.lua'] = shops
    files.update({f'scripts/zones/{path}': text for path, text in (scripts or NPC_SCRIPTS).items()})
    for relative, text in files.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
    return root


class Travel(unittest.TestCase):
    def test_a_run_boards_when_its_door_opens_and_leaves_when_it_departs(self):
        with tempfile.TemporaryDirectory() as tmp:
            routes = world.build_travel(checkout(pathlib.Path(tmp)), NAMES)
        # Docked 41 s after the 920 s offset; departing 41 + 192 + 1 s after it, past the 1152 s cycle's end.
        self.assertEqual(routes, [('Boats', 'Selbina to Mhaura', 1152, [(961, 2)])])

    def test_runs_of_one_name_are_one_route_and_runs_without_a_door_are_left_out(self):
        with tempfile.TemporaryDirectory() as tmp:
            routes = world.build_travel(checkout(pathlib.Path(tmp), {'carpenters_landing': LANDING}), LANDING_NAMES)
        self.assertEqual(routes, [('Barge', 'Central to South Landing', 3456, [(678, 752), (3031, 3105)])])

    def test_a_run_without_a_name_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), {'selbina': SELBINA.replace('selbina_mhaura_boat', 'selbina_new_boat')})
            with self.assertRaises(world.WorldDataError) as raised:
                world.build_travel(root, NAMES)
        self.assertIn('selbina_new_boat', str(raised.exception))

    def test_a_name_without_its_run_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp))
            with self.assertRaises(world.WorldDataError) as raised:
                world.build_travel(root, {**NAMES, 'jeuno_mars_airship': ('Airships', 'Jeuno to Mars')})
        self.assertIn('jeuno_mars_airship', str(raised.exception))

    def test_a_run_that_never_docks_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), {'selbina': SELBINA.replace('state:     docked', 'state:     waiting')})
            with self.assertRaises(world.WorldDataError) as raised:
                world.build_travel(root, NAMES)
        self.assertIn('docked', str(raised.exception))


class Guilds(unittest.TestCase):
    def test_each_shop_with_its_guild_place_hours_and_holiday(self):
        with tempfile.TemporaryDirectory() as tmp:
            shops = world.build_guilds(checkout(pathlib.Path(tmp)))
        # An alias draws on its shop and keeps its hours; Firesday is 0 and Lightningday 5.
        self.assertEqual(shops, [('Alchemy', 'Wahraga, Gathweeda', 'Aht Urhgan Whitegate', 8, 23, 5),
                                 ('Woodworking', 'Dehbi Moshal', 'Al Zahbi', 6, 21, 0),
                                 ('Woodworking', 'Chaupire', "Northern San d'Oria", 6, 21, 0)])

    def test_the_goblins_who_sell_chips_are_left_out(self):
        shops = GUILD_SHOPS.replace("\n}\n", "\n    ['Gaudylox'] =\n    {\n        hours   = { 11, 22 },\n"
                                    "        holiday = xi.day.FIRESDAY,\n        stock   = {},\n    },\n}\n")
        scripts = {**NPC_SCRIPTS, 'Northern_San_dOria/npcs/Gaudylox.lua':
                   "-- Area: Northern San d'Oria\n--  NPC: Gaudylox\n-- Standard merchant, though he acts like a guild merchant\n"}
        with tempfile.TemporaryDirectory() as tmp:
            kept = world.build_guilds(checkout(pathlib.Path(tmp), shops=shops, scripts=scripts))
        self.assertEqual([row[1] for row in kept], ['Wahraga, Gathweeda', 'Dehbi Moshal', 'Chaupire'])

    def test_a_merchant_of_no_kind_it_knows_is_an_error(self):
        scripts = dict(NPC_SCRIPTS)
        scripts['Al_Zahbi/npcs/Dehbi_Moshal.lua'] = '-- Area: Al Zahbi\n--  NPC: Dehbi Moshal\n-- Juggling Guild\n'
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(world.WorldDataError) as raised:
                world.build_guilds(checkout(pathlib.Path(tmp), scripts=scripts))
        self.assertIn('Dehbi_Moshal', str(raised.exception))

    def test_a_shop_without_its_npc_script_is_an_error(self):
        scripts = {path: text for path, text in NPC_SCRIPTS.items() if 'Chaupire' not in path}
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(world.WorldDataError) as raised:
                world.build_guilds(checkout(pathlib.Path(tmp), scripts=scripts))
        self.assertIn('Chaupire', str(raised.exception))

    def test_an_alias_of_no_shop_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = checkout(pathlib.Path(tmp), shops=GUILD_SHOPS.replace("sharedStock = 'Wahraga'", "sharedStock = 'Nobody'"))
            with self.assertRaises(world.WorldDataError) as raised:
                world.build_guilds(root)
        self.assertIn('Nobody', str(raised.exception))


class Write(unittest.TestCase):
    def test_writes_both_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp) / 'out'
            world.write(out, [('Boats', 'Selbina to Mhaura', 1152, [(961, 2)])],
                        [('Woodworking', 'Chaupire', "Northern San d'Oria", 6, 21, 0)])
            self.assertEqual((out / 'phoenix_travel.tsv').read_text(encoding='utf-8'),
                             'group\tname\tevery\tdepartures\nBoats\tSelbina to Mhaura\t1152\t961:2\n')
            self.assertEqual((out / 'phoenix_guilds.tsv').read_text(encoding='utf-8'),
                             'guild\tshops\tzone\topen\tclose\tholiday\n'
                             "Woodworking\tChaupire\tNorthern San d'Oria\t6\t21\t0\n")


@unittest.skipUnless((REAL / 'scripts' / 'data' / 'guild_shops.lua').is_file(), 'no Phoenix checkout')
class RealCheckout(unittest.TestCase):
    def test_every_run_with_a_door_has_a_name(self):
        routes = {name: (every, departures) for _, name, every, departures in world.build_travel(REAL)}
        # Phoenix's Jeuno airship to San d'Oria: docked 26 s and departing 180 s after its 2 s offset.
        self.assertEqual(routes["Jeuno to San d'Oria"], (864, [(28, 182)]))
        self.assertEqual(len(routes['Central to South Landing'][1]), 2)

    def test_every_guild_shop_has_a_kind(self):
        shops = world.build_guilds(REAL)
        self.assertEqual(len(shops), 38)  # 43 NPCs, two of them sharing another's shop and three goblins selling chips
        self.assertIn(('Woodworking', 'Chaupire', "Northern San d'Oria", 6, 21, 0), shops)
