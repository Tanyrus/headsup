#pragma once

#include "check.h"
#include "currencies.h"
#include "game_packets.h"
#include "player_status.h"
#include "search_server.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace headsup
{
    // What a packet leaves for the plugin, which alone reaches the game's memory and its camera.
    struct PacketFollowUp
    {
        bool zonedIn = false;            // the scene camera is the last zone's
        std::optional<CheckReply> check; // its lifetime needs the mob's name from the game's memory
        std::optional<CheckAnswer> answer; // the mob a check's answer is about, gauged or not, for Auto-Check
        std::optional<uint32_t> mobDied;   // the server ID of a mob that died within 50 yalms
    };

    // A player's /check as the server answered it: the message, then the general part, kept until the next check of
    // them.
    struct PlayerCheck
    {
        InspectGeneral general;
        InspectMessage message; // empty when no message came before the general part
        uint64_t order = 0;     // which came last of this and a lookup (GameState::ReportedJobs)
        bool operator==(const PlayerCheck&) const = default;
    };

    struct PlayerSync
    {
        bool synced = false, held = false; // as CharSyncUpdate's
        uint8_t level = 0;   // CharSyncUpdate's or a level up's, or 0 since a check or lookup of them, which is newer
        bool sameJob  = false; // the level is from a level up or down, so of the job they had
    };

    // Whether the last check of a player names a linkshell you wear. No name is no linkshell.
    bool SharesLinkshell(const PlayerCheck* check, std::span<const std::string> yours);

    constexpr double kConquestAskGap   = 5.0;  // seconds between two conquest requests, at least
    constexpr double kPopupRefresh    = 60.0; // seconds between two requests for what an open popup shows

    // What the server's packets say, each parsed once as it arrives, whether or not anything shows it.
    class GameState
    {
    public:
        PacketFollowUp Received(uint16_t id, const uint8_t* data, uint32_t size);

        // Yours from 0x037 when index is self, anyone else's from 0x00D; null before one arrives.
        const PlayerStatus* PlayerStatusOf(uint32_t index, uint32_t self) const;
        // A player's last sync packet, yours included; all off when none came since they or you arrived. The plugin
        // also reads yours and your party's sync from the buffs the game keeps.
        PlayerSync SyncOf(uint16_t index) const;
        // Whether the server has made this mob or NPC untargetable, from its last update that said.
        bool Untargetable(uint16_t index) const;
        // Whether this mob's claimer called for help, from its last update that said.
        bool CalledForHelp(uint16_t index) const;
        const std::optional<Weather>& ZoneWeather() const { return m_Weather; }
        const std::optional<RegionOwners>& Owners() const { return m_Owners; }
        const std::optional<RegionShares>& Shares() const { return m_Shares; }
        // The last conquest packet as it came, which /hu debug writes to check its layout against.
        const std::vector<uint8_t>& LastConquestPacket() const { return m_ConquestBytes; }
        const PlayerCheck* PlayerCheckOf(uint32_t serverId) const;
        // The search server's answer about the player at index: newer than their last sync, as a check is.
        void LookedUp(uint32_t serverId, uint16_t index, const SearchedPlayer& answer);
        // A player's jobs from the newer of their last check and their last lookup; none from neither, or from one
        // that says nothing of them (anonymous).
        std::optional<PlayerJobs> ReportedJobs(uint32_t serverId) const;
        // The players whose look changed since the last call, by target index: not their first look.
        std::vector<uint16_t> TakeLookChanges();

        // Whether HeadsUp should ask for the conquest standings (kConquestRequest) now: once after loading and once per
        // zone-in, while no conquest update has come since, never within kConquestAskGap of the last ask, and again
        // every kPopupRefresh while refresh (the Zone popup, which shows them, is open). open: the bar runs with the
        // Zone widget on and the game is not zoning.
        bool ConquestRequestDue(double now, bool open, bool refresh) const;
        void ConquestRequested(double now);

        const CurrencyValues& Currencies() const { return m_Currencies; }
        // Whether HeadsUp should ask for the currencies (kCurrencies1Request and _2) now: as the conquest standings
        // are, once after loading and once per zone-in, and again every kPopupRefresh while refresh (their popup
        // is open). open: the bar runs with the Gil widget on and the game is not zoning.
        bool CurrencyRequestDue(double now, bool open, bool refresh) const;
        void CurrencyRequested(double now);

    private:
        std::unordered_map<uint16_t, PlayerStatus> m_PlayerStatus; // other players, by target index
        std::unordered_map<uint16_t, PlayerSync> m_PlayerSyncs;    // players, you included, by target index
        std::unordered_set<uint16_t> m_Untargetable;               // mobs and NPCs, by target index
        std::unordered_set<uint16_t> m_CalledForHelp;              // mobs, by target index
        std::optional<PlayerStatus> m_OwnStatus;
        std::optional<Weather> m_Weather;
        std::optional<RegionOwners> m_Owners; // kept across zoning
        std::optional<RegionShares> m_Shares;       // of the region zoned into: the next zone's may be another
        std::optional<uint32_t> m_Player;           // your server id, from the last zone-in
        std::vector<uint8_t> m_ConquestBytes;
        bool m_ConquestOwed = true;           // loading counts as a zone-in
        std::optional<double> m_ConquestAsked;
        CurrencyValues m_Currencies{}; // kept across zoning, not across characters
        bool m_CurrenciesOwed = true;
        std::optional<double> m_CurrenciesAsked;
        std::optional<InspectMessage> m_CheckMessage;              // waits for its general part
        std::unordered_map<uint32_t, PlayerCheck> m_PlayerChecks; // by server id, kept across zoning
        struct Lookup
        {
            PlayerJobs jobs;
            uint64_t order = 0;
        };
        std::unordered_map<uint32_t, Lookup> m_Lookups;           // by server id, kept across zoning
        uint64_t m_Order = 0;                                     // the last check's or lookup's
        std::unordered_map<uint16_t, PlayerLook> m_Looks;         // by target index
        std::vector<uint16_t> m_LookChanges;
    };
}
