#pragma once

#include "game_packets.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace headsup
{
    constexpr uint16_t kEventNumPacket = 0x034; // server to client: an event begins, with numbers for it

    // A conquest overseer's event: every overseer starts one of six, two a nation (conquest.lua's guardEvent), with
    // its eight numbers.
    struct GuardEvent
    {
        Nation nation = Nation::None; // the overseer's
        std::array<int32_t, 8> numbers{};
        bool operator==(const GuardEvent&) const = default;
    };
    // Phoenix's s2c 0x034: the eight numbers at 0x08, the event at 0x2C. Nothing for any other event.
    std::optional<GuardEvent> ParseGuardEvent(const uint8_t* data, uint32_t size);

    // Which outposts' supply runs a character has done, for each nation, as far as overseers have told: the server
    // keeps them, and tells them only in an overseer's event. In a nation's city an overseer's fifth number is all of
    // its nation's runs, a bit for each region from bit 5; at an outpost, a delivery's numbers say that one was run.
    class OutpostRuns
    {
    public:
        // An overseer's event in a zone of this region. True when it told something new.
        bool Heard(const GuardEvent& event, uint8_t region);
        // Whether the nation's supplies were run to this region's outpost; nothing while no overseer has said.
        std::optional<bool> Ran(Nation nation, uint8_t region) const;
        bool operator==(const OutpostRuns&) const = default;

    private:
        friend std::string OutpostLines(uint32_t character, const OutpostRuns& runs);
        friend bool ReadOutpostLine(std::string_view line, std::map<uint32_t, OutpostRuns>& into);
        struct Known
        {
            uint32_t told = 0, ran = 0; // by bit: what an overseer said, and what of it was run
            bool operator==(const Known&) const = default;
        };
        std::array<Known, kContenderNations - 1> m_Nations{}; // the three nations, from San d'Oria
    };

    // Every character's, by character id, as config/headsup/outposts.txt keeps them: a line for each nation known of
    // each character, its id, the nation's number, and what was told and what was run, in hex.
    using OutpostFile = std::map<uint32_t, OutpostRuns>;
    std::string OutpostFileText(const OutpostFile& file);
    OutpostFile OutpostFileFrom(std::string_view text);
}
