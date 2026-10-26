#pragma once

#include "bar_connection.h"
#include "bar_content.h"
#include "bar_fps.h"
#include "bar_job.h"
#include "bar_memory.h"
#include "bar_party.h"
#include "bar_signet.h"
#include "bar_types.h"
#include "game_packets.h"
#include "game_state.h"
#include "icons.h"
#include "settings.h"
#include "strip_text.h"
#include "tracker.h"
#include "weather.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace headsup
{
    struct BagCount
    {
        uint32_t used = 0, max = 0;
    };
    constexpr uint32_t kInventory = 0, kGilSlot = 0; // gil is an item in the inventory's first slot
    constexpr int kMogSafe2 = 9;
    // That slot's item read as gil: its count when the item is gil, nullopt for anything else there.
    std::optional<uint32_t> GilOf(uint16_t item, uint32_t count);
    // A container as Ashita counts it: nullopt for one the character has no room in (locked, or not bought).
    std::optional<BagCount> BagOf(uint32_t used, uint32_t max);
    // The container whose name, as the Bags popup gives it, the caption is: the game's caption for a bag list, or
    // "Items" for the inventory as that menu opens.
    std::optional<int> ContainerNamed(const std::string& caption);
    // And the other way: a container's name, "Mog Satchel".
    const char* ContainerName(int container);

    // What the widgets show, gathered by the plugin each frame.
    struct BarState
    {
        int64_t unixMs = 0; // Earth time, UTC, for the Vana'diel clock
        int earthYear = 0, earthMonth = 0, earthDay = 0, earthHour = 0, earthMinute = 0, earthSecond = 0; // local
        uint16_t zone = 0; // 0 while unknown
        const ActorInfo* target = nullptr;
        std::optional<float> targetDistance;   // yalms, with the target
        // The game's name for an item ID, for a mob's drops; nullptr, or an empty name, when it has none.
        const char* (*itemName)(uint16_t item) = nullptr;
        // And for a spell ID, for what a mob casts.
        const char* (*spellName)(uint16_t spell) = nullptr;
        std::optional<double> targetPhSeconds; // the target placeholder's countdown, when one runs
        std::optional<PlayerCheck> targetCheck; // the targeted player's last check, gathered with the target
        std::optional<Caster> caster;           // your magic accuracy, gathered with the target for its chances
        std::optional<uint32_t> gil;           // nullopt while the inventory reports nothing
        CurrencyValues currencies{};           // the server's, as last sent; gil is the inventory's, above
        std::array<std::optional<BagCount>, kContainerCount> bags{}; // nullopt for a container without room
        std::optional<StripText> strip;    // nullopt while the strip cannot be read
        std::optional<BagCount> stripBag;  // the bag the caption names
        std::optional<Weather> weather; // the latest from the server; nullopt before one since loading or zoning
        std::optional<RegionOwners> owners; // nullopt before a conquest update
        std::optional<RegionShares> shares; // nullopt before a conquest update since the last zone-in
        std::optional<FrameReport> frames; // nullopt before the first frame
        std::optional<JobReport> job; // nullopt while the game reports no job
        std::optional<ConnectionReport> connection;
        Party party{}; // empty slots for no one
        std::optional<SignetState> signet; // nullopt while neither is on
        std::optional<MemoryReport> memory; // nullopt before the first whole walk of the space
        std::optional<double> cpuPercent;   // of one core; nullopt before two samples
        LinuxXIState linuxxi;
    };

    // The parts of BarState that the shown widgets and the open popup read, for the plugin to gather only those; it
    // always copies the weather, the owners and the shares, which GameState keeps from packets.
    struct BarNeeds
    {
        bool unixTime   = false; // unixMs
        bool localTime  = false; // the earth date and time
        bool zone       = false;
        bool target     = false; // with its placeholder's countdown and a player's check
        bool gil        = false;
        bool strip      = false; // the game's top-strip text
        bool frames     = false;
        bool job        = false;
        bool connection = false;
        bool party      = false;
        bool signet     = false; // read with the clock, which its timer counts from
        bool memory     = false;
        bool cpu        = false;
        bool linuxxi    = false;
        int containers = 0; // bags from 0 up to this
    };
    BarNeeds NeedsOf(const Settings& settings, std::optional<PanelWidget> open);

    // Game help is never shown on the bar: it has a bar of its own, under the top bar.
    WidgetContent BarWidgetContent(PanelWidget widget, const BarState& state, const Settings& settings);
    // The used and total slots of the bag the game's caption names, as the Bags widget writes them.
    std::optional<Span> StripBagFill(const BarState& state);
    PopupContent BarPopupContent(PanelWidget widget, const BarState& state, const Settings& settings);
    // The element colors of the weekdays and weather, from the spec.
    uint32_t ElementColor(Element element);
}
