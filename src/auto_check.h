#pragma once

#include "check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace headsup
{
    constexpr uint16_t kCheckRequestPacket = 0x0DD; // client to server: /check (Phoenix's GP_CLI_COMMAND_EQUIP_INSPECT)
    constexpr size_t kCheckRequestSize     = 16;
    // A /check of an entity as the client sends it, header and all as Ashita's AddOutgoingPacket takes it: its
    // server id at 0x04, its index at 0x08 and the kind, 0 for a plain check, at 0x0C.
    std::array<uint8_t, kCheckRequestSize> CheckRequest(uint32_t serverId, uint16_t index);
    // The server id an outgoing check asks about; nothing for a short packet.
    std::optional<uint32_t> CheckRequestTarget(const uint8_t* data, uint32_t size);

    // Past 50 yalms Phoenix logs a mob's check as one from too far away: HeadsUp asks from well inside that.
    constexpr float kAutoCheckRange = 45.0f;
    constexpr double kAutoCheckGap  = 0.5;  // seconds between two of HeadsUp's own checks
    constexpr double kOwnAnswerWait = 5.0;  // seconds an answer may take and still be the one HeadsUp asked for
    constexpr double kAskAgain      = 60.0; // seconds before a mob that never answered is asked about again
    constexpr double kReportWait    = 0.5;  // seconds after an answer in which the checker addon's line of it comes

    // Whether HeadsUp may check anything now: with the setting on, in a zone, and out of any event, during which
    // the server takes no check.
    constexpr bool MayAutoCheck(bool on, bool zoning, bool inEvent, int level) { return on && !zoning && !inEvent && level > 0; }

    struct CheckCandidate
    {
        uint32_t serverId = 0;
        uint16_t index    = 0;
        float distance    = 0.0f; // yalms from you
        bool alive        = false;
        bool known        = false; // a check's answer for it is held
        bool gaugeable    = true;  // false for one the server never gauges, as the data says of notorious monsters
        const char* name  = "";    // only read by Due
    };

    // Checks mobs by itself so their exact level shows without the player asking: one at a time, the nearest
    // first, each once for as long as its answer holds. Players are never checked: the server tells a player who
    // examined them.
    class AutoCheck
    {
    public:
        // The mob to check now, if one is due, which is then asked about.
        std::optional<CheckCandidate> Due(std::span<const CheckCandidate> mobs, double now);
        // The player checked something themselves: its answer is theirs to read.
        void PlayerAsked(uint32_t serverId, double now);
        // An answer came. True when HeadsUp asked for it and the player did not, so that nobody waits to read it
        // in the chat log.
        bool TakeAnswer(const CheckAnswer& answer, double now);
        // Whether a chat line is the checker addon's report of a check HeadsUp made by itself. The addon (atom0s's,
        // loaded by Ashita's default script) writes one for every answer whoever asked, and reads the packet as
        // it came, so keeping the answer from the game does not keep this from the log.
        bool OwnReport(std::string_view line, double now) const;
        // The mob was seen dead: what comes back is another spawn.
        void Forget(uint32_t serverId);
        // Another zone, where the ids are other mobs'.
        void Clear();

    private:
        std::unordered_map<uint32_t, double> m_Asked, m_PlayerAsked; // by server id: when
        std::unordered_map<uint32_t, double> m_OwnAnswered;          // when an answer HeadsUp alone asked for came
        std::unordered_map<uint32_t, std::string> m_Names;           // of the mobs asked about
        std::unordered_set<uint32_t> m_Waiting;                      // asked by HeadsUp, not answered yet
        std::unordered_set<uint32_t> m_NeverGauged;                  // answered "impossible to gauge"
        std::optional<double> m_LastAsk;
    };
}
