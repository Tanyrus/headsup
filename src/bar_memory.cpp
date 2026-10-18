#include "bar_memory.h"

#include <string>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kKbPerMb = 1024, kMbPerGb = 1024;
        constexpr int kLuaColumns  = 4; // two addons a row, each a name and its size

        std::string MbText(uint32_t mb) { return Grouped(mb) + " MB"; }
        std::string LeftText(uint32_t mb)
        {
            return mb >= kMbPerGb ? Printed("%.1f GB", static_cast<double>(mb) / kMbPerGb) : MbText(mb);
        }
        std::string KbText(uint64_t kb)
        {
            return kb >= kKbPerMb ? Printed("%.1f MB", static_cast<double>(kb) / kKbPerMb) : Grouped(static_cast<uint32_t>(kb)) + " KB";
        }

        // The space left, in the color of how little it is.
        Span LeftSpan(std::string text, Headroom headroom, Tone plenty)
        {
            switch (headroom)
            {
            case Headroom::Plenty: break;
            case Headroom::Low: return Colored(std::move(text), kWarningColor);
            case Headroom::Critical: return Colored(std::move(text), kAlarmColor);
            }
            return Text(std::move(text), plenty);
        }

        // One fix's figures; what it is when it never started or is switched off.
        Span FixSpan(uint32_t flags, uint32_t started, uint32_t on, std::string figures)
        {
            if ((flags & started) == 0) return Text("not in use", Tone::Dim);
            if ((flags & on) == 0) return Colored("switched off", kWarningColor);
            return Text(std::move(figures));
        }

        void AddLinuxXI(std::vector<PopupSection>& sections, const LinuxXIReport& r)
        {
            PopupSection fixes{"LinuxXI", {}};
            fixes.rows.push_back(Labeled("Packet copies",
                FixSpan(r.flags, kPoolHooked, kPoolOn, Grouped(r.copiesHeld) + " (" + Grouped(r.copiesPeak) + " at most)")));
            if ((r.flags & kPoolHooked) != 0)
                fixes.rows.push_back(Labeled("Pool", Text(MbText(r.chunks * r.chunkMb) + " (" + MbText(r.chunksPeak * r.chunkMb) + " at most)")));
            fixes.rows.push_back(Labeled("Releases held back",
                FixSpan(r.flags, kGraceActive, kGraceOn, Grouped(r.releasesHeldBack) + " (" + Grouped(r.releasesWaiting) + " waiting)")));
            fixes.rows.push_back(Labeled("Addons stepped", Text(Grouped(r.addonsStepped) + " (" + Grouped(r.addonsNotLoaded) + " not loaded)")));
            fixes.rows.push_back(Labeled("Registry compares", FixSpan(r.flags, kLocaleActive, kLocaleOn, "C locale")));
            sections.push_back(std::move(fixes));
            if (r.addonCount == 0) return;
            PopupSection lua{"Lua memory", {}};
            uint64_t total = 0;
            for (uint32_t i = 0; i < r.addonCount; ++i)
                total += r.lua[i].kilobytes;
            lua.rows.push_back(Labeled("All addons", Text(KbText(total))));
            PopupRow grid = Grid(kLuaColumns, 2 * r.addonCount);
            for (uint32_t i = 0; i < r.addonCount; ++i)
            {
                grid.cells.push_back(Cell(Text(r.lua[i].name)));
                grid.cells.push_back(Cell(Text(KbText(r.lua[i].kilobytes), Tone::Dim)));
            }
            lua.rows.push_back(std::move(grid));
            sections.push_back(std::move(lua));
        }
    }

    void CpuMeter::Add(double now, double cpuSeconds)
    {
        if (!Wants(now)) return;
        if (m_At && cpuSeconds >= *m_Cpu)
            m_Percent = 100.0 * (cpuSeconds - *m_Cpu) / (now - *m_At);
        else if (m_At)
            m_Percent.reset();
        m_At  = now;
        m_Cpu = cpuSeconds;
    }

    Headroom HeadroomOf(const MemoryReport& memory)
    {
        if (memory.leftMb < kCriticalLeftMb) return Headroom::Critical;
        if (memory.leftMb < kLowLeftMb) return Headroom::Low;
        return Headroom::Plenty;
    }

    WidgetContent MemoryWidget(const std::optional<MemoryReport>& memory)
    {
        WidgetContent w;
        w.shown   = true;
        w.figures = true;
        if (!memory)
        {
            w.title = ListOf(Text("-"));
            return w;
        }
        const Headroom headroom = HeadroomOf(*memory);
        const std::string used  = MbText(memory->usedMb);
        w.title    = ListOf(headroom == Headroom::Critical ? Colored(used, kAlarmColor) : Text(used));
        w.subtitle = ListOf(LeftSpan(LeftText(memory->leftMb) + " left", headroom, Tone::Dim));
        w.hasPopup = true;
        return w;
    }

    PopupContent MemoryPopup(const std::optional<MemoryReport>& memory, std::optional<double> cpuPercent,
        const std::optional<FrameReport>& frames, const LinuxXIState& linuxxi)
    {
        PopupContent popup;
        popup.title = ListOf(Text("Memory"));
        if (memory)
        {
            const Headroom headroom = HeadroomOf(*memory);
            PopupSection space;
            space.rows.push_back(Labeled("Used", Text(MbText(memory->usedMb))));
            space.rows.push_back(Labeled("Left", LeftSpan(MbText(memory->leftMb), headroom, Tone::Normal)));
            space.rows.push_back(Labeled("Of", Text(MbText(memory->ceilingMb))));
            if (headroom != Headroom::Plenty) space.rows.push_back(LineOf("The game closes once none is left.", Tone::Dim));
            popup.sections.push_back(std::move(space));
        }
        PopupSection processor{"Processor", {}};
        if (cpuPercent) processor.rows.push_back(Labeled("CPU", Text(Printed("%.0f%%", *cpuPercent) + " of one core")));
        if (frames)
        {
            processor.rows.push_back(Labeled("Frame rate", Text(std::to_string(FramesPerSecond(frames->averageMs)) + " fps")));
            processor.rows.push_back(Labeled("Frame time", Text(Printed("%.1f ms", frames->averageMs))));
            processor.rows.push_back(Labeled("Slowest frame", Text(Printed("%.1f ms", frames->slowestMs))));
        }
        if (!processor.rows.empty()) popup.sections.push_back(std::move(processor));
        if (linuxxi.report)
            AddLinuxXI(popup.sections, *linuxxi.report);
        else if (linuxxi.otherVersion)
            popup.sections.push_back(PopupSection{"LinuxXI", ListOf(LineOf("It sends report version " + std::to_string(*linuxxi.otherVersion) +
                                                                     "; this HeadsUp reads " + std::to_string(kLinuxXIReportVersion) + ".", Tone::Dim))});
        return popup;
    }
}
