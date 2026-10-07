#include "player_status.h"
#include "settings.h"
#include "test.h"

#include <cstring>
#include <vector>

using namespace headsup;

namespace
{
    // GP_SERV_CHAR_PC (0x00D), up to its third flags word, as Phoenix's src/map/packets/char_update.cpp lays it out.
    std::vector<uint8_t> OtherPlayer(uint16_t index, uint8_t sendFlags, uint32_t flags1, uint32_t flags2, uint32_t flags3)
    {
        std::vector<uint8_t> p(0x2C, 0);
        p[0] = 0x0D;
        std::memcpy(&p[0x08], &index, 2);
        p[0x0A] = sendFlags;
        std::memcpy(&p[0x20], &flags1, 4);
        std::memcpy(&p[0x24], &flags2, 4);
        std::memcpy(&p[0x28], &flags3, 4);
        return p;
    }

    // GP_SERV_SERVERSTATUS (0x037), up to its fourth flags word (src/map/packets/char_status.cpp).
    std::vector<uint8_t> OwnStatus(uint32_t flags0, uint32_t flags1, uint8_t r, uint8_t g, uint8_t b, uint32_t flags3)
    {
        std::vector<uint8_t> p(0x3C, 0);
        p[0] = 0x37;
        std::memcpy(&p[0x28], &flags0, 4);
        std::memcpy(&p[0x2C], &flags1, 4);
        p[0x31] = r;
        p[0x32] = g;
        p[0x33] = b;
        std::memcpy(&p[0x38], &flags3, 4);
        return p;
    }

    // GP_SERV_ENTITY_SET (0x067) up to its flags byte, as Phoenix's src/map/packets/char_sync.cpp lays it out for a
    // character (kind 2) and entity_set_name.cpp for a named entity (kind 3).
    std::vector<uint8_t> CharSync(uint8_t kind, uint16_t index, uint8_t flags)
    {
        std::vector<uint8_t> p(0x28, 0);
        p[0]    = 0x67;
        p[0x04] = kind;
        std::memcpy(&p[0x06], &index, 2);
        p[0x10] = flags;
        return p;
    }

    constexpr uint8_t kGeneral = 0x04, kDespawn = 0x20;

    struct Expected
    {
        bool seekingParty, bazaar, linkshell, away, mentor, newAdventurer, gm;
    };

    void CheckStatus(const PlayerStatus& s, const Expected& e)
    {
        CHECK(s.seekingParty == e.seekingParty);
        CHECK(s.bazaar == e.bazaar);
        CHECK(s.linkshell == e.linkshell);
        CHECK(s.away == e.away);
        CHECK(s.mentor == e.mentor);
        CHECK(s.newAdventurer == e.newAdventurer);
        CHECK(s.gm == e.gm);
    }
}

TEST(each_flag_of_another_players_update_comes_from_its_own_bit)
{
    // One flag per row; GM is a 3-bit level from bit 24, so levels 1 and 4 pin the shift and bit 27 is not GM.
    struct Row
    {
        uint32_t flags1, flags3;
        Expected expected;
    };
    const Row rows[] = {
        {1u << 11, 0, {true, false, false, false, false, false, false}},
        {1u << 31, 0, {false, true, false, false, false, false, false}},
        {1u << 17, 0, {false, false, true, false, false, false, false}},
        {1u << 14, 0, {false, false, false, true, false, false, false}},
        {0, 1u << 24, {false, false, false, false, true, false, false}},
        {0, 1u << 23, {false, false, false, false, false, true, false}},
        {1u << 24, 0, {false, false, false, false, false, false, true}},
        {4u << 24, 0, {false, false, false, false, false, false, true}},
        {1u << 27, 0, {false, false, false, false, false, false, false}},
    };
    for (const Row& row : rows)
    {
        const auto p      = OtherPlayer(1137, kGeneral, row.flags1, 0, row.flags3);
        const auto update = ParseOtherPlayer(p.data(), static_cast<uint32_t>(p.size()));
        CHECK(update.has_value() && update->status.has_value());
        CheckStatus(*update->status, row.expected);
    }
}

TEST(another_players_update_has_their_index_and_linkshell_color)
{
    const auto p      = OtherPlayer(1137, kGeneral, 0, 0x00FF1F8F, 0); // red, green, blue from the low byte
    const auto update = ParseOtherPlayer(p.data(), static_cast<uint32_t>(p.size()));
    CHECK(update.has_value() && update->status.has_value());
    CHECK_EQ(update->index, 1137);
    CHECK(!update->despawn);
    CHECK_EQ(update->status->linkshellArgb, 0xFF8F1FFFu);
}

TEST(a_position_only_update_or_a_despawn_carries_no_status)
{
    const auto moved  = OtherPlayer(1137, 0x01, 1u << 31, 0, 0);
    const auto update = ParseOtherPlayer(moved.data(), static_cast<uint32_t>(moved.size()));
    CHECK(update.has_value() && !update->despawn && !update->status.has_value());
    const auto gone    = OtherPlayer(1137, kDespawn, 0, 0, 0);
    const auto despawn = ParseOtherPlayer(gone.data(), static_cast<uint32_t>(gone.size()));
    CHECK(despawn.has_value() && despawn->despawn);
    CHECK(!ParseOtherPlayer(gone.data(), 0x20).has_value()); // cut short
    CHECK(!ParseOtherPlayer(nullptr, 0).has_value());
}

TEST(each_flag_of_your_own_status_comes_from_its_own_bit)
{
    // Seeking, away, linkshell and a 3-bit GM level from bit 29 in the first word; bazaar is bit 29 of the second, so a
    // GM row must not read as a bazaar; new adventurer and mentor are in the fourth.
    struct Row
    {
        uint32_t flags0, flags1, flags3;
        Expected expected;
    };
    const Row rows[] = {
        {1u << 4, 0, 0, {true, false, false, false, false, false, false}},
        {0, 1u << 29, 0, {false, true, false, false, false, false, false}},
        {1u << 25, 0, 0, {false, false, true, false, false, false, false}},
        {1u << 7, 0, 0, {false, false, false, true, false, false, false}},
        {0, 0, 1u << 4, {false, false, false, false, true, false, false}},
        {0, 0, 1u << 3, {false, false, false, false, false, true, false}},
        {1u << 29, 0, 0, {false, false, false, false, false, false, true}},
        {4u << 29, 0, 0, {false, false, false, false, false, false, true}},
    };
    for (const Row& row : rows)
    {
        const auto p = OwnStatus(row.flags0, row.flags1, 0, 0, 0, row.flags3);
        const auto s = ParseOwnStatus(p.data(), static_cast<uint32_t>(p.size()));
        CHECK(s.has_value());
        CheckStatus(*s, row.expected);
    }
}

TEST(your_own_status_has_your_linkshell_color)
{
    const auto p = OwnStatus(0, 0, 0x2F, 0xCF, 0x6F, 0);
    const auto s = ParseOwnStatus(p.data(), static_cast<uint32_t>(p.size()));
    CHECK(s.has_value());
    CHECK_EQ(s->linkshellArgb, 0xFF2FCF6Fu);
    CHECK(!ParseOwnStatus(p.data(), 0x38).has_value());
}

namespace
{
    PlayerStatus Everything()
    {
        PlayerStatus all;
        all.seekingParty = all.bazaar = all.linkshell = all.away = all.mentor = all.newAdventurer = all.gm = all.levelSync = true;
        return all;
    }

    void CheckRow(const IconSet& row, std::vector<Icon> expected)
    {
        CHECK_EQ(row.count, static_cast<int>(expected.size()));
        for (int i = 0; i < row.count && i < static_cast<int>(expected.size()); ++i)
            CHECK(row.icons[i] == expected[static_cast<size_t>(i)]);
    }
}

TEST(player_icons_in_order_left_of_the_name_by_default)
{
    const PlayerIconRows none = PlayerIcons(PlayerStatus{}, Settings{});
    CHECK(none.left.count == 0 && none.right.count == 0);
    const PlayerIconRows all = PlayerIcons(Everything(), Settings{});
    CheckRow(all.left, {Icon::Gm, Icon::Mentor, Icon::NewAdventurer, Icon::LevelSync, Icon::Away, Icon::Linkshell,
                           Icon::Bazaar, Icon::Invite});
    CHECK_EQ(all.right.count, 0);
}

TEST(each_player_icon_goes_to_its_side_or_nowhere)
{
    Settings s;
    s.playerIconSide[static_cast<int>(PlayerIcon::LevelSync)] = IconSide::Right;
    s.playerIconSide[static_cast<int>(PlayerIcon::Gm)]        = IconSide::Right;
    s.playerIconSide[static_cast<int>(PlayerIcon::Away)]      = IconSide::Hidden;
    s.playerIconSide[static_cast<int>(PlayerIcon::Bazaar)]    = IconSide::Hidden;
    const PlayerIconRows rows = PlayerIcons(Everything(), s);
    CheckRow(rows.left, {Icon::Mentor, Icon::NewAdventurer, Icon::Linkshell, Icon::Invite});
    CheckRow(rows.right, {Icon::Gm, Icon::LevelSync});
    PlayerStatus synced;
    synced.levelSync = true;
    CheckRow(PlayerIcons(synced, s).left, {}); // only what the player has
    CheckRow(PlayerIcons(synced, s).right, {Icon::LevelSync});
}

TEST(level_sync_comes_from_a_characters_sync_packet)
{
    const auto synced = CharSync(0x02, 1105, 0x04);
    const auto update = ParseCharSync(synced.data(), static_cast<uint32_t>(synced.size()));
    CHECK(update && update->index == 1105 && update->synced);
    const auto both = CharSync(0x02, 1105, 0x06); // in a campaign battle too
    CHECK(ParseCharSync(both.data(), static_cast<uint32_t>(both.size()))->synced);
    const auto campaign = CharSync(0x02, 1105, 0x02);
    const auto off      = ParseCharSync(campaign.data(), static_cast<uint32_t>(campaign.size()));
    CHECK(off && off->index == 1105 && !off->synced);
    const auto named = CharSync(0x03, 1105, 0x04); // naming an entity sets the same byte
    CHECK(!ParseCharSync(named.data(), static_cast<uint32_t>(named.size())));
    CHECK(!ParseCharSync(synced.data(), 0x10)); // too short for the flags
    CHECK(!ParseCharSync(nullptr, 0x28));
}

TEST(your_level_sync_comes_from_your_buffs)
{
    int16_t buffs[kStatusIconSlots];
    for (int16_t& b : buffs)
        b = -1;
    CHECK(!LevelSyncInBuffs(buffs));
    buffs[0] = 13; // Slow: Level Sync's low byte alone
    buffs[3] = 269;
    CHECK(LevelSyncInBuffs(buffs));
    buffs[3] = 525; // the same low byte in the 512 range
    CHECK(!LevelSyncInBuffs(buffs));
    CHECK(!LevelSyncInBuffs(nullptr));
}

namespace
{
    // A party member's status icons as Phoenix's StatusEffectContainer::UpdateStatusIcons packs them for 0x076: each
    // icon's low byte in its slot, and in the mask bit 2 * slot for the 256 range and the next bit for the 512 range.
    struct PartyIcons
    {
        uint8_t icons[kStatusIconSlots];
        uint64_t bitMask = 0;
        PartyIcons() { std::memset(icons, 0xFF, sizeof(icons)); }
        void Put(int slot, uint16_t icon)
        {
            icons[slot] = static_cast<uint8_t>(icon);
            if (icon >= 256 && icon < 512) bitMask |= uint64_t{1} << (slot * 2);
            if (icon >= 512) bitMask |= uint64_t{1} << (slot * 2 + 1);
        }
    };
}

TEST(a_party_members_level_sync_comes_from_their_status_icons)
{
    PartyIcons none;
    CHECK(!LevelSyncInPartyIcons(none.icons, none.bitMask));
    PartyIcons first;
    first.Put(0, 269);
    CHECK(LevelSyncInPartyIcons(first.icons, first.bitMask));
    PartyIcons last; // the mask's top bits: a 64-bit shift
    last.Put(0, 13);
    last.Put(31, 269);
    CHECK(LevelSyncInPartyIcons(last.icons, last.bitMask));
    PartyIcons slow;
    slow.Put(5, 13);
    CHECK(!LevelSyncInPartyIcons(slow.icons, slow.bitMask));
    PartyIcons high;
    high.Put(5, 525);
    CHECK(!LevelSyncInPartyIcons(high.icons, high.bitMask));
    CHECK(!LevelSyncInPartyIcons(nullptr, first.bitMask));
}

TEST(seeking_bazaar_and_linkshell_come_from_the_games_render_flags)
{
    // Render.Flags1, Flags2 and the linkshell color (blue, green, red) of players in a capture, beside what the packets
    // said: Maryel had a bazaar and a linkshell, Rednecktech was seeking a party, Xhendrole both seeking and in a
    // linkshell.
    const PlayerStatus maryel = StatusFromRender(0x08000800, 0xA0020201, 0x000F0F0F);
    CHECK(maryel.bazaar && maryel.linkshell && !maryel.seekingParty);
    CHECK_EQ(maryel.linkshellArgb, 0xFF0F0F0Fu);
    const PlayerStatus rednecktech = StatusFromRender(0x02100800, 0xA0020001, 0);
    CHECK(rednecktech.seekingParty && !rednecktech.bazaar && !rednecktech.linkshell);
    const PlayerStatus xhendrole = StatusFromRender(0x08100800, 0xA0020001, 0x00FF5FFF);
    CHECK(xhendrole.seekingParty && xhendrole.linkshell && !xhendrole.bazaar);
    CHECK_EQ(xhendrole.linkshellArgb, 0xFFFF5FFFu);
    const PlayerStatus shio = StatusFromRender(0x0A000800, 0xA0020201, 0x009FAF0F);
    CHECK_EQ(shio.linkshellArgb, 0xFF0FAF9Fu); // red is the low byte
}

TEST(the_rest_comes_from_the_packets_once_known)
{
    // Memory is current for seeking, bazaar and linkshell; the packet, when seen, for the rest.
    PlayerStatus memory;
    memory.seekingParty = memory.bazaar = memory.linkshell = true;
    memory.linkshellArgb = 0xFF102030;
    PlayerStatus packet;
    packet.away = packet.mentor = packet.newAdventurer = packet.gm = true;
    packet.linkshellArgb = 0xFF405060;
    const PlayerStatus both = WithPacketStatus(memory, &packet);
    CheckStatus(both, {true, true, true, true, true, true, true});
    CHECK_EQ(both.linkshellArgb, 0xFF102030u);
    const PlayerStatus stale = WithPacketStatus(packet, &memory); // the other way round, every field differs
    CheckStatus(stale, {false, false, false, false, false, false, false});
    CheckStatus(WithPacketStatus(memory, nullptr), {true, true, true, false, false, false, false});
}
