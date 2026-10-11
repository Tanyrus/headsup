#include "test.h"
#include "zones.h"

#include <string>

using namespace headsup;

TEST(zones_are_found_by_id_with_names_players_know_and_their_regions)
{
    const ZoneRecord* dunes = ZoneById(103);
    CHECK(dunes != nullptr);
    CHECK(std::string(dunes->name) == "Valkurm Dunes");
    CHECK(RegionName(dunes->region) == "Zulkheim");
    CHECK(std::string(ZoneById(190)->name) == "King Ranperre's Tomb");
    CHECK(std::string(ZoneById(146)->name) == "Balga's Dais");
    CHECK(std::string(ZoneById(227)->name) == "Ship bound for Selbina"); // with pirates aboard, named as 220
    CHECK(std::string(ZoneById(228)->name) == "Ship bound for Mhaura");
    CHECK(std::string(ZoneById(299)->name) == "Gwora-Throne Room"); // the last
    CHECK(ZoneById(300) == nullptr && ZoneById(999) == nullptr);
    CHECK(RegionName(kUnknownRegion).empty());
}

TEST(a_zone_has_its_weather_calendar)
{
    CHECK(PackedWeather(*ZoneById(0), 0) == 0x0421); // sunshine, sunshine, sunshine
    CHECK(!PackedWeather(ZoneRecord{"Nowhere", kUnknownRegion, kNoCalendar}, 0).has_value());
}

TEST(zone_0_and_ids_past_the_data_are_unknown_and_read_as_the_bar_names_them)
{
    CHECK(KnownZone(0) == nullptr && KnownZone(999) == nullptr && KnownZone(103) == ZoneById(103));
    CHECK(ZoneTitle(103) == "Valkurm Dunes");
    CHECK(ZoneTitle(999) == "Zone 999"); // past the data: its number
    CHECK(ZoneTitle(0) == "-");          // no zone
    CHECK(ZoneRegion(KnownZone(103)) == "Zulkheim" && ZoneRegion(nullptr).empty());
}
