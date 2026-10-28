#include "game_packets.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    std::vector<uint8_t> Packet(size_t size, std::initializer_list<std::pair<size_t, uint16_t>> words)
    {
        std::vector<uint8_t> data(size, 0);
        for (const auto& [offset, value] : words)
            std::memcpy(data.data() + offset, &value, sizeof(value));
        return data;
    }
}

TEST(a_zone_in_names_the_zones_weather)
{
    const std::vector<uint8_t> in = Packet(0x100, {{0x30, 103}, {0x68, 6}}); // zone 103, rain
    CHECK(ParseZoneInWeather(in.data(), static_cast<uint32_t>(in.size())) == Weather::Rain);
    CHECK(!ParseZoneInWeather(in.data(), 0x69).has_value()); // too short for the weather
    const std::vector<uint8_t> odd = Packet(0x100, {{0x30, 103}, {0x68, 25}});
    CHECK(!ParseZoneInWeather(odd.data(), 0x100).has_value());
}

TEST(the_skills_packet_holds_each_crafts_rank_in_its_low_bits)
{
    // 0x062: two bytes a skill from 0x80; Fishing is skill 48, Cooking 56. A craft's rank is its low 5 bits, its level
    // above them, and its cap flag the top bit (the SDK's craftskill_t).
    const auto at = [](int skill) { return static_cast<size_t>(0x80 + 2 * skill); };
    const std::vector<uint8_t> skills = Packet(0x100, {{at(48), (23 << 5) | 3}, {at(56), (5 << 5) | 1},
                                                          {at(52), 0x8000 | (100 << 5) | 10}});
    const std::optional<CraftRanks> ranks = ParseCraftRanks(skills.data(), static_cast<uint32_t>(skills.size()));
    CHECK(ranks.has_value());
    CHECK((*ranks)[0] == 3 && (*ranks)[8] == 1 && (*ranks)[4] == 10 && (*ranks)[1] == 0);
    CHECK(!ParseCraftRanks(skills.data(), static_cast<uint32_t>(at(56) + 1)).has_value()); // too short for Cooking
}

TEST(a_weather_change_names_the_new_weather)
{
    const std::vector<uint8_t> change = Packet(0x0C, {{0x08, 14}});
    CHECK(ParseWeather(change.data(), 0x0C) == Weather::Thunder);
    CHECK(!ParseWeather(change.data(), 0x09).has_value());
    const std::vector<uint8_t> odd = Packet(0x0C, {{0x08, 99}});
    CHECK(!ParseWeather(odd.data(), 0x0C).has_value());
}

TEST(a_conquest_update_names_each_regions_owner)
{
    std::vector<uint8_t> data(0xB4, 0);
    data[0x1D + 4 * 0] = 1;   // Ronfaure: San d'Oria
    data[0x1D + 4 * 1] = 2;   // Zulkheim: Bastok
    data[0x1D + 4 * 18] = 4;  // Tavnazia: beastmen
    data[0x1D + 4 * 2]  = 9;  // nonsense: no one
    const std::optional<RegionOwners> owners = ParseConquest(data.data(), static_cast<uint32_t>(data.size()));
    CHECK(owners.has_value());
    CHECK((*owners)[0] == Nation::SanDoria && (*owners)[1] == Nation::Bastok && (*owners)[18] == Nation::Beastmen);
    CHECK((*owners)[2] == Nation::None && (*owners)[3] == Nation::None);
    CHECK(!ParseConquest(data.data(), 0x65).has_value());
    CHECK(std::string(NationName(Nation::SanDoria)) == "San d'Oria" && std::string(NationName(Nation::None)).empty());
}

// West Ronfaure's update of 2026-10-10 as /hu debug wrote it: 3D 15 0B at 0x86 and 05 at 0x94, with San d'Oria
// first of the three nations in Ronfaure's ranking byte (39 at 0x1A).
TEST(a_conquest_update_ranks_the_influence_in_your_region)
{
    std::vector<uint8_t> data(0xB4, 0);
    data[0x86] = 0x3D, data[0x87] = 0x15, data[0x88] = 0x0B, data[0x94] = 0x05;
    data[0x89] = 0x40, data[0x8A] = 0x17, data[0x8B] = 0x0B; // the nations' shares without the beastmen, unread
    CHECK(ParseRegionShares(data.data(), 0xB4) ==
          (RegionShares{{{Nation::SanDoria, 61}, {Nation::Bastok, 21}, {Nation::Windurst, 11}, {Nation::Beastmen, 5}}}));
    CHECK(ParseRegionShares(data.data(), 0x95).has_value()); // through the beastmen's byte
    CHECK(!ParseRegionShares(data.data(), 0x94).has_value());
    CHECK(!ParseRegionShares(nullptr, 0xB4).has_value());
}

TEST(the_largest_share_of_influence_comes_first_and_equal_shares_stay_in_the_nations_order)
{
    std::vector<uint8_t> data(0xB4, 0);
    data[0x86] = 3, data[0x87] = 4, data[0x88] = 3, data[0x94] = 89;
    CHECK(ParseRegionShares(data.data(), 0xB4) ==
          (RegionShares{{{Nation::Beastmen, 89}, {Nation::Bastok, 4}, {Nation::SanDoria, 3}, {Nation::Windurst, 3}}}));
    data[0x94] = 91; // 101 in all: not shares, so not the layout HeadsUp expects
    CHECK(!ParseRegionShares(data.data(), 0xB4).has_value());
    data = std::vector<uint8_t>(0xB4, 0); // a week with no influence yet, or a zone outside the conquest
    CHECK(ParseRegionShares(data.data(), 0xB4) ==
          (RegionShares{{{Nation::SanDoria, 0}, {Nation::Bastok, 0}, {Nation::Windurst, 0}, {Nation::Beastmen, 0}}}));
}

// The weather ids are whole 16-bit words, which a set high byte takes past every weather; each packet is read up to
// its last byte and no further; and no packet reads as nothing.
TEST(packets_are_read_as_whole_words_up_to_their_last_byte_and_never_from_nothing)
{
    const std::vector<uint8_t> in = Packet(0x6A, {{0x68, 6}});
    CHECK(ParseZoneInWeather(in.data(), 0x6A) == Weather::Rain);
    const std::vector<uint8_t> inHigh = Packet(0x6A, {{0x68, 0x0106}});
    CHECK(!ParseZoneInWeather(inHigh.data(), 0x6A).has_value());
    const std::vector<uint8_t> change = Packet(0x0A, {{0x08, 6}});
    CHECK(ParseWeather(change.data(), 0x0A) == Weather::Rain);
    const std::vector<uint8_t> changeHigh = Packet(0x0A, {{0x08, 0x0106}});
    CHECK(!ParseWeather(changeHigh.data(), 0x0A).has_value());
    std::vector<uint8_t> conquest(0x66, 0);
    conquest[0x1D + 4 * 18] = 3; // Tavnazia: Windurst, in the last byte read
    const std::optional<RegionOwners> owners = ParseConquest(conquest.data(), 0x66);
    CHECK(owners.has_value() && (*owners)[18] == Nation::Windurst);
    CHECK(!ParseZoneInWeather(nullptr, 0x100).has_value() && !ParseWeather(nullptr, 0x100).has_value());
    CHECK(!ParseConquest(nullptr, 0x100).has_value());
}

namespace
{
    // A check's general packet: Tanyrus (server id 0x01006A2B, index 0x410), WAR 75 / NIN 37, in Phoenix's linkshell.
    std::vector<uint8_t> InspectGeneralPacket(uint8_t mode = 0x01)
    {
        std::vector<uint8_t> p(0x54, 0);
        const uint32_t id = 0x01006A2B;
        const uint16_t index = 0x410;
        std::memcpy(p.data() + 0x04, &id, sizeof(id));
        std::memcpy(p.data() + 0x08, &index, sizeof(index));
        p[0x0A] = mode;
        const uint8_t phoenix[] = {0xA8, 0x83, 0xC5, 0x38, 0x96, 0x3F};
        std::memcpy(p.data() + 0x10, phoenix, sizeof(phoenix));
        p[0x22] = 1, p[0x23] = 13, p[0x24] = 75, p[0x25] = 37, p[0x26] = 1;
        return p;
    }
}

TEST(a_linkshell_name_unpacks_from_six_bit_codes)
{
    const uint8_t phoenix[] = {0xA8, 0x83, 0xC5, 0x38, 0x96, 0x3F}, ls2[] = {0x9A, 0xDD, 0xFF},
        letters[] = {0x04, 0x20, 0xC4, 0x14, 0x61, 0xC8, 0x24, 0xA2, 0xCC, 0x34, 0xE3, 0xD0, 0x45, 0x24, 0xD4, 0xFC};
    CHECK_EQ(LinkshellName(phoenix, sizeof(phoenix)), std::string("Phoenix"));
    CHECK_EQ(LinkshellName(ls2, sizeof(ls2)), std::string("LS2"));
    CHECK_EQ(LinkshellName(letters, sizeof(letters)), std::string("abcdefghijklmnopqrst")); // 20 letters, the most
    const uint8_t none[16] = {};
    CHECK(LinkshellName(none, sizeof(none)).empty() && LinkshellName(nullptr, 16).empty());
}

TEST(a_checks_general_packet_carries_the_jobs_levels_and_linkshell)
{
    const std::vector<uint8_t> p = InspectGeneralPacket();
    CHECK(ParseInspectGeneral(p.data(), static_cast<uint32_t>(p.size())) ==
          (InspectGeneral{0x01006A2B, 0x410, "Phoenix", 1, 13, 75, 37, 0}));
    CHECK(!ParseInspectGeneral(p.data(), 0x28).has_value()); // short of the master flags
    const std::vector<uint8_t> equipment = InspectGeneralPacket(0x03);
    CHECK(!ParseInspectGeneral(equipment.data(), static_cast<uint32_t>(equipment.size())).has_value());
    CHECK(!ParseInspectGeneral(nullptr, 0x54).has_value());
}

TEST(a_checks_message_carries_the_name_and_title)
{
    std::vector<uint8_t> p(0x94, 0);
    std::memcpy(p.data() + 0x80, "Tanyrus", 7);
    const uint32_t title = 206; // NEW_ADVENTURER in Phoenix's title.lua
    std::memcpy(p.data() + 0x90, &title, sizeof(title));
    CHECK(ParseInspectMessage(p.data(), 0x94) == (InspectMessage{"Tanyrus", 206}));
    std::memset(p.data() + 0x80, 'x', 16); // a 16-letter name fills its field with no NUL
    CHECK_EQ(ParseInspectMessage(p.data(), 0x94)->name, std::string(16, 'x'));
    CHECK(!ParseInspectMessage(p.data(), 0x93).has_value());
}

// Each length from 1 to 20, as Phoenix's EncodeStringLinkshell packs it into the item's 15-byte Name and the packet's
// 16-byte field. A length of 2, 6, 10, 14 or 18 ends on 4 one bits, which read on as a code of 60 ('7') before a 0.
TEST(a_linkshell_name_of_every_length_unpacks_as_phoenix_packed_it)
{
    const struct
    {
        const char* name;
        uint8_t packed[16];
    } names[] = {
        {"V", {0xC3, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"Va", {0xC0, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"Van", {0xC0, 0x13, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"Vana", {0xC0, 0x13, 0x81, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaD", {0xC0, 0x13, 0x81, 0x7B, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDi", {0xC0, 0x13, 0x81, 0x78, 0x9F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDie", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDiel", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDiel2", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDiel20", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDiel202", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDiel2026", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDiel2026E", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDiel2026Ex", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0x7D, 0x8F, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDiel2026Exp", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0x7D, 0x84, 0x3F, 0x00, 0x00, 0x00, 0x00}},
        {"VanaDiel2026Expl", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0x7D, 0x84, 0x0C, 0xFC, 0x00, 0x00, 0x00}},
        {"VanaDiel2026Explo", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0x7D, 0x84, 0x0C, 0x3F, 0xF0, 0x00, 0x00}},
        {"VanaDiel2026Explor", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0x7D, 0x84, 0x0C, 0x3D, 0x2F, 0x00, 0x00}},
        {"VanaDiel2026Explore", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0x7D, 0x84, 0x0C, 0x3D, 0x21, 0x7F, 0x00}},
        {"VanaDiel2026Explorer", {0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0x7D, 0x84, 0x0C, 0x3D, 0x21, 0x52, 0x00}},
    };
    for (const auto& n : names)
        CHECK_EQ(LinkshellName(n.packed, sizeof(n.packed)), std::string(n.name));
    const uint8_t ls[] = {0x9A, 0xDF, 0x00};
    CHECK_EQ(LinkshellName(ls, sizeof(ls)), std::string("LS"));
}

namespace
{
    constexpr size_t kItemExtraBytes = 28; // Ashita's item_t::Extra

    // A linkshell item's extra data as Phoenix's Exdata::Linkshell lays it out: the group's id and key, its color and
    // its kind (a shell, a sack or a pearl), then the packed name's 15 bytes.
    std::array<uint8_t, kItemExtraBytes> LinkshellExtra(std::initializer_list<uint8_t> packedName)
    {
        std::array<uint8_t, kItemExtraBytes> extra{};
        const uint32_t groupId = 0x00001A2B;
        const uint16_t groupKey = 0x0304, color = 0xF8C4;
        std::memcpy(extra.data(), &groupId, sizeof(groupId));
        std::memcpy(extra.data() + 4, &groupKey, sizeof(groupKey));
        std::memcpy(extra.data() + 6, &color, sizeof(color));
        extra[8] = 1;
        std::copy(packedName.begin(), packedName.end(), extra.begin() + 9);
        return extra;
    }
}

TEST(your_linkshells_name_comes_from_the_linkshell_item_you_wear)
{
    // Phoenix's ItemLockFlg: 0x13 on a linkshell you wear, 0x05 on other gear you wear, 0x19 on an item in your bazaar.
    constexpr uint32_t kWornLinkshell = 0x13, kWornGear = 0x05, kInBazaar = 0x19;
    const auto phoenix = LinkshellExtra({0xA8, 0x83, 0xC5, 0x38, 0x96, 0x3F});
    CHECK_EQ(WornLinkshellName(kWornLinkshell, phoenix.data(), phoenix.size()), std::string("Phoenix"));
    for (const uint32_t flags : {uint32_t{0}, kWornGear, kInBazaar}) // a shell in your bag is not the one you wear
        CHECK(WornLinkshellName(flags, phoenix.data(), phoenix.size()).empty());
    // The longest name fills the item's 15 bytes.
    const auto longest =
        LinkshellExtra({0xC0, 0x13, 0x81, 0x78, 0x91, 0x4C, 0xDF, 0x5D, 0xFB, 0x7D, 0x84, 0x0C, 0x3D, 0x21, 0x52});
    CHECK_EQ(WornLinkshellName(kWornLinkshell, longest.data(), longest.size()), std::string("VanaDiel2026Explorer"));
    CHECK(WornLinkshellName(kWornLinkshell, longest.data(), 23).empty()); // short of the name's last byte
    CHECK(WornLinkshellName(kWornLinkshell, nullptr, kItemExtraBytes).empty());
}

TEST(a_containers_items_are_looked_for_within_the_slots_the_client_keeps)
{
    // Ashita's items_t holds 81 slots, 0 to 80; slot 0 holds no item but the inventory's gil.
    CHECK_EQ(LastItemSlot(30), 30u); // a new character's inventory
    CHECK_EQ(LastItemSlot(80), 80u); // the most a container holds
    CHECK_EQ(LastItemSlot(81), 80u);
    CHECK_EQ(LastItemSlot(255), 80u); // a capacity the game never gives: no read past the array
    CHECK_EQ(LastItemSlot(0), 0u);    // a container you have no room in: no slot from 1 on
}

TEST(the_conquest_request_is_a_bare_header_naming_0x05a_and_one_word)
{
    const auto word = static_cast<uint16_t>(kConquestRequest[0] | kConquestRequest[1] << 8);
    CHECK((word & 0x1FF) == kConquestRequestPacket);
    CHECK(static_cast<size_t>(word >> 9) * 4 == kConquestRequest.size()); // XiPackets: 0x0004 bytes
    CHECK(kConquestRequest[2] == 0 && kConquestRequest[3] == 0);         // the sync word, the game's to fill
}
