#include "test.h"
#include "linuxxi_report.h"

#include <cstring>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    LinuxXIReport Sample()
    {
        LinuxXIReport r;
        r.flags      = kPoolHooked | kPoolOn | kGraceActive | kGraceOn;
        r.copiesHeld = 588, r.copiesPeak = 1348, r.chunks = 3, r.chunksPeak = 4, r.chunkMb = 4;
        r.releasesHeldBack = 2055, r.releasesWaiting = 1;
        r.addonsStepped = 25, r.addonsNotLoaded = 3;
        r.addonCount = 2;
        std::strcpy(r.lua[0].name, "xiui");
        r.lua[0].kilobytes = 41250;
        std::strcpy(r.lua[1].name, "craftbook");
        r.lua[1].kilobytes = 9800;
        return r;
    }

    std::vector<uint8_t> Bytes(const LinuxXIReport& r)
    {
        std::vector<uint8_t> bytes(sizeof r);
        std::memcpy(bytes.data(), &r, sizeof r);
        return bytes;
    }
}

TEST(a_report_linuxxi_sends_is_read_whole)
{
    const std::vector<uint8_t> bytes = Bytes(Sample());
    const std::optional<LinuxXIReport> read = ReadLinuxXIReport(bytes.data(), static_cast<uint32_t>(bytes.size()));
    CHECK(read == Sample());
    CHECK(!OtherLinuxXIVersion(bytes.data(), static_cast<uint32_t>(bytes.size())).has_value());
}

TEST(a_report_of_another_size_or_version_is_not_read_and_its_version_is_named)
{
    std::vector<uint8_t> bytes = Bytes(Sample());
    CHECK(!ReadLinuxXIReport(bytes.data(), static_cast<uint32_t>(bytes.size() - 1)).has_value());
    CHECK(!ReadLinuxXIReport(nullptr, static_cast<uint32_t>(bytes.size())).has_value());
    LinuxXIReport newer = Sample();
    newer.version       = kLinuxXIReportVersion + 1;
    newer.size          = sizeof(LinuxXIReport) + sizeof(uint32_t);
    bytes               = Bytes(newer);
    bytes.resize(newer.size);
    CHECK(!ReadLinuxXIReport(bytes.data(), newer.size).has_value());
    CHECK(OtherLinuxXIVersion(bytes.data(), newer.size) == std::optional<uint32_t>(kLinuxXIReportVersion + 1));
    CHECK(!OtherLinuxXIVersion(bytes.data(), sizeof(uint32_t)).has_value());
}

TEST(a_count_of_addons_past_the_list_is_not_read)
{
    LinuxXIReport r = Sample();
    r.addonCount    = kLinuxXIAddons + 1;
    const std::vector<uint8_t> bytes = Bytes(r);
    CHECK(!ReadLinuxXIReport(bytes.data(), static_cast<uint32_t>(bytes.size())).has_value());
}

TEST(an_addon_name_without_its_end_is_cut_at_its_field)
{
    LinuxXIReport r = Sample();
    std::memset(r.lua[1].name, 'a', kLinuxXINameChars);
    const std::vector<uint8_t> bytes = Bytes(r);
    const std::optional<LinuxXIReport> read = ReadLinuxXIReport(bytes.data(), static_cast<uint32_t>(bytes.size()));
    CHECK(read.has_value() && std::string(read->lua[1].name) == std::string(kLinuxXINameChars - 1, 'a'));
}
