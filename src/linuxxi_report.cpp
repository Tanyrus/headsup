#include "linuxxi_report.h"

#include <cstring>

namespace headsup
{
    std::optional<LinuxXIReport> ReadLinuxXIReport(const void* data, uint32_t size)
    {
        if (data == nullptr || size != sizeof(LinuxXIReport)) return std::nullopt;
        LinuxXIReport report;
        std::memcpy(&report, data, sizeof report);
        if (report.size != size || report.version != kLinuxXIReportVersion || report.addonCount > kLinuxXIAddons) return std::nullopt;
        for (LinuxXIAddon& addon : report.lua)
            addon.name[kLinuxXINameChars - 1] = '\0';
        return report;
    }

    std::optional<uint32_t> OtherLinuxXIVersion(const void* data, uint32_t size)
    {
        uint32_t head[2]; // size and version, the first two fields of every version
        if (data == nullptr || size < sizeof head) return std::nullopt;
        std::memcpy(head, data, sizeof head);
        if (head[1] == kLinuxXIReportVersion) return std::nullopt;
        return head[1];
    }
}
