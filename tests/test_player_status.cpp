#include "player_status.h"
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

    constexpr uint8_t kGeneral = 0x04, kDespawn = 0x20;
}

TEST(another_players_update_gives_their_icons)
{
    // Seeking a party (bit 11), a linkshell (bit 17) and a bazaar (bit 31), with the linkshell's color.
    const auto p = OtherPlayer(1137, kGeneral, 1u << 11 | 1u << 17 | 1u << 31, 0x00FF1F8F, 0);
    const auto update = ParseOtherPlayer(p.data(), static_cast<uint32_t>(p.size()));
    CHECK(update.has_value());
    CHECK_EQ(update->index, 1137);
    CHECK(!update->despawn && update->status.has_value());
    const PlayerStatus& s = *update->status;
    CHECK(s.seekingParty && s.linkshell && s.bazaar);
    CHECK(!s.away && !s.mentor && !s.newAdventurer && !s.gm);
    CHECK_EQ(s.linkshellArgb, 0xFF8F1FFFu);
}

TEST(away_gm_mentor_and_new_adventurer_come_from_their_bits)
{
    // Away (bit 14) and GM level 2 (bits 24-26) in the first word; new adventurer (23) and mentor (24) in the third.
    const auto p = OtherPlayer(1138, kGeneral, 1u << 14 | 2u << 24, 0, 1u << 23 | 1u << 24);
    const PlayerStatus s = *ParseOtherPlayer(p.data(), static_cast<uint32_t>(p.size()))->status;
    CHECK(s.away && s.gm && s.newAdventurer && s.mentor);
    CHECK(!s.seekingParty && !s.bazaar && !s.linkshell);
}

TEST(a_position_only_update_or_a_despawn_carries_no_status)
{
    const auto moved = OtherPlayer(1137, 0x01, 1u << 31, 0, 0);
    const auto update = ParseOtherPlayer(moved.data(), static_cast<uint32_t>(moved.size()));
    CHECK(update.has_value() && !update->despawn && !update->status.has_value());
    const auto gone = OtherPlayer(1137, kDespawn, 0, 0, 0);
    CHECK(ParseOtherPlayer(gone.data(), static_cast<uint32_t>(gone.size()))->despawn);
    CHECK(!ParseOtherPlayer(gone.data(), 0x20).has_value()); // cut short
    CHECK(!ParseOtherPlayer(nullptr, 0).has_value());
}

TEST(your_own_status_has_its_own_layout)
{
    // Seeking (bit 4), away (7), a linkshell (25) and GM level 1 (29-31); a bazaar is bit 29 of the second word, and
    // new adventurer (3) and mentor (4) are in the fourth.
    const auto p = OwnStatus(1u << 4 | 1u << 7 | 1u << 25 | 1u << 29, 1u << 29, 0x2F, 0xCF, 0x6F, 1u << 3 | 1u << 4);
    const auto s = ParseOwnStatus(p.data(), static_cast<uint32_t>(p.size()));
    CHECK(s.has_value());
    CHECK(s->seekingParty && s->away && s->linkshell && s->gm && s->bazaar && s->newAdventurer && s->mentor);
    CHECK_EQ(s->linkshellArgb, 0xFF2FCF6Fu);
    CHECK(!ParseOwnStatus(p.data(), 0x38).has_value());
}

TEST(player_icons_in_order)
{
    CHECK_EQ(PlayerIcons(PlayerStatus{}).count, 0);
    PlayerStatus all;
    all.seekingParty = all.bazaar = all.linkshell = all.away = all.mentor = all.newAdventurer = all.gm = true;
    const IconSet icons = PlayerIcons(all);
    const Icon expected[] = {Icon::Gm, Icon::Mentor, Icon::NewAdventurer, Icon::Away, Icon::Linkshell, Icon::Bazaar,
        Icon::Invite};
    CHECK_EQ(icons.count, 7);
    for (int i = 0; i < 7; ++i)
        CHECK(icons.icons[i] == expected[i]);
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
    PlayerStatus memory;
    memory.bazaar = true;
    PlayerStatus packet;
    packet.away = packet.mentor = true;
    packet.bazaar = false; // memory is current; a packet may be older
    const PlayerStatus both = WithPacketStatus(memory, &packet);
    CHECK(both.bazaar && both.away && both.mentor && !both.gm);
    CHECK(WithPacketStatus(memory, nullptr).bazaar);
}
