#include "outposts.h"
#include "test.h"

#include <array>
#include <cstring>
#include <vector>

using namespace headsup;

namespace
{
    constexpr uint8_t kGustaberg = 3, kDerfland = 4, kBastokCity = 20, kJeunoCity = 22;
    // Phoenix's 0x034 as Ashita hands it over, its 4-byte header first: the NPC, eight numbers, its index, the zone's
    // event table and the event.
    std::vector<uint8_t> EventPacket(uint16_t event, std::array<int32_t, 8> numbers)
    {
        std::vector<uint8_t> packet(0x38, 0);
        packet[0] = 0x34;
        for (size_t i = 0; i < numbers.size(); ++i)
            std::memcpy(packet.data() + 0x08 + 4 * i, &numbers[i], sizeof(int32_t));
        std::memcpy(packet.data() + 0x2C, &event, sizeof(event));
        return packet;
    }
    constexpr std::array<int32_t, 8> kDelivered{16, 0, 0, 0, 1, 0, 0, 255}; // 'you have brought us supplies!'
}

TEST(an_overseers_event_is_read_and_its_nation_comes_from_its_number)
{
    const std::vector<uint8_t> packet = EventPacket(32761, {1, 2, 3, 4, 0x220, 6, 7, 8});
    const std::optional<GuardEvent> e = ParseGuardEvent(packet.data(), static_cast<uint32_t>(packet.size()));
    CHECK(e.has_value() && e->nation == Nation::Bastok && e->numbers[4] == 0x220);
    CHECK(ParseGuardEvent(packet.data(), 0x20) == std::nullopt); // short
    const std::vector<uint8_t> other = EventPacket(500, {});
    CHECK(!ParseGuardEvent(other.data(), static_cast<uint32_t>(other.size())).has_value()); // any other event
    for (const auto& [event, nation] : {std::pair{32758, Nation::Windurst}, std::pair{32759, Nation::Windurst},
             std::pair{32760, Nation::Bastok}, std::pair{32762, Nation::SanDoria}, std::pair{32763, Nation::SanDoria}})
    {
        const std::vector<uint8_t> p = EventPacket(static_cast<uint16_t>(event), {});
        CHECK(ParseGuardEvent(p.data(), static_cast<uint32_t>(p.size()))->nation == nation);
    }
}

TEST(supply_runs_are_learned_from_an_overseer_in_town_and_from_each_delivery)
{
    OutpostRuns runs;
    CHECK(!runs.Ran(Nation::Bastok, kGustaberg).has_value()); // unknown until an overseer says
    // At an outpost, before any overseer in town: a delivery says that one, and nothing of the rest.
    CHECK(runs.Heard(GuardEvent{Nation::Bastok, kDelivered}, kDerfland));
    CHECK(runs.Ran(Nation::Bastok, kDerfland) == true && !runs.Ran(Nation::Bastok, kGustaberg).has_value());
    // In Bastok, the overseer's fifth number is every outpost run, a bit for each region from bit 5.
    CHECK(runs.Heard(GuardEvent{Nation::Bastok, {1, 0, 0, 0, 1 << (kGustaberg + 5), 0, 0, 0}}, kBastokCity));
    CHECK(runs.Ran(Nation::Bastok, kGustaberg) == true && runs.Ran(Nation::Bastok, kDerfland) == false);
    CHECK(!runs.Ran(Nation::SanDoria, kGustaberg).has_value()); // another nation's are its own
    // Jeuno's overseers and the outposts' own say nothing of it, whatever their numbers.
    CHECK(!runs.Heard(GuardEvent{Nation::Bastok, {1, 0, 0, 0, 0, 0, 0, 0}}, kJeunoCity));
    CHECK(!runs.Heard(GuardEvent{Nation::Bastok, {1, 0, 0x3F0000, 0, 0, 0, 0, 0}}, kGustaberg));
    CHECK(runs.Ran(Nation::Bastok, kGustaberg) == true);
    // The same news twice changes nothing, so nothing is saved again.
    CHECK(!runs.Heard(GuardEvent{Nation::Bastok, kDelivered}, kGustaberg));
}

TEST(each_characters_supply_runs_are_saved_and_read_back)
{
    OutpostFile file;
    file[65001].Heard(GuardEvent{Nation::Bastok, {1, 0, 0, 0, 0x220, 0, 0, 0}}, kBastokCity);
    file[65002].Heard(GuardEvent{Nation::Windurst, kDelivered}, kGustaberg);
    const OutpostFile read = OutpostFileFrom(OutpostFileText(file));
    CHECK(read.size() == 2 && read.at(65001) == file.at(65001) && read.at(65002) == file.at(65002));
    CHECK(OutpostFileFrom("not a line\n12 x y z\n").empty()); // what cannot be read is left out
}
