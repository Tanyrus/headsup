#pragma once

#include "bar_content.h"

#include <deque>
#include <optional>

namespace headsup
{
    constexpr double kAverageWindow = 1.0, kSlowestWindow = 5.0; // seconds

    struct FrameReport
    {
        double averageMs = 0.0; // over the frames that ended in the last kAverageWindow
        double slowestMs = 0.0; // over the last kSlowestWindow
        bool operator==(const FrameReport&) const = default;
    };

    // The time between Presents, as the plugin measures each frame.
    class FrameStats
    {
    public:
        // A frame that ended at now (seconds) after ms.
        void Add(double now, double ms);
        // As of the first frame of the latest whole second, so the figures shown change once a second at most; nothing
        // before the first frame.
        std::optional<FrameReport> Report() const { return m_Report; }

    private:
        struct Frame
        {
            double end = 0.0, ms = 0.0;
        };
        std::deque<Frame> m_Frames; // the last kSlowestWindow, oldest first
        std::optional<FrameReport> m_Report;
    };

    // Frames a second at that average frame time, rounded; 0 for none.
    int FramesPerSecond(double averageMs);

    WidgetContent FpsWidget(const std::optional<FrameReport>& frames);
    PopupContent FpsPopup(const std::optional<FrameReport>& frames);
}
