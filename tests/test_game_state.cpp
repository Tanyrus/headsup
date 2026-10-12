#include "game_state.h"
#include "test.h"

#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

using namespace headsup;

namespace
{
    constexpr uint16_t kSelf = 1046, kOther = 1137, kSecond = 1105; // target indexes: yours and two other players'
    constexpr uint32_t kMob  = 0x01067220;                         // Goblin Bounty Hunter, target index 0x220

    // A packet as Ashita hands it over: its id beside its bytes, which count Ashita's 4-byte header.
    struct Packet
    {
        uint16_t id;
        std::vector<uint8_t> bytes;

        template <typename T>
        Packet& Put(size_t offset, T value)
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
            return *this;
        }
    };

    Packet Blank(uint16_t id, size_t size) { return Packet{id, std::vector<uint8_t>(size, 0)}; }
    Packet Cut(Packet p, size_t size)
    {
        p.bytes.resize(size);
        return p;
    }

    // Phoenix's s2c 0x00A: the zone at 0x30 (103, Valkurm Dunes) and its weather at 0x68.
    Packet ZoneIn(uint16_t weather) { return Blank(0x00A, 0x100).Put<uint16_t>(0x30, 103).Put(0x68, weather); }
    // s2c 0x057: the weather at 0x08.
    Packet WeatherChange(uint16_t weather) { return Blank(0x057, 0x0C).Put(0x08, weather); }
    // s2c 0x05E: region r's owner byte at 0x1D + 4r.
    Packet Conquest(std::initializer_list<std::pair<size_t, uint8_t>> owners)
    {
        Packet p = Blank(0x05E, 0xB4);
        for (const auto& [region, owner] : owners)
            p.Put(0x1D + 4 * region, owner);
        return p;
    }
    // GP_SERV_CHAR_PC (0x00D) up to its third flags word: the index at 0x08, what it carries at 0x0A, the first flags
    // word at 0x20.
    Packet OtherPlayer(uint16_t index, uint8_t sends, uint32_t flags1)
    {
        return Blank(0x00D, 0x2C).Put(0x08, index).Put(0x0A, sends).Put(0x20, flags1);
    }
    // GP_SERV_SERVERSTATUS (0x037) up to its fourth flags word: the first at 0x28.
    Packet OwnStatus(uint32_t flags0) { return Blank(0x037, 0x3C).Put(0x28, flags0); }
    // GP_SERV_ENTITY_SET (0x067): the kind at 0x04 (2 a character, 3 a named entity), the index at 0x06, flags at 0x10.
    Packet CharSync(uint8_t kind, uint16_t index, uint8_t flags)
    {
        return Blank(0x067, 0x28).Put(0x04, kind).Put(0x06, index).Put(0x10, flags);
    }
    // Message Basic (0x029) for a check of the goblin: level 22 at 0x0C, check type 0x43 (Decent Challenge) at 0x10.
    Packet CheckReplyWith(uint16_t message)
    {
        return Blank(0x029, 0x1C).Put(0x08, kMob).Put<int32_t>(0x0C, 22).Put<uint32_t>(0x10, 0x43)
            .Put<uint16_t>(0x16, 0x220).Put(0x18, message);
    }

    constexpr uint8_t kMoved = 0x01, kGeneral = 0x04, kDespawn = 0x20;   // what an 0x00D carries
    constexpr uint32_t kOtherSeeking = 1u << 11, kOtherAway = 1u << 14; // its first flags word
    constexpr uint32_t kOwnSeeking = 1u << 4, kOwnAway = 1u << 7;       // 0x037's first flags word
    constexpr uint8_t kCharacter = 0x02, kNamed = 0x03;                 // 0x067's kinds
    constexpr uint8_t kCampaign = 0x02, kLevelSync = 0x04;              // its flags
    constexpr uint16_t kRain = 6, kThunder = 14;
    constexpr uint16_t kCon = 0xAA, kDefeat = 0x06; // Message Basic's messages
    constexpr uint8_t kSanDoria = 1, kBastok = 2, kWindurst = 3, kBeastmen = 4;

    PacketFollowUp Feed(GameState& game, const Packet& p)
    {
        return game.Received(p.id, p.bytes.data(), static_cast<uint32_t>(p.bytes.size()));
    }

    // All a reader can ask of the state about the players above.
    struct Seen
    {
        bool other, otherAway, own, ownAway, otherSynced, selfSynced;
        std::optional<Weather> weather;
        std::optional<RegionOwners> owners;
        bool operator==(const Seen&) const = default;
    };

    Seen Look(const GameState& game)
    {
        const PlayerStatus* other = game.PlayerStatusOf(kOther, kSelf);
        const PlayerStatus* own   = game.PlayerStatusOf(kSelf, kSelf);
        return Seen{other != nullptr, other != nullptr && other->away, own != nullptr, own != nullptr && own->away,
            game.LevelSynced(kOther), game.LevelSynced(kSelf), game.ZoneWeather(), game.Owners()};
    }

    // Something of every kind to lose.
    GameState Filled()
    {
        GameState game;
        Feed(game, OtherPlayer(kOther, kGeneral, kOtherAway));
        Feed(game, OwnStatus(kOwnAway));
        Feed(game, CharSync(kCharacter, kOther, kLevelSync));
        Feed(game, CharSync(kCharacter, kSelf, kLevelSync));
        Feed(game, WeatherChange(kThunder));
        Feed(game, Conquest({{0, kSanDoria}, {18, kBeastmen}}));
        const Seen seen = Look(game);
        CHECK(seen.other && seen.otherAway && seen.own && seen.ownAway && seen.otherSynced && seen.selfSynced);
        CHECK(seen.weather == Weather::Thunder && seen.owners.has_value());
        return game;
    }
}

TEST(a_zone_in_forgets_other_players_and_everyones_sync_names_the_weather_and_says_so)
{
    GameState game            = Filled();
    const PacketFollowUp next = Feed(game, ZoneIn(kRain));
    CHECK(next.zonedIn && !next.check.has_value());
    CHECK(game.PlayerStatusOf(kOther, kSelf) == nullptr);
    CHECK(!game.LevelSynced(kOther) && !game.LevelSynced(kSelf));
    const PlayerStatus* own = game.PlayerStatusOf(kSelf, kSelf); // only the next 0x037 changes yours
    CHECK(own != nullptr && own->away);
    CHECK(game.ZoneWeather() == Weather::Rain);
}

TEST(a_zone_in_without_a_weather_it_names_clears_the_last_zones)
{
    GameState game;
    Feed(game, WeatherChange(kThunder));
    Feed(game, ZoneIn(25)); // no weather has that id
    CHECK(!game.ZoneWeather().has_value());
    // Target indexes are reused in the next zone, so one cut short before its weather still forgets the players.
    game = Filled();
    const PacketFollowUp cut = Feed(game, Cut(ZoneIn(kRain), 0x69));
    CHECK(cut.zonedIn && !game.ZoneWeather().has_value());
    CHECK(game.PlayerStatusOf(kOther, kSelf) == nullptr && !game.LevelSynced(kOther));
}

TEST(a_short_or_unknown_packet_changes_nothing_and_leaves_nothing_to_do)
{
    GameState game  = Filled();
    const Seen full = Look(game);
    // Each one byte short of what its parser reads, and carrying what would change the state; then a zone-in's bytes
    // under 0x00B, the zone-out, which HeadsUp does not read.
    const Packet packets[] = {
        Cut(WeatherChange(kRain), 0x09),
        Cut(Conquest({{0, kBastok}, {18, kWindurst}}), 0x65),
        Cut(OtherPlayer(kOther, kDespawn, 0), 0x2B),
        Cut(OwnStatus(0), 0x3B),
        Cut(CharSync(kCharacter, kOther, 0), 0x10),
        Cut(CheckReplyWith(kCon), 0x19),
        Packet{0x00B, ZoneIn(kRain).bytes},
    };
    for (const Packet& p : packets)
    {
        const PacketFollowUp next = Feed(game, p);
        CHECK(!next.zonedIn && !next.check.has_value());
        CHECK(Look(game) == full);
    }
}

TEST(the_weather_and_conquest_take_each_update_and_outlast_other_packets)
{
    GameState game;
    CHECK(!game.ZoneWeather().has_value() && !game.Owners().has_value());
    Feed(game, WeatherChange(kThunder));
    Feed(game, Conquest({{0, kSanDoria}, {1, kBastok}, {18, kBeastmen}}));
    for (const Packet& p : {OtherPlayer(kOther, kGeneral, kOtherAway), OwnStatus(kOwnAway),
             CharSync(kCharacter, kOther, kLevelSync), CheckReplyWith(kCon)})
        Feed(game, p);
    CHECK(game.ZoneWeather() == Weather::Thunder);
    CHECK(game.Owners().has_value());
    CHECK((*game.Owners())[0] == Nation::SanDoria && (*game.Owners())[1] == Nation::Bastok);
    CHECK((*game.Owners())[18] == Nation::Beastmen && (*game.Owners())[2] == Nation::None);
    Feed(game, WeatherChange(99)); // no weather has that id: the last one stands
    CHECK(game.ZoneWeather() == Weather::Thunder);
    Feed(game, WeatherChange(kRain));
    CHECK(game.ZoneWeather() == Weather::Rain);
    Feed(game, Conquest({{0, kWindurst}})); // the whole standings each time
    CHECK((*game.Owners())[0] == Nation::Windurst && (*game.Owners())[1] == Nation::None);
}

TEST(conquest_owners_outlast_a_zone_in)
{
    GameState game;
    Feed(game, Conquest({{0, kSanDoria}, {18, kBeastmen}}));
    Feed(game, ZoneIn(kRain));
    CHECK(game.Owners().has_value());
    CHECK((*game.Owners())[0] == Nation::SanDoria && (*game.Owners())[18] == Nation::Beastmen);
}

TEST(a_players_despawn_forgets_their_status_and_sync_and_no_one_elses)
{
    GameState game;
    Feed(game, OtherPlayer(kOther, kGeneral, kOtherAway));
    Feed(game, CharSync(kCharacter, kOther, kLevelSync));
    Feed(game, OtherPlayer(kSecond, kGeneral, kOtherSeeking));
    Feed(game, CharSync(kCharacter, kSecond, kLevelSync));
    Feed(game, OtherPlayer(kOther, kMoved, 0)); // a move carries no status, and keeps the last
    const PlayerStatus* moved = game.PlayerStatusOf(kOther, kSelf);
    CHECK(moved != nullptr && moved->away);
    Feed(game, OtherPlayer(kOther, static_cast<uint8_t>(kDespawn | kGeneral), kOtherAway)); // over the flags beside it
    CHECK(game.PlayerStatusOf(kOther, kSelf) == nullptr && !game.LevelSynced(kOther));
    const PlayerStatus* second = game.PlayerStatusOf(kSecond, kSelf);
    CHECK(second != nullptr && second->seekingParty && game.LevelSynced(kSecond));
}

TEST(a_characters_sync_marks_and_clears_their_level_sync)
{
    GameState game;
    Feed(game, CharSync(kCharacter, kOther, kLevelSync));
    Feed(game, CharSync(kCharacter, kSelf, kLevelSync)); // yours comes the same way
    CHECK(game.LevelSynced(kOther) && game.LevelSynced(kSelf));
    CHECK(game.PlayerStatusOf(kOther, kSelf) == nullptr);
    Feed(game, CharSync(kCharacter, kOther, kCampaign)); // in a campaign battle and no longer synced
    CHECK(!game.LevelSynced(kOther) && game.LevelSynced(kSelf));
    Feed(game, CharSync(kNamed, kOther, kLevelSync)); // naming an entity sets the same byte
    CHECK(!game.LevelSynced(kOther));
}

TEST(your_own_status_is_kept_apart_from_other_players)
{
    GameState game;
    Feed(game, OtherPlayer(kSelf, kGeneral, kOtherAway)); // yours comes from 0x037 alone
    CHECK(game.PlayerStatusOf(kSelf, kSelf) == nullptr);
    Feed(game, OwnStatus(kOwnSeeking));
    Feed(game, OtherPlayer(kOther, kGeneral, kOtherAway));
    const PlayerStatus* own = game.PlayerStatusOf(kSelf, kSelf);
    CHECK(own != nullptr && own->seekingParty && !own->away);
    const PlayerStatus* other = game.PlayerStatusOf(kOther, kSelf);
    CHECK(other != nullptr && other->away && !other->seekingParty);
    Feed(game, OwnStatus(kOwnAway));
    own = game.PlayerStatusOf(kSelf, kSelf);
    CHECK(own != nullptr && own->away && !own->seekingParty);
}

TEST(a_check_reply_is_handed_back_for_the_plugin_to_finish)
{
    GameState game            = Filled();
    const Seen full           = Look(game);
    const PacketFollowUp next = Feed(game, CheckReplyWith(kCon));
    CHECK(next.check.has_value() && !next.zonedIn);
    CHECK_EQ(next.check->serverId, kMob);
    CHECK_EQ(next.check->targetIndex, 0x220);
    CHECK_EQ(next.check->result.level, 22);
    CHECK(next.check->result.con == Con::DecentChallenge);
    CHECK(Look(game) == full);
    CHECK(Feed(game, CheckReplyWith(kCon)).check.has_value()); // checking again renews the result
    CHECK(!Feed(game, CheckReplyWith(kDefeat)).check.has_value());
}

TEST(a_player_check_pairs_the_message_with_the_general_packet_that_follows_it)
{
    GameState game;
    CHECK(game.PlayerCheckOf(0x01006A2B) == nullptr);
    Packet message = Blank(0x0CA, 0x94);
    std::memcpy(message.bytes.data() + 0x80, "Tanyrus", 7);
    message.Put<uint32_t>(0x90, 206);
    Packet equipment = Blank(0x0C9, 0x54).Put<uint32_t>(0x04, 0x01006A2B).Put<uint8_t>(0x0A, 0x03);
    Packet general = Blank(0x0C9, 0x54).Put<uint32_t>(0x04, 0x01006A2B).Put<uint16_t>(0x08, 0x410)
        .Put<uint8_t>(0x0A, 0x01).Put<uint8_t>(0x22, 1).Put<uint8_t>(0x23, 13).Put<uint8_t>(0x24, 75)
        .Put<uint8_t>(0x25, 37);
    Feed(game, message);
    Feed(game, equipment); // Phoenix's order: the message, the equipment, then the general packet
    Feed(game, general);
    const PlayerCheck* check = game.PlayerCheckOf(0x01006A2B);
    CHECK(check != nullptr && check->message == (InspectMessage{"Tanyrus", 206}));
    CHECK(check->general.mainJob == 1 && check->general.subLevel == 37);
    Feed(game, general.Put<uint8_t>(0x24, 0).Put<uint8_t>(0x25, 0).Put<uint8_t>(0x22, 0).Put<uint8_t>(0x23, 0));
    CHECK(game.PlayerCheckOf(0x01006A2B)->general.mainJob == 0);  // a later check replaces it: now anonymous
    CHECK(game.PlayerCheckOf(0x01006A2B)->message.name.empty()); // and no message came before that one
}

TEST(a_short_check_message_leaves_no_message_waiting)
{
    GameState game;
    Packet message = Blank(0x0CA, 0x94);
    std::memcpy(message.bytes.data() + 0x80, "Tanyrus", 7);
    Feed(game, message);
    Feed(game, Cut(message, 0x93)); // replaces the one held
    Feed(game, Blank(0x0C9, 0x54).Put<uint32_t>(0x04, 0x01006A2B).Put<uint8_t>(0x0A, 0x01));
    CHECK(game.PlayerCheckOf(0x01006A2B)->message.name.empty());
}

TEST(the_conquest_standings_are_asked_for_once_after_loading_and_once_per_zone_in)
{
    GameState game;
    CHECK(!game.ConquestRequestDue(0.0, false, false)); // the bar not running, neither widget on, or zoning
    CHECK(game.ConquestRequestDue(0.0, true, false));   // loading counts as a zone-in
    game.ConquestRequested(0.0);
    CHECK(!game.ConquestRequestDue(30.0, true, false)); // once: an answer that never comes is not asked for again
    Feed(game, ZoneIn(kRain));
    CHECK(game.ConquestRequestDue(30.0, true, false) && !game.ConquestRequestDue(30.0, false, false));
}

TEST(no_request_goes_once_a_conquest_update_has_come_since_the_zone_in)
{
    GameState game;
    Feed(game, Conquest({{0, kSanDoria}})); // the server sent it unasked, as at the tally
    CHECK(!game.ConquestRequestDue(0.0, true, false));
    Feed(game, ZoneIn(kRain));
    CHECK(game.ConquestRequestDue(1.0, true, false));
    Feed(game, Cut(Conquest({{0, kSanDoria}}), 0x20)); // any 0x05E answers, even one too short to read
    CHECK(!game.ConquestRequestDue(1.0, true, false));
}

TEST(two_conquest_requests_are_never_within_five_seconds)
{
    GameState game;
    game.ConquestRequested(100.0);
    Feed(game, ZoneIn(kRain)); // a zone line crossed straight back
    CHECK(!game.ConquestRequestDue(104.9, true, false));
    CHECK(game.ConquestRequestDue(100.0 + kConquestAskGap, true, false));
}

TEST(the_standings_are_asked_for_again_each_minute_while_the_zone_popup_is_open)
{
    GameState game;
    game.ConquestRequested(0.0);
    CHECK(!game.ConquestRequestDue(59.0, true, true));
    CHECK(game.ConquestRequestDue(kPopupRefresh, true, true));
    CHECK(!game.ConquestRequestDue(kPopupRefresh, true, false)); // the popup closed
    CHECK(!game.ConquestRequestDue(kPopupRefresh, false, true)); // the bar not running, or zoning
    game.ConquestRequested(kPopupRefresh);
    CHECK(!game.ConquestRequestDue(kPopupRefresh + 59.0, true, true));
    GameState unasked;
    Feed(unasked, Conquest({{0, kSanDoria}})); // the server's own update, of an age HeadsUp does not know
    CHECK(unasked.ConquestRequestDue(0.0, true, true) && !unasked.ConquestRequestDue(0.0, true, false));
}

// The shares are of the region the character was in when the server answered, and the packet does not name it.
TEST(the_influence_is_the_last_updates_and_is_forgotten_at_a_zone_in)
{
    GameState game;
    CHECK(!game.Shares().has_value());
    Feed(game, Conquest({{0, kSanDoria}}).Put(0x86, uint8_t{61}).Put(0x87, uint8_t{21}).Put(0x88, uint8_t{11})
                   .Put(0x94, uint8_t{5}));
    CHECK(game.Shares() ==
          (RegionShares{{{Nation::SanDoria, 61}, {Nation::Bastok, 21}, {Nation::Windurst, 11}, {Nation::Beastmen, 5}}}));
    Feed(game, Cut(Conquest({{0, kSanDoria}}), 0x90)); // too short for the beastmen's share: the last stays
    CHECK(game.Shares().has_value() && (*game.Shares())[0].percent == 61);
    Feed(game, Conquest({{0, kSanDoria}}).Put(0x87, uint8_t{40}));
    CHECK(game.Shares().has_value() && (*game.Shares())[0] == (RegionShare{Nation::Bastok, 40}));
    Feed(game, ZoneIn(kRain));
    CHECK(!game.Shares().has_value() && game.Owners().has_value());
}

TEST(a_conquest_update_keeps_your_standing_and_its_bytes_and_a_short_one_keeps_the_last)
{
    GameState state;
    CHECK(!state.Standing().has_value() && state.LastConquestPacket().empty());
    Packet full = Blank(kConquestPacket, 0xB4);
    full.Put(0x8C, uint8_t{59}).Put(0x90, uint32_t{12345}).Put(0xB0, uint32_t{8210});
    state.Received(full.id, full.bytes.data(), static_cast<uint32_t>(full.bytes.size()));
    CHECK(state.Standing() == (ConquestStanding{59, 12345, 8210}));
    CHECK(state.LastConquestPacket() == full.bytes);
    const Packet owners = Blank(kConquestPacket, 0x90); // the owners alone
    state.Received(owners.id, owners.bytes.data(), static_cast<uint32_t>(owners.bytes.size()));
    CHECK(state.Standing() == (ConquestStanding{59, 12345, 8210}));
}

TEST(another_character_forgets_the_last_ones_conquest_points_and_standing_until_the_server_answers)
{
    constexpr uint32_t kMe = 0x00012345, kAlt = 0x00054321; // server ids, in 0x00A's UniqueNo at 0x04
    GameState state;
    Feed(state, ZoneIn(kRain).Put(0x04, kMe));
    Packet full = Blank(kConquestPacket, 0xB4);
    full.Put(0x8C, uint8_t{59}).Put(0x90, uint32_t{12345}).Put(0xB0, uint32_t{8210});
    Feed(state, full);
    // Zoning as the same character keeps them, as the server sends them only when asked.
    Feed(state, ZoneIn(kRain).Put(0x04, kMe));
    CHECK(state.Standing() == (ConquestStanding{59, 12345, 8210}) && !state.LastConquestPacket().empty());
    Feed(state, ZoneIn(kRain).Put(0x04, kAlt));
    CHECK(!state.Standing().has_value() && state.LastConquestPacket().empty());
    CHECK(state.Owners().has_value()); // who holds each region is the same for everyone
    full.Put(0x90, uint32_t{500}).Put(0xB0, uint32_t{0});
    Feed(state, full);
    CHECK(state.Standing() == (ConquestStanding{59, 500, 0}));
}

TEST(a_checked_player_shares_your_linkshell_when_their_check_names_one_you_wear)
{
    PlayerCheck check;
    check.general.linkshell = "Phoenix";
    const std::vector<std::string> yours{"Vanguard", "Phoenix"}, others{"Vanguard"};
    CHECK(SharesLinkshell(&check, yours));
    CHECK(!SharesLinkshell(&check, others));
    CHECK(!SharesLinkshell(&check, {}));     // you wear none
    CHECK(!SharesLinkshell(nullptr, yours)); // never checked, so not known
    // No name is no linkshell: theirs when they wear none, yours when the item's name does not unpack.
    const std::vector<std::string> unread{""};
    check.general.linkshell.clear();
    CHECK(!SharesLinkshell(&check, unread));
}
