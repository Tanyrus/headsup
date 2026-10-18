#include "xilinux_report.h"

#include <cstring>

namespace headsup
{
    std::optional<XiLinuxReport> ReadXiLinuxReport(const void* data, uint32_t size)
    {
        if (data == nullptr || size != sizeof(XiLinuxReport)) return std::nullopt;
        XiLinuxReport report;
        std::memcpy(&report, data, sizeof report);
        if (report.size != size || report.version != kXiLinuxReportVersion || report.addonCount > kXiLinuxAddons) return std::nullopt;
        for (XiLinuxAddon& addon : report.lua)
            addon.name[kXiLinuxNameChars - 1] = '\0';
        return report;
    }

    std::optional<uint32_t> OtherXiLinuxVersion(const void* data, uint32_t size)
    {
        uint32_t head[2]; // size and version, the first two fields of every version
        if (data == nullptr || size < sizeof head) return std::nullopt;
        std::memcpy(head, data, sizeof head);
        if (head[1] == kXiLinuxReportVersion) return std::nullopt;
        return head[1];
    }
}
