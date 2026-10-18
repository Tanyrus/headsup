#pragma once

#include "address_space.h"
#include "bar_content.h"
#include "bar_fps.h"
#include "linuxxi_report.h"

#include <optional>

namespace headsup
{
    constexpr double kCpuSampleSeconds = 1.0;
    constexpr uint32_t kLowLeftMb = 512, kCriticalLeftMb = 256; // the space left that turns the widget amber, then red

    // The process's processor time as a share of one core, between samples a second or more apart.
    class CpuMeter
    {
    public:
        // Whether a sample at now would count, so the time is read only then.
        bool Wants(double now) const { return !m_At || now - *m_At >= kCpuSampleSeconds; }
        // cpuSeconds: the process's kernel and user time so far.
        void Add(double now, double cpuSeconds);
        std::optional<double> Percent() const { return m_Percent; }

    private:
        std::optional<double> m_At, m_Cpu, m_Percent;
    };

    enum class Headroom : uint8_t
    {
        Plenty,
        Low,
        Critical,
    };
    Headroom HeadroomOf(const MemoryReport& memory);

    // What LinuxXI last reported, while it still reports, or the version it sends when this HeadsUp cannot read it.
    struct LinuxXIState
    {
        std::optional<LinuxXIReport> report;
        std::optional<uint32_t> otherVersion;
        bool operator==(const LinuxXIState&) const = default;
    };

    WidgetContent MemoryWidget(const std::optional<MemoryReport>& memory);
    PopupContent MemoryPopup(const std::optional<MemoryReport>& memory, std::optional<double> cpuPercent,
        const std::optional<FrameReport>& frames, const LinuxXIState& linuxxi);
}
