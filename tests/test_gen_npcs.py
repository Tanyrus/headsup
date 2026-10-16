import unittest

from tooling import load

gen = load('tools/gen_npcs.py')


def npc(npc_id, **fields):
    return dict({'id': npc_id, 'zone': 2, 'script': 'x', 'name': '???', 'pos': [0.0, 0.0, 0.0, 0], 'trades': [],
                 'gives': [], 'keyItems': [], 'needs': [], 'pops': [], 'shop': 0, 'links': [], 'systems': [],
                 'unread': []}, **fields)


# Carpenters' Landing's qm_mycophile (its mushrooms and the Mycophile), a blank NPC, one that only sells by a stock its
# script works out, and one that shares the mushroom trade and has a little of everything else.
NPCS = [npc(16785729, trades=[4373, 4374, 4375], pops=[16785722]), npc(16785730), npc(16785731, unread=['shop']),
        npc(16785800, trades=[4373, 4374, 4375], gives=[503], keyItems=[400], needs=[87], unread=['pops', 'shop'])]
KEY_ITEMS = {87: 'Silver Bell', 400: 'Map of the Caves'}


class Generate(unittest.TestCase):
    def setUp(self):
        self.text = gen.generate(NPCS, KEY_ITEMS, 'abc123')

    def test_only_npcs_with_something_to_show_and_a_list_shared_once(self):
        # The pools start with the empty entry at 0; the second NPC's trade is the first's, at 1 for 3.
        self.assertIn('const NpcRecord kNpcs[] = {\n'
                      '    {16785729u, {1, 3}, {0, 0}, {0, 0}, {0, 0}, {1, 1}, 0},\n'
                      '    {16785800u, {1, 3}, {4, 1}, {1, 1}, {2, 1}, {0, 0}, 12},\n'
                      '};', self.text)
        self.assertIn('const uint16_t kNpcItems[] = {\n    0, 4373, 4374, 4375, 503,\n};', self.text)
        self.assertIn('const uint16_t kNpcKeyItems[] = {\n    0, 400, 87,\n};', self.text)
        self.assertIn('const uint32_t kNpcMobs[] = {\n    0u, 16785722u,\n};', self.text)

    def test_key_item_names_in_id_order_and_the_unread_bits_checked(self):
        self.assertIn('const KeyItemName kKeyItemNames[] = {\n    {87, "Silver Bell"},\n    {400, "Map of the Caves"},\n};',
                      self.text)
        self.assertIn('static_assert(kNpcUnreadPops == 4, ', self.text)
        self.assertIn('(Phoenix abc123)', self.text)

    def test_a_list_too_long_for_its_span_is_an_error(self):
        with self.assertRaises(gen.DataError) as caught:
            gen.generate([npc(1, gives=list(range(1, 257)))], {}, 'abc123')
        self.assertIn('NPC 1: 256 gives do not fit NpcSpan', str(caught.exception))


if __name__ == '__main__':
    unittest.main()
