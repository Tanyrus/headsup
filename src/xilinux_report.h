#pragma once

#include <cstdint>
#include <optional>

namespace headsup
{
    // What the xilinux plugin sends every plugin once a second through Ashita's plugin events, for the Memory widget.
    // Fixed-width fields alone, so its 32-bit build and the tests' 64-bit one agree; a field added goes before lua
    // with the version raised, and a reader of the old version then refuses it by its size.
    constexpr const char* kXiLinuxReportEvent = "xilinux_report";
    constexpr uint32_t kXiLinuxReportVersion  = 1;
    constexpr uint32_t kXiLinuxAddons         = 64;
    constexpr uint32_t kXiLinuxNameChars      = 32; // with its terminator

    enum XiLinuxFlags : uint32_t
    {
        kPoolHooked   = 1, // the addon host's copies come from the pool
        kPoolOn       = 2,
        kGraceActive  = 4, // Direct3D's releases are held back
        kGraceOn      = 8,
    };

    struct XiLinuxAddon
    {
        char name[kXiLinuxNameChars];
        uint32_t kilobytes;
        bool operator==(const XiLinuxAddon&) const = default;
    };

    struct XiLinuxReport
    {
        uint32_t size    = sizeof(XiLinuxReport);
        uint32_t version = kXiLinuxReportVersion;
        uint32_t flags   = 0;
        uint32_t copiesHeld = 0, copiesPeak = 0, chunks = 0, chunksPeak = 0, chunkMb = 0;
        uint32_t releasesHeldBack = 0, releasesWaiting = 0;
        uint32_t addonsStepped = 0, addonsNotLoaded = 0;
        uint32_t addonCount = 0; // of lua, largest first
        XiLinuxAddon lua[kXiLinuxAddons]{};
        bool operator==(const XiLinuxReport&) const = default;
    };
    static_assert(sizeof(XiLinuxAddon) == 36 && sizeof(XiLinuxReport) == 52 + 36 * kXiLinuxAddons);

    // The report in an event's bytes; nothing for bytes of another size or version, or a count past the list.
    std::optional<XiLinuxReport> ReadXiLinuxReport(const void* data, uint32_t size);
    // The version those bytes say they are, when it is not the one read here.
    std::optional<uint32_t> OtherXiLinuxVersion(const void* data, uint32_t size);
}
