#include "bar_fps.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace headsup
{
    void FrameStats::Add(double now, double ms)
    {
        const bool newSecond = m_Frames.empty() || std::floor(now) != std::floor(m_Frames.back().end);
        m_Frames.push_back(Frame{now, ms});
        while (!m_Frames.empty() && m_Frames.front().end <= now - kSlowestWindow)
            m_Frames.pop_front();
        if (!newSecond) return;
        double sum = 0.0, slowest = 0.0;
        int recent = 0;
        for (const Frame& f : m_Frames)
        {
            slowest = std::max(slowest, f.ms);
            if (f.end <= now - kAverageWindow) continue;
            sum += f.ms;
            ++recent;
        }
        m_Report = FrameReport{sum / recent, slowest};
    }

    int FramesPerSecond(double averageMs)
    {
        return averageMs > 0.0 ? static_cast<int>(std::lround(static_cast<double>(kMsPerSecond) / averageMs)) : 0;
    }

    WidgetContent FpsWidget(const std::optional<FrameReport>& frames)
    {
        WidgetContent w;
        w.shown    = true;
        w.title    = ListOf(Text(frames ? std::to_string(FramesPerSecond(frames->averageMs)) + " fps" : "-"));
        w.hasPopup = frames.has_value();
        w.figures  = true;
        return w;
    }

    PopupContent FpsPopup(const std::optional<FrameReport>& frames)
    {
        PopupContent popup;
        popup.title = ListOf(Text("Frame rate"));
        if (!frames) return popup;
        popup.sections = ListOf(PopupSection{"", ListOf(Labeled("Frame time", Text(Printed("%.1f ms", frames->averageMs))),
            Labeled("Slowest frame", Text(Printed("%.1f ms", frames->slowestMs))))});
        return popup;
    }
}
