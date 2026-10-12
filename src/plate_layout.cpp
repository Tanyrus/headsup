#include "plate_layout.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        float TextTop(const LinePlace& place, const LineInput& line)
        {
            return place.top + (place.height - line.textHeight) / 2.0f;
        }
    }

    PlateLayout LayoutPlate(const ScreenBox& anchor, const std::vector<LineInput>& lines, float cursorWidth,
        float cursorHeight, float cursorTip, int timerCount, float timerHeight)
    {
        PlateLayout layout;
        layout.centerX = anchor.CenterX();
        layout.lines.resize(lines.size());
        float widest = 0.0f;
        for (size_t i = 0; i < lines.size(); ++i)
        {
            LinePlace& place = layout.lines[i];
            for (size_t j = 0; j < lines[i].items.size(); ++j)
            {
                place.width += (j > 0 ? kItemGap : 0.0f) + lines[i].items[j].width;
                place.height = std::max(place.height, lines[i].items[j].height);
            }
            widest = std::max(widest, place.width);
        }
        for (size_t i = 0; i < lines.size(); ++i)
        {
            LinePlace& place = layout.lines[i];
            switch (lines[i].align)
            {
            case Align::Left: place.left = layout.centerX - widest / 2.0f; break;
            case Align::Center: place.left = layout.centerX - place.width / 2.0f; break;
            case Align::Right: place.left = layout.centerX + widest / 2.0f - place.width; break;
            }
        }

        if (!lines.empty()) layout.lines.back().top = anchor.CenterY() - layout.lines.back().height / 2.0f;
        for (size_t i = lines.size(); i-- > 1;)
        {
            const LineInput& lowerIn = lines[i];
            const LineInput& upperIn = lines[i - 1];
            LinePlace& lower         = layout.lines[i];
            LinePlace& upper         = layout.lines[i - 1];
            const float sunk         = lowerIn.textHeight > 0.0f ? TextTop(lower, lowerIn) + kNameOverlap : lower.top;
            float bottom;
            if (lowerIn.ornamentHeight > 0.0f)
            {
                lower.ornamentWidth  = lowerIn.ornamentWidth;
                lower.ornamentHeight = lowerIn.ornamentHeight;
                lower.ornament.y = sunk - kLineGap - lowerIn.ornamentHeight;
                lower.ornament.x = layout.centerX - lowerIn.ornamentWidth / 2.0f;
                bottom           = lower.ornament.y - kLineGap;
            }
            else
                bottom = lowerIn.textHeight > 0.0f && !upperIn.hasIcons ? sunk : lower.top - kLineGap;
            upper.top = bottom - upper.height;
        }

        layout.top  = anchor.minY;
        float above = anchor.minY - kLineGap;
        if (!lines.empty())
        {
            layout.top = layout.lines.front().top;
            above      = lines.front().textHeight > 0.0f ? TextTop(layout.lines.front(), lines.front()) + kNameOverlap
                                                         : layout.top - kLineGap;
        }

        layout.timerStep = timerHeight + kLineGap;
        float cursorY    = above;
        layout.timersY   = above;
        if (timerCount > 0)
        {
            layout.timersY = above + kLineGap - static_cast<float>(timerCount) * layout.timerStep;
            cursorY        = layout.timersY - kLineGap;
        }
        layout.cursor = CursorAtAnchor(layout.centerX, cursorY, cursorWidth, cursorHeight, cursorTip);

        for (size_t i = 0; i < lines.size(); ++i)
        {
            LinePlace& place = layout.lines[i];
            float x          = place.left;
            place.items.reserve(lines[i].items.size());
            for (const ItemSize& item : lines[i].items)
            {
                place.items.push_back({x, place.top + (place.height - item.height) / 2.0f});
                x += item.width + kItemGap;
            }
        }
        return layout;
    }

    PlateBox BoxOf(const PlateLayout& layout)
    {
        PlateBox box{layout.centerX, 0.0f, layout.centerX, 0.0f};
        bool any       = false;
        const auto add = [&](float left, float top, float width, float height) {
            box.left   = std::min(box.left, left);
            box.right  = std::max(box.right, left + width);
            box.top    = any ? std::min(box.top, top) : top;
            box.bottom = any ? std::max(box.bottom, top + height) : top + height;
            any        = true;
        };
        for (const LinePlace& place : layout.lines)
        {
            if (!place.items.empty()) add(place.left, place.top, place.width, place.height);
            if (place.ornamentHeight > 0.0f) add(place.ornament.x, place.ornament.y, place.ornamentWidth, place.ornamentHeight);
        }
        return box;
    }

    float IconRunWidth(int count, float iconSize, bool badges)
    {
        if (count <= 0) return 0.0f;
        return static_cast<float>(count) * (iconSize + kIconGap + (badges ? iconSize * kBadgeOverhang : 0.0f)) - kIconGap;
    }
}
