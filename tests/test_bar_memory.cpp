#include "bar_memory.h"
#include "test.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    bool Near(double a, double b) { return std::fabs(a - b) < 1e-6; }

    MemoryReport Left(uint32_t leftMb)
    {
        constexpr uint32_t kCeilingMb = 4095;
        return MemoryReport{kCeilingMb - leftMb, leftMb, kCeilingMb};
    }

    std::string Joined(const std::vector<Span>& spans)
    {
        std::string text;
        for (const Span& s : spans)
            text += s.text;
        return text;
    }

    // The color each span is drawn in when it has one of its own; 0 for the theme's.
    std::vector<uint32_t> Colors(const std::vector<Span>& spans)
    {
        std::vector<uint32_t> colors;
        for (const Span& s : spans)
            colors.push_back(s.tone == Tone::Own ? s.argb : 0);
        return colors;
    }

    XiLinuxReport TwoAddons()
    {
        XiLinuxReport r;
        r.flags      = kPoolHooked | kPoolOn | kGraceActive | kGraceOn;
        r.copiesHeld = 588, r.copiesPeak = 1348, r.chunks = 3, r.chunksPeak = 4, r.chunkMb = 4;
        r.releasesHeldBack = 2055, r.releasesWaiting = 1, r.addonsStepped = 25, r.addonsNotLoaded = 3;
        r.addonCount = 2;
        std::strcpy(r.lua[0].name, "xiui");
        r.lua[0].kilobytes = 41250;
        std::strcpy(r.lua[1].name, "craftbook");
        r.lua[1].kilobytes = 900;
        return r;
    }

    // Every cell's text in the popup, in order.
    std::vector<std::string> Cells(const PopupContent& popup)
    {
        std::vector<std::string> cells;
        for (const PopupSection& section : popup.sections)
            for (const PopupRow& row : section.rows)
                for (const PopupCell& cell : row.cells)
                    cells.push_back(Joined(cell.text));
        return cells;
    }

    bool Has(const std::vector<std::string>& cells, const std::string& part)
    {
        for (const std::string& cell : cells)
            if (cell.find(part) != std::string::npos) return true;
        return false;
    }
}

TEST(the_processor_share_is_the_time_used_over_the_time_passed)
{
    CpuMeter cpu;
    CHECK(cpu.Wants(10.0));
    cpu.Add(10.0, 100.0);
    CHECK(!cpu.Percent().has_value());
    CHECK(!cpu.Wants(10.5));
    cpu.Add(10.5, 100.4); // too soon to count
    cpu.Add(11.0, 100.5);
    CHECK(cpu.Percent().has_value() && Near(*cpu.Percent(), 50.0));
    cpu.Add(13.0, 102.5);
    CHECK(Near(*cpu.Percent(), 100.0));
}

TEST(a_processor_time_that_goes_back_reports_nothing)
{
    CpuMeter cpu;
    cpu.Add(0.0, 50.0);
    cpu.Add(1.0, 50.2);
    cpu.Add(2.0, 10.0);
    CHECK(!cpu.Percent().has_value());
}

TEST(the_widget_shows_the_space_used_over_the_space_left)
{
    const WidgetContent none = MemoryWidget(std::nullopt);
    CHECK(none.shown && !none.hasPopup && none.subtitle.empty());
    const WidgetContent w = MemoryWidget(MemoryReport{1532, 2563, 4095});
    CHECK(w.shown && w.hasPopup && w.figures);
    CHECK(Joined(w.title).find("1,532") != std::string::npos);
    CHECK(Joined(w.subtitle).find("2.5") != std::string::npos);
    CHECK(Joined(MemoryWidget(Left(300)).subtitle).find("300") != std::string::npos);
}

TEST(the_widget_turns_amber_then_red_as_the_space_left_runs_low)
{
    CHECK(HeadroomOf(Left(kLowLeftMb)) == Headroom::Plenty);
    CHECK(HeadroomOf(Left(kLowLeftMb - 1)) == Headroom::Low);
    CHECK(HeadroomOf(Left(kCriticalLeftMb)) == Headroom::Low);
    CHECK(HeadroomOf(Left(kCriticalLeftMb - 1)) == Headroom::Critical);
    const WidgetContent plenty = MemoryWidget(Left(kLowLeftMb)), low = MemoryWidget(Left(kLowLeftMb - 1)),
                        critical = MemoryWidget(Left(kCriticalLeftMb - 1));
    CHECK(Colors(plenty.title) == std::vector<uint32_t>{0} && Colors(plenty.subtitle) == std::vector<uint32_t>{0});
    CHECK(Colors(low.title) == std::vector<uint32_t>{0} && Colors(low.subtitle) == std::vector<uint32_t>{kWarningColor});
    CHECK(Colors(critical.title) == std::vector<uint32_t>{kAlarmColor} && Colors(critical.subtitle) == std::vector<uint32_t>{kAlarmColor});
}

TEST(the_popup_has_the_space_the_processor_and_the_frames)
{
    const PopupContent popup = MemoryPopup(MemoryReport{1532, 2563, 4095}, 18.0, FrameReport{1000.0 / 30.0, 120.0}, XiLinuxState{});
    const std::vector<std::string> cells = Cells(popup);
    CHECK(Has(cells, "1,532") && Has(cells, "2,563") && Has(cells, "4,095"));
    CHECK(Has(cells, "18%") && Has(cells, "30 fps") && Has(cells, "33.3 ms") && Has(cells, "120.0 ms"));
    CHECK(!Has(cells, "xiui"));
    const std::vector<std::string> unread = Cells(MemoryPopup(MemoryReport{1532, 2563, 4095}, std::nullopt, std::nullopt, XiLinuxState{}));
    CHECK(!Has(unread, "%") && !Has(unread, "fps"));
}

TEST(the_popup_lists_what_xilinux_reports_with_each_addon_largest_first)
{
    const PopupContent popup = MemoryPopup(MemoryReport{1532, 2563, 4095}, 18.0, std::nullopt, XiLinuxState{TwoAddons(), std::nullopt});
    const std::vector<std::string> cells = Cells(popup);
    CHECK(Has(cells, "588") && Has(cells, "1,348") && Has(cells, "12 MB") && Has(cells, "2,055"));
    std::vector<std::string> addons;
    for (size_t i = 0; i + 1 < cells.size(); ++i)
        if (cells[i] == "xiui" || cells[i] == "craftbook") addons.push_back(cells[i] + " " + cells[i + 1]);
    CHECK(addons == (std::vector<std::string>{"xiui 40.3 MB", "craftbook 900 KB"}));
    CHECK(Has(cells, "41.2 MB")); // the two together
}

TEST(a_fix_xilinux_has_switched_off_or_not_started_says_so_in_amber)
{
    XiLinuxReport off = TwoAddons();
    off.flags         = kPoolHooked | kGraceActive; // both switched off
    std::vector<uint32_t> amber;
    for (const PopupSection& section : MemoryPopup(std::nullopt, std::nullopt, std::nullopt, XiLinuxState{off, std::nullopt}).sections)
        for (const PopupRow& row : section.rows)
            for (const PopupCell& cell : row.cells)
                for (const Span& span : cell.text)
                    if (span.tone == Tone::Own && span.argb == kWarningColor) amber.push_back(span.argb);
    CHECK(amber.size() == 2);
}

TEST(a_report_of_another_version_names_the_version)
{
    const std::vector<std::string> cells =
        Cells(MemoryPopup(MemoryReport{1532, 2563, 4095}, std::nullopt, std::nullopt, XiLinuxState{std::nullopt, 7}));
    CHECK(Has(cells, "7"));
}
