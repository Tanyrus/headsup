#include "bar_layout.h"

#include "mouse_messages.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kEllipsis = "...";
        constexpr float kBottomLine  = 1.0f;  // the strip's last row is a line
        constexpr float kBorder      = 1.0f;  // a pill's or frame's
        constexpr float kInkMargin   = 0.06f; // the ink's clear space inside a pill, of its height
        constexpr float kMinInkMargin = 2.0f; // pixels
        constexpr float kLineGap     = 0.08f; // H from the title's descenders to the detail's top, when there is room
        constexpr float kMinLineGap  = 3.0f;  // pixels: two clear rows between them still read as touching
        constexpr int kMinDetail     = 9;     // pixels, the smallest detail line worth drawing
        constexpr float kShrinkStep  = 0.02f; // both lines get smaller together by this share of their size at a time
        constexpr int kShrinkSteps   = 25;    // to half at most

        float Width(const std::array<float, kBarWidgetCount>& natural, BarWidget w)
        {
            return natural[static_cast<int>(w)];
        }

        constexpr float kFitSlack = 0.01f; // pixels of float rounding: a widget that fits exactly is not dropped

        constexpr float kDiamondGap = 2.0f; // pixels clear between a frame or hover tint and a divider's diamond

        // Between neighbours in a section: `between`, and in ornate, where a diamond on the plain neighbour's edge
        // divides them, the framed target's clear space from it.
        struct Spacing
        {
            float between = 0.0f, frameClear = 0.0f;
            float Lead(BarWidget w) const { return w == BarWidget::Target ? frameClear : 0.0f; }
            float Apart(BarWidget a, BarWidget b) const { return between + Lead(a) + Lead(b); }
        };

        struct Group
        {
            std::vector<PlacedWidget> widgets; // in placing order
            std::vector<float> dividers;
            float targetWidth = 0.0f;
        };

        // Places order one after another from `from` towards `limit` (leftward when limit is less): each widget at its
        // width, spaced from the last; the first that does not fit goes with every one after it.
        Group PlaceGroup(const std::vector<BarWidget>& order, float from, float limit, const Spacing& spacing,
                         bool diamonds, const std::array<float, kBarWidgetCount>& widths, const BarRow& pill,
                         float rowY, float rowH)
        {
            Group g;
            const float dir = limit >= from ? 1.0f : -1.0f;
            float at        = from;
            for (const BarWidget w : order)
            {
                const float width = Width(widths, w);
                if (width <= 0.0f) continue;
                const bool first = g.widgets.empty();
                const float gap  = first ? 0.0f : spacing.Apart(g.widgets.back().widget, w);
                if (width > (limit - at) * dir - gap + kFitSlack) break;
                if (!first && diamonds)
                    g.dividers.push_back(at + dir * (spacing.Lead(g.widgets.back().widget) + spacing.between / 2.0f));
                const float x       = dir > 0.0f ? at + gap : at - gap - width;
                const bool isTarget = w == BarWidget::Target; // framed on the pill row in every shape
                g.widgets.push_back({w, BarRect{x, isTarget ? pill.y : rowY, width, isTarget ? pill.height : rowH}});
                if (isTarget) g.targetWidth = width;
                at = dir > 0.0f ? x + width : x;
            }
            return g;
        }

        float GroupWidth(const std::vector<BarWidget>& order, const std::array<float, kBarWidgetCount>& widths,
                         const Spacing& spacing)
        {
            float total = 0.0f;
            std::optional<BarWidget> last;
            for (const BarWidget w : order)
                if (Width(widths, w) > 0.0f)
                {
                    total += Width(widths, w) + (last ? spacing.Apart(*last, w) : 0.0f);
                    last = w;
                }
            return total;
        }

        // The ink's clear space above and below inside a pill row, in whole pixels, so the drawn text, rounded to
        // them, keeps all of it.
        float InkMargin(float rowHeight) { return std::ceil(std::max(kMinInkMargin, kInkMargin * rowHeight)); }

        float InkRoom(float rowHeight) { return rowHeight - 2.0f * kBorder - 2.0f * InkMargin(rowHeight); }
    }

    BarLayout LayoutBar(float screenWidth, const BarMetrics& m, BarShape shape,
                        const std::array<float, kBarWidgetCount>& natural, float targetMinimum,
                        const BarPlacement& placement, BarSeparators separators)
    {
        const bool ornate   = shape == BarShape::Ornate;
        const bool diamonds = DiamondsOn(separators, shape);
        // Ornate neighbours abut, a diamond on their boundary, the tints keeping clear of it. Pills sit a gap apart; a
        // diamond between them takes its width and a gap on either side.
        const float pillGap = diamonds ? 2.0f * m.gap + static_cast<float>(WholePixels(m.diamond)) : m.gap;
        const Spacing spacing{.between = ornate ? 0.0f : pillGap, .frameClear = ornate ? DiamondClearance(m) : 0.0f};
        const float edge    = ornate ? m.cap : m.gap; // between a section and the screen's edge
        const BarRow pill   = PillRow(m);
        const float rowY    = ornate ? 0.0f : pill.y;
        const float rowH    = ornate ? m.height : pill.height;
        const auto& left    = placement[static_cast<int>(BarSection::Left)];
        const auto& center  = placement[static_cast<int>(BarSection::Center)];
        const auto& right   = placement[static_cast<int>(BarSection::Right)];

        // The target gives way first, wherever it sits: it gives up what the bar is short of, in whole pixels, and goes
        // when that leaves it less than min(its width, its minimum). Only then do other widgets go. Sections with
        // something shown are a gap apart and the outer ones an edge from the screen's sides.
        const auto giveWay = [&](std::array<float, kBarWidgetCount> widths)
        {
            const auto shortOf = [&]
            {
                float demand = 2.0f * edge;
                int filled   = 0;
                for (const std::vector<BarWidget>* section : {&left, &center, &right})
                    if (const float w = GroupWidth(*section, widths, spacing); w > 0.0f)
                        demand += w + (filled++ > 0 ? m.gap : 0.0f);
                return demand - screenWidth;
            };
            constexpr int kTarget = static_cast<int>(BarWidget::Target);
            if (const float want = widths[kTarget], lack = shortOf(); want > 0.0f && lack > kFitSlack)
            {
                const float got = std::floor(want - lack);
                widths[kTarget] = got > 0.0f && got >= std::min(want, targetMinimum) ? got : 0.0f;
            }
            return widths;
        };

        const auto place = [&](const std::array<float, kBarWidgetCount>& widths)
        {
            BarLayout l;
            l.height = m.height;
            Group lg = PlaceGroup(left, edge, screenWidth - edge, spacing, diamonds, widths, pill, rowY, rowH);
            const float leftEnd =
                lg.widgets.empty() ? edge - m.gap : lg.widgets.back().rect.x + lg.widgets.back().rect.width;
            const std::vector<BarWidget> rightInward(right.rbegin(), right.rend());
            Group rg = PlaceGroup(rightInward, screenWidth - edge, leftEnd + m.gap, spacing, diamonds, widths, pill,
                rowY, rowH);
            std::reverse(rg.widgets.begin(), rg.widgets.end());
            const float rightStart = rg.widgets.empty() ? screenWidth - edge + m.gap : rg.widgets.front().rect.x;

            // The center: what fits between the sides, a gap from each, its widgets going from its end.
            const float lowest = leftEnd + m.gap, highest = rightStart - m.gap;
            std::vector<BarWidget> shown;
            for (const BarWidget w : center)
                if (Width(widths, w) > 0.0f) shown.push_back(w);
            while (!shown.empty() && GroupWidth(shown, widths, spacing) > highest - lowest + kFitSlack)
                shown.pop_back();
            Group cg;
            if (!shown.empty())
            {
                const float total = GroupWidth(shown, widths, spacing);
                // The two bounds are equal when the center got all the room, and float rounding can put the first an
                // ulp above the second, which std::clamp does not allow.
                const float x = std::max(lowest, std::min(screenWidth / 2.0f - total / 2.0f, highest - total));
                // Its widths fit already: no limit, so float rounding cannot drop its last widget.
                cg = PlaceGroup(shown, x, std::numeric_limits<float>::infinity(), spacing, diamonds, widths, pill,
                    rowY, rowH);
            }

            for (const Group* g : {&lg, &cg, &rg})
            {
                l.widgets.insert(l.widgets.end(), g->widgets.begin(), g->widgets.end());
                l.dividers.insert(l.dividers.end(), g->dividers.begin(), g->dividers.end());
                l.targetWidth = std::max(l.targetWidth, g->targetWidth);
            }
            std::sort(l.dividers.begin(), l.dividers.end());
            return l;
        };

        constexpr int kTarget = static_cast<int>(BarWidget::Target);
        const std::array<float, kBarWidgetCount> widths = giveWay(natural);
        BarLayout l = place(widths);
        // The target gave way against every widget; those that went anyway leave room it takes back.
        std::array<float, kBarWidgetCount> kept = natural;
        for (int w = 0; w < kBarWidgetCount; ++w)
            if (w != kTarget && RectOf(l, static_cast<BarWidget>(w)) == nullptr) kept[w] = 0.0f;
        if (const std::array<float, kBarWidgetCount> refit = giveWay(kept); refit[kTarget] != widths[kTarget])
            l = place(refit);
        return l;
    }

    bool DiamondsOn(BarSeparators separators, BarShape shape)
    {
        return separators == BarSeparators::Diamonds || (separators == BarSeparators::Theme && shape == BarShape::Ornate);
    }

    float DiamondClearance(const BarMetrics& m)
    {
        return static_cast<float>(WholePixels(m.diamond)) / 2.0f + kDiamondGap;
    }

    BarRow PillRow(const BarMetrics& m)
    {
        const float room   = std::max(0.0f, m.height - kBottomLine);
        const float height = std::max(0.0f, std::min(std::round(m.pillHeight), room));
        return BarRow{std::floor((room - height) / 2.0f), height};
    }

    TextRow FitTextRow(const BarMetrics& m, FunctionRef<LineInk(bool title, int pixels)> ink)
    {
        const BarRow row    = PillRow(m);
        const float inside  = row.height - 2.0f * kBorder;
        const float margin  = InkMargin(row.height);
        // The margin grows a pixel a side at a time, so a row a pixel taller can have a pixel less room for the ink: a
        // row takes no more than the next one's, and a taller bar never gets smaller text.
        const float room    = std::min(InkRoom(row.height), InkRoom(row.height + 1.0f));
        const float maxGap  = std::max(kMinLineGap, std::round(kLineGap * m.height));
        TextRow t;
        LineInk title, detail;
        float spare = 0.0f; // for the gap
        // A set size is the player's title: only its detail line gets smaller, a pixel at a time down to the floor.
        const int steps = m.titleSet ? WholePixels(m.subtitle) - kMinDetail : kShrinkSteps;
        for (int step = 0; step <= steps; ++step)
        {
            const float scale = 1.0f - kShrinkStep * static_cast<float>(step);
            t.title           = WholePixels(m.titleSet ? m.title : m.title * scale);
            const int share   = m.titleSet ? WholePixels(m.subtitle) - step : WholePixels(m.subtitle * scale);
            t.detail          = std::min(share, t.title - 1);
            if (t.detail < kMinDetail) break;
            title  = ink(true, t.title);
            detail = ink(false, t.detail);
            spare  = room - (title.bottom - title.top) - (detail.bottom - detail.top);
            if (spare >= kMinLineGap)
            {
                t.twoLines = true;
                break;
            }
        }
        if (t.twoLines)
        {
            const float gap   = std::min(spare, maxGap);
            const float block = (title.bottom - title.top) + gap + (detail.bottom - detail.top);
            const float top   = row.y + kBorder + (inside - block) / 2.0f; // of the title's ink
            t.titleY          = top - title.top;
            t.detailY         = top + (title.bottom - title.top) + gap - detail.top;
        }
        else
        {
            // Too short a row for two lines kept apart: titles alone, as large as their ink fits.
            t.detail = 0;
            for (t.title = WholePixels(m.title); t.title > 1; --t.title)
                if (const LineInk alone = ink(true, t.title); alone.bottom - alone.top <= room) break;
            title = ink(true, t.title);
        }
        // A title alone: its capitals centered, raised if its descenders would come too near the bottom.
        const float centered = row.y + (row.height - (title.baseline - title.top)) / 2.0f;
        const float lowest   = row.y + row.height - kBorder - margin - (title.bottom - title.top);
        t.aloneY             = std::min(centered, lowest) - title.top;
        return t;
    }

    std::string Shortest(const std::string& text, int characters)
    {
        size_t end = 0;
        for (int n = 0; n < characters && end < text.size(); ++n)
            do
                ++end;
            while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80);
        if (end >= text.size()) return text;
        std::string cut = text.substr(0, end);
        while (!cut.empty() && cut.back() == ' ')
            cut.pop_back();
        return cut.empty() ? std::string() : cut + kEllipsis;
    }

    std::optional<BarWidget> WidgetAt(const BarLayout& layout, float x, float y)
    {
        for (const PlacedWidget& p : layout.widgets)
            if (p.rect.Contains(x, y)) return p.widget;
        return std::nullopt;
    }

    const BarRect* RectOf(const BarLayout& layout, BarWidget widget)
    {
        for (const PlacedWidget& p : layout.widgets)
            if (p.widget == widget) return &p.rect;
        return nullptr;
    }

    BarRect PlacePopup(const BarRect& widget, float barHeight, float width, float height, float screenWidth,
                       float screenHeight, float gap, float ring)
    {
        const float clear    = gap + ring;
        const float centered = widget.x + widget.width / 2.0f - width / 2.0f;
        const float x        = std::max(0.0f, std::max(clear, std::min(centered, screenWidth - clear - width)));
        const float y        = barHeight + clear;
        return BarRect{x, y, width, std::max(0.0f, std::min(height, PopupRoom(barHeight, screenHeight, gap, ring)))};
    }

    float PopupRoom(float barHeight, float screenHeight, float gap, float ring)
    {
        return screenHeight - barHeight - 2.0f * (gap + ring);
    }

    std::string FitText(const std::string& text, float width, FunctionRef<float(const std::string&)> measure)
    {
        if (text.empty() || measure(text) <= width) return text;
        // Each shorter start once, without the spaces it ends in.
        std::vector<size_t> cuts;
        for (size_t end = 1; end < text.size(); ++end)
        {
            if ((static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) continue; // never cut inside a UTF-8 character
            size_t cut = end;
            while (cut > 0 && text[cut - 1] == ' ')
                --cut;
            if (cut > 0 && (cuts.empty() || cuts.back() != cut)) cuts.push_back(cut);
        }
        // A longer start is never narrower, so the longest that fits is found by halving.
        const auto past = std::ranges::partition_point(
            cuts, [&](size_t cut) { return measure(text.substr(0, cut) + kEllipsis) <= width; });
        return past == cuts.begin() ? std::string() : text.substr(0, *(past - 1)) + kEllipsis;
    }

    void BarInput::Frame(const BarLayout& layout, const std::array<bool, kBarWidgetCount>& hasPopup)
    {
        m_Layout   = layout;
        m_HasPopup = hasPopup;
        if (m_Pinned && RectOf(m_Layout, *m_Pinned) == nullptr) m_Pinned.reset();
        Reevaluate();
    }

    void BarInput::SetPopup(const BarRect& popup)
    {
        m_Popup = popup;
        Reevaluate();
    }

    void BarInput::Move(float x, float y)
    {
        m_X = x;
        m_Y = y;
        Reevaluate();
    }

    void BarInput::Leave() { Move(kAway, kAway); }

    void BarInput::Reevaluate()
    {
        m_Under = WidgetAt(m_Layout, m_X, m_Y);
        if (m_Under && m_HasPopup[static_cast<int>(*m_Under)])
            m_Hover = m_Under;
        else if (!(m_Hover && m_HasPopup[static_cast<int>(*m_Hover)] && InPopupColumn(m_X, m_Y)))
            m_Hover.reset();
    }

    bool BarInput::InPopupColumn(float x, float y) const
    {
        return BarRect{m_Popup.x, 0.0f, m_Popup.width, m_Popup.y + m_Popup.height}.Contains(x, y);
    }

    bool BarInput::Over(float x, float y) const
    {
        return (y >= 0.0f && y < m_Layout.height) || WidgetAt(m_Layout, x, y) || (Open() && m_Popup.Contains(x, y));
    }

    bool BarInput::Press(float x, float y, bool left)
    {
        if (!Over(x, y)) return false;
        if (!left) return true;
        const std::optional<BarWidget> under = WidgetAt(m_Layout, x, y);
        if (under && m_HasPopup[static_cast<int>(*under)])
            m_Pinned = m_Pinned == under ? std::nullopt : under;
        else if (!under && Open() && m_Popup.Contains(x, y))
            m_Pinned = m_Pinned == Open() ? std::nullopt : Open();
        return true;
    }

    bool BarClicks::Take(uint32_t message, uint32_t buttons, float x, float y, bool barOn, bool overGui, bool blocked)
    {
        if (!barOn) m_Left = m_Right = false;
        const bool barsTurn = barOn && !overGui && !blocked;
        switch (message)
        {
        case kMouseMove:
            // A move with a button up is the first news of a release outside the window, which nothing else reports.
            m_Left  = m_Left && (buttons & kLeftButton) != 0;
            m_Right = m_Right && (buttons & kRightButton) != 0;
            if (barsTurn)
                m_Input.Move(x, y);
            else
                m_Input.Leave();
            return false;
        case kLeftUp: return std::exchange(m_Left, false);
        case kRightUp: return std::exchange(m_Right, false);
        // A new press shows the last one ended, even when its release went to another window.
        case kLeftDown:
        case kLeftDouble:
            m_Left = barsTurn && m_Input.Press(x, y, true);
            return m_Left;
        case kRightDown:
        case kRightDouble:
            m_Right = barsTurn && m_Input.Press(x, y, false);
            return m_Right;
        }
        return false;
    }
}
