#include "plate_layout.h"

#include <algorithm>
#include <cstddef>

namespace headsup
{
    namespace
    {
        float TextTop(const LinePlace& place, const LineInput& line)
        {
            return place.top + (place.height - line.textHeight) / 2.0f;
        }

        bool Badged(const PlateItem& item) { return std::ranges::any_of(item.icons, HasBadge); }

        int BadgesIn(const PlateItem& run, size_t first, size_t count)
        {
            const auto begin = run.icons.begin() + static_cast<std::ptrdiff_t>(first);
            return static_cast<int>(std::count_if(begin, begin + static_cast<std::ptrdiff_t>(count), HasBadge));
        }

        float RowStep(const PlateItem& run, float iconSize)
        {
            return iconSize + (Badged(run) ? iconSize * kBadgeHang : 0.0f) + kLineGap;
        }

        // From its top row's top to its bottom row's bottom; one icon's for an item without icons.
        float RowsHeight(const PlateItem& run, float iconSize)
        {
            const size_t above = (std::max(run.icons.size(), size_t{1}) - 1) / kIconsPerRow;
            return iconSize + static_cast<float>(above) * RowStep(run, iconSize);
        }
    }

    namespace
    {
        // What stands before a line's first text item or after its last, with the gap beside each: icons, and the
        // spaces between them and the text, which hang past the text as it aligns. Nothing on a line without text.
        float Hang(const LineInput& line, bool before)
        {
            if (line.firstText < 0) return 0.0f;
            const size_t from = before ? 0 : static_cast<size_t>(line.lastText) + 1;
            const size_t to   = before ? static_cast<size_t>(line.firstText) : line.items.size();
            float hang        = 0.0f;
            for (size_t j = from; j < to; ++j)
                hang += line.items[j].width + kItemGap;
            return hang;
        }

        void PlaceOrnament(LinePlace& place, const LineInput& in, float centerX, float top)
        {
            place.ornamentWidth  = in.ornamentWidth;
            place.ornamentHeight = in.ornamentHeight;
            place.ornament       = {centerX - in.ornamentWidth / 2.0f, top};
        }
    }

    PlateLayout LayoutPlate(const ScreenBox& anchor, std::span<const LineInput> lines, float cursorWidth,
        float cursorHeight, float cursorTip, int timerCount, float timerHeight, float cursorRaise)
    {
        PlateLayout layout;
        layout.centerX = anchor.CenterX();
        layout.lines.resize(lines.size());
        for (size_t i = 0; i < lines.size(); ++i)
        {
            LinePlace& place = layout.lines[i];
            for (size_t j = 0; j < lines[i].items.size(); ++j)
            {
                place.width += (j > 0 ? kItemGap : 0.0f) + lines[i].items[j].width;
                place.height = std::max(place.height, lines[i].items[j].height);
            }
        }
        // A line's text aligns, and the icons at its ends hang past it. The bottom line's stays centered on the game's
        // name. Another starts where it starts or ends where it ends, whichever is further to the side it is set to:
        // one wider than the name hangs out on that side.
        const auto core     = [&](size_t i) { return layout.lines[i].width - Hang(lines[i], true) - Hang(lines[i], false); };
        const float against = lines.empty() ? 0.0f : core(lines.size() - 1);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            LinePlace& place    = layout.lines[i];
            const float width   = core(i);
            const float starts  = layout.centerX - against / 2.0f;
            const float ends    = layout.centerX + against / 2.0f - width;
            switch (lines[i].align)
            {
            case Align::Left: place.left = std::min(starts, ends); break;
            case Align::Center: place.left = layout.centerX - width / 2.0f; break;
            case Align::Right: place.left = std::max(starts, ends); break;
            }
            place.left -= Hang(lines[i], true);
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
            if (lowerIn.ornamentHeight > 0.0f && !lowerIn.ornamentUnder)
            {
                PlaceOrnament(lower, lowerIn, layout.centerX, sunk - kLineGap - lowerIn.ornamentHeight);
                bottom = lower.ornament.y - kLineGap;
            }
            else
                bottom = lowerIn.textHeight > 0.0f && !upperIn.hasIcons ? sunk : lower.top - kLineGap;
            upper.top = bottom - upper.height - upperIn.padBelow - lowerIn.padAbove;
        }

        layout.top  = anchor.minY;
        float above = anchor.minY - kLineGap;
        if (!lines.empty())
        {
            LinePlace& first = layout.lines.front();
            layout.top       = first.top;
            above            = lines.front().textHeight > 0.0f ? TextTop(first, lines.front()) + kNameOverlap : layout.top - kLineGap;
            // Over the plate's first line: what stands on the plate then stands a gap over the ornament.
            if (lines.front().ornamentHeight > 0.0f && !lines.front().ornamentUnder)
            {
                PlaceOrnament(first, lines.front(), layout.centerX, above - kLineGap - lines.front().ornamentHeight);
                layout.top = std::min(layout.top, first.ornament.y);
                above      = first.ornament.y - kLineGap;
            }
            LinePlace& last = layout.lines.back();
            if (lines.back().ornamentHeight > 0.0f && lines.back().ornamentUnder)
                PlaceOrnament(last, lines.back(), layout.centerX, last.top + last.height + kLineGap);
        }

        layout.timerStep = timerHeight + kLineGap;
        float cursorY    = above;
        layout.timersY   = above;
        if (timerCount > 0)
        {
            layout.timersY = above + kLineGap - static_cast<float>(timerCount) * layout.timerStep;
            cursorY        = layout.timersY - kLineGap;
        }
        layout.cursor = CursorAtAnchor(layout.centerX, cursorY, cursorWidth, cursorHeight, cursorTip, cursorRaise);

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

    float IconOffset(int before, int badged, float iconSize)
    {
        return static_cast<float>(before) * (iconSize + kIconGap) + static_cast<float>(badged) * iconSize * kBadgeOverhang;
    }

    float IconRunWidth(int count, int badges, float iconSize)
    {
        return count > 0 ? IconOffset(count, badges, iconSize) - kIconGap : 0.0f;
    }

    RunShape ShapeOfRun(const ComposedLine& line, size_t item, float scale)
    {
        const auto isText    = [](const PlateItem& other) { return other.kind == ItemKind::Text; };
        const PlateItem& run = line.items[item];
        const auto first     = std::min(run.icons.size(), static_cast<size_t>(kIconsPerRow));
        RunShape shape;
        shape.iconSize = line.look.iconSize * scale;
        shape.rowStep  = RowStep(run, shape.iconSize);
        shape.width    = IconRunWidth(static_cast<int>(first), BadgesIn(run, 0, first), shape.iconSize);
        shape.right    = line.items.size() > 1 && item + 1 == line.items.size();
        float rows     = RowsHeight(run, shape.iconSize);
        float hang     = 0.0f;
        if (std::ranges::none_of(line.items, isText))
        {
            for (const PlateItem& other : line.items)
                rows = std::max(rows, RowsHeight(other, shape.iconSize));
            if (std::ranges::any_of(line.items, Badged)) hang = shape.iconSize * kBadgeHang;
        }
        shape.height       = rows + hang;
        shape.bottomRowTop = rows - shape.iconSize;
        return shape;
    }

    IconSpot IconInRun(const RunShape& shape, const PlateItem& run, size_t i)
    {
        const size_t row   = i / kIconsPerRow;
        const size_t first = row * kIconsPerRow;
        const size_t inRow = std::min(run.icons.size() - first, static_cast<size_t>(kIconsPerRow));
        const float width  = IconRunWidth(static_cast<int>(inRow), BadgesIn(run, first, inRow), shape.iconSize);
        return IconSpot{(shape.right ? shape.width - width : 0.0f) +
                            IconOffset(static_cast<int>(i - first), BadgesIn(run, first, i - first), shape.iconSize),
            shape.bottomRowTop - static_cast<float>(row) * shape.rowStep};
    }

    void MeasureLine(const ComposedLine& line, std::span<const ItemSize> texts, float scale, LineInput& into)
    {
        into.items.clear();
        into.align          = line.look.align;
        into.textHeight     = 0.0f;
        into.hasIcons       = false;
        // The bottom design line stays on the game's name, so its padding is the space over it. A line broken into
        // rows has it under its last row, or over the first.
        const bool bottom   = line.source == kBottomLine;
        into.padBelow       = !bottom && line.lastRow ? line.look.padding * scale : 0.0f;
        into.padAbove       = bottom && line.row == 0 ? line.look.padding * scale : 0.0f;
        into.ornamentWidth  = 0.0f;
        into.ornamentHeight = 0.0f;
        into.ornamentUnder  = false;
        into.firstText      = -1;
        into.lastText       = -1;
        size_t text         = 0;
        for (size_t i = 0; i < line.items.size(); ++i)
        {
            const PlateItem& item = line.items[i];
            switch (item.kind)
            {
            case ItemKind::Text:
                if (into.firstText < 0) into.firstText = static_cast<int>(i);
                into.lastText   = static_cast<int>(i);
                into.textHeight = std::max(into.textHeight, texts[text].height);
                into.items.push_back(texts[text++]);
                break;
            case ItemKind::Icons:
            {
                const RunShape shape = ShapeOfRun(line, i, scale);
                into.hasIcons        = true;
                into.items.push_back({shape.width, shape.height});
                break;
            }
            case ItemKind::Space:
                into.items.push_back({static_cast<float>(item.spaces) * kSpaceShare * line.look.textSize * scale, 0.0f});
                break;
            }
        }
    }
}
