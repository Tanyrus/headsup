#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace headsup
{
    // Client to server: GP_CLI_COMMAND_CURRENCIES_1 and _2, which the game sends when its Currencies menu opens.
    // Phoenix answers each with kCurrencies1Packet or kCurrencies2Packet (0x10f_currencies_1.cpp,
    // 0x115_currencies_2.cpp). Their bytes as kConquestRequest's: the header alone.
    constexpr uint16_t kCurrencies1Request = 0x10F, kCurrencies2Request = 0x115;
    constexpr std::array<uint8_t, 4> kCurrencies1RequestBytes = {0x0F, 0x03, 0x00, 0x00};
    constexpr std::array<uint8_t, 4> kCurrencies2RequestBytes = {0x15, 0x03, 0x00, 0x00};
    constexpr uint16_t kCurrencies1Packet = 0x113, kCurrencies2Packet = 0x118;

    struct CurrencyInfo
    {
        const char* key;  // as the settings write it
        const char* name; // as the game's Currencies menu names it
        uint16_t packet;  // 0 for gil, which the inventory holds
        uint16_t offset;  // from the packet's start, Ashita's 4-byte header counted
        uint8_t bytes;    // 2 or 4, little-endian; a 4-byte one is signed and below 0 reads as 0
    };
    // Gil first; the rest in the order of Phoenix's GP_SERV_COMMAND_CURRENCIES_1 and _2.
    inline constexpr CurrencyInfo kCurrencyTable[] = {
        {"gil", "Gil", 0, 0, 0},
        {"cp_sandoria", "Conquest Points (San d'Oria)", kCurrencies1Packet, 0x04, 4},
        {"cp_bastok", "Conquest Points (Bastok)", kCurrencies1Packet, 0x08, 4},
        {"cp_windurst", "Conquest Points (Windurst)", kCurrencies1Packet, 0x0C, 4},
        {"beastmen_seals", "Beastmen's Seals", kCurrencies1Packet, 0x10, 2},
        {"kindred_seals", "Kindred's Seals", kCurrencies1Packet, 0x12, 2},
        {"kindred_crests", "Kindred's Crests", kCurrencies1Packet, 0x14, 2},
        {"high_kindred_crests", "High Kindred's Crests", kCurrencies1Packet, 0x16, 2},
        {"sacred_kindred_crests", "Sacred Kindred's Crests", kCurrencies1Packet, 0x18, 2},
        {"ancient_beastcoins", "Ancient Beastcoins", kCurrencies1Packet, 0x1A, 2},
        {"valor_points", "Valor Points", kCurrencies1Packet, 0x1C, 2},
        {"scylds", "Scylds", kCurrencies1Packet, 0x1E, 2},
        {"guild_fishing", "Guild Points (Fishing)", kCurrencies1Packet, 0x20, 4},
        {"guild_woodworking", "Guild Points (Woodworking)", kCurrencies1Packet, 0x24, 4},
        {"guild_smithing", "Guild Points (Smithing)", kCurrencies1Packet, 0x28, 4},
        {"guild_goldsmithing", "Guild Points (Goldsmithing)", kCurrencies1Packet, 0x2C, 4},
        {"guild_clothcraft", "Guild Points (Clothcraft)", kCurrencies1Packet, 0x30, 4},
        {"guild_leathercraft", "Guild Points (Leathercraft)", kCurrencies1Packet, 0x34, 4},
        {"guild_bonecraft", "Guild Points (Bonecraft)", kCurrencies1Packet, 0x38, 4},
        {"guild_alchemy", "Guild Points (Alchemy)", kCurrencies1Packet, 0x3C, 4},
        {"guild_cooking", "Guild Points (Cooking)", kCurrencies1Packet, 0x40, 4},
        {"cinders", "Cinders", kCurrencies1Packet, 0x44, 4},
        {"ballista_points", "Ballista Points", kCurrencies1Packet, 0x50, 4},
        {"fellow_points", "Fellow Points", kCurrencies1Packet, 0x54, 4},
        {"daily_tally", "Daily Tally", kCurrencies1Packet, 0x5E, 2},
        {"research_marks", "Research Marks", kCurrencies1Packet, 0x60, 4},
        {"moblin_marbles", "Moblin Marbles", kCurrencies1Packet, 0x68, 4},
        {"infamy", "Infamy", kCurrencies1Packet, 0x6C, 2},
        {"prestige", "Prestige", kCurrencies1Packet, 0x6E, 2},
        {"legion_points", "Legion Points", kCurrencies1Packet, 0x70, 4},
        {"sparks", "Sparks of Eminence", kCurrencies1Packet, 0x74, 4},
        {"shining_stars", "Shining Stars", kCurrencies1Packet, 0x78, 4},
        {"imperial_standing", "Imperial Standing", kCurrencies1Packet, 0x7C, 4},
        {"assault_leujaoam", "Assault Points (Leujaoam)", kCurrencies1Packet, 0x80, 4},
        {"assault_mamool", "Assault Points (Mamool Ja)", kCurrencies1Packet, 0x84, 4},
        {"assault_lebros", "Assault Points (Lebros)", kCurrencies1Packet, 0x88, 4},
        {"assault_periqia", "Assault Points (Periqia)", kCurrencies1Packet, 0x8C, 4},
        {"assault_ilrusi", "Assault Points (Ilrusi)", kCurrencies1Packet, 0x90, 4},
        {"nyzul_tokens", "Nyzul Isle Tokens", kCurrencies1Packet, 0x94, 4},
        {"zeni", "Zeni", kCurrencies1Packet, 0x98, 4},
        {"jettons", "Jettons", kCurrencies1Packet, 0x9C, 4},
        {"therion_ichor", "Therion Ichor", kCurrencies1Packet, 0xA0, 4},
        {"allied_notes", "Allied Notes", kCurrencies1Packet, 0xA4, 4},
        {"login_points", "Login Points", kCurrencies1Packet, 0xAA, 2},
        {"cruor", "Cruor", kCurrencies1Packet, 0xAC, 4},
        {"resistance_credits", "Resistance Credits", kCurrencies1Packet, 0xB0, 4},
        {"dominion_notes", "Dominion Notes", kCurrencies1Packet, 0xB4, 4},
        {"traverser_stones", "Traverser Stones", kCurrencies1Packet, 0xC0, 4},
        {"voidstones", "Voidstones", kCurrencies1Packet, 0xC4, 4},
        {"unity_accolades", "Unity Accolades", kCurrencies1Packet, 0xE4, 4},
        {"deeds", "Deeds", kCurrencies1Packet, 0xF8, 2},
        {"bayld", "Bayld", kCurrencies2Packet, 0x04, 4},
        {"escha_beads", "Escha Beads", kCurrencies2Packet, 0x4A, 2},
        {"escha_silt", "Escha Silt", kCurrencies2Packet, 0x4C, 4},
        {"potpourri", "Potpourri", kCurrencies2Packet, 0x50, 4},
        {"hallmarks", "Hallmarks", kCurrencies2Packet, 0x54, 4},
        {"gallantry", "Badges of Gallantry", kCurrencies2Packet, 0x5C, 4},
        {"crafter_points", "Crafter Points", kCurrencies2Packet, 0x60, 4},
        {"domain_points", "Domain Points", kCurrencies2Packet, 0x84, 4},
        {"mog_segments", "Mog Segments", kCurrencies2Packet, 0x8C, 4},
        {"gallimaufry", "Gallimaufry", kCurrencies2Packet, 0x90, 4},
        {"temenos_units", "Temenos Units", kCurrencies2Packet, 0x98, 4},
        {"apollyon_units", "Apollyon Units", kCurrencies2Packet, 0x9C, 4},
    };
    constexpr int kCurrencyCount = static_cast<int>(std::size(kCurrencyTable));
    constexpr int kGilCurrency   = 0;

    // Each currency's amount by kCurrencyTable's order: nothing until its packet came.
    using CurrencyValues = std::array<std::optional<uint32_t>, kCurrencyCount>;
    // Takes the amounts a kCurrencies1Packet or kCurrencies2Packet holds; another packet, or one too short for the
    // last of them, changes nothing.
    void ReadCurrencies(CurrencyValues& values, uint16_t id, const uint8_t* data, uint32_t size);
    // The currency a key names; none for one no currency has.
    std::optional<int> CurrencyOfKey(std::string_view key);
}
