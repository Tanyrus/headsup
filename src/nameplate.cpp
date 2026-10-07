#include "nameplate.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace headsup
{
    namespace
    {
        constexpr float kAboveNameGap     = -3.0f;  // pixels: negative, as the fonts' own space above the letters is enough
        constexpr float kLineGap          = 2.0f;   // pixels between stacked lines, and above the game's name
        constexpr float kIconGap          = 2.0f;   // pixels between icons
        constexpr float kLettersPerScreen = 180.0f; // the screen is this many typical game letters tall
        constexpr float kMinScale         = 0.5f;
        constexpr float kMaxScale         = 2.5f;
        constexpr double kBobPeriod       = 1.2;    // seconds
        constexpr float kBobShare         = 0.15f;  // of the cursor's height
        constexpr float kRasterSmallest   = 6.0f;   // pixels
        constexpr float kRasterStep       = 1.25f;
        constexpr float kRasterOverscale  = 1.05f;  // text is scaled up this much before it is redrawn larger

        float RowWidth(int count, float size)
        {
            return count > 0 ? static_cast<float>(count) * size + static_cast<float>(count - 1) * kIconGap : 0.0f;
        }
    }

    bool Steady(uint32_t meshDraws, uint32_t nameFramesInRow) { return meshDraws != 0 && nameFramesInRow >= kStableFrames; }

    bool NameOnScreen(const ScreenBox& plate, float screenWidth, float screenHeight)
    {
        return plate.valid && plate.maxX > 0.0f && plate.maxY > 0.0f && plate.minX < screenWidth && plate.minY < screenHeight;
    }

    PlateLines ChooseLines(const PlateFacts& facts, const Settings& settings, const CursorTargets& targets, bool iconsFailed)
    {
        PlateLines lines;
        lines.name      = facts.replaced;
        lines.label     = facts.steady && settings.showLabels && facts.alive && facts.hasLabel;
        lines.icons     = facts.steady && settings.showIcons && facts.alive && !iconsFailed ? facts.mobIcons : 0;
        const bool playerIcons = lines.name && settings.showPlayerIcons && !iconsFailed;
        lines.leftIcons        = playerIcons ? facts.leftIcons : 0;
        lines.rightIcons       = playerIcons ? facts.rightIcons : 0;
        if (settings.replaceCursor && facts.index != 0)
        {
            if (facts.index == targets.subTarget)
                lines.cursor = CursorKind::SubTarget;
            else if (facts.index == targets.target)
                lines.cursor = targets.locked ? CursorKind::Locked : CursorKind::Target;
        }
        return lines;
    }

    NameplateLayout LayoutNameplate(const ScreenBox& plate, const LineSizes& sizes, bool showName)
    {
        NameplateLayout l{};
        const float centerX        = plate.CenterX();
        float bottom               = plate.minY - kLineGap; // the next line up ends here
        // Each side's icons and the gap between them and the name.
        auto beside = [&](int count) { return count > 0 ? RowWidth(count, sizes.nameIconSize) + kIconGap : 0.0f; };
        const float left = beside(sizes.leftIconCount), right = beside(sizes.rightIconCount);
        if (showName)
        {
            const float shift = sizes.centerNameAndIcons ? (left - right) * 0.5f : 0.0f;
            l.nameX           = centerX - sizes.nameWidth * 0.5f + shift;
            l.nameY           = plate.CenterY() - sizes.nameHeight * 0.5f;
            bottom            = l.nameY - kAboveNameGap;
        }
        if (sizes.labelHeight > 0.0f)
        {
            l.labelX = centerX - sizes.labelWidth * 0.5f;
            l.labelY = bottom - sizes.labelHeight;
            bottom   = l.labelY - kLineGap;
        }
        l.iconsX   = centerX - RowWidth(sizes.iconCount, sizes.iconSize) * 0.5f;
        l.iconsY   = bottom - sizes.iconSize;
        l.iconStep = sizes.iconSize + kIconGap;
        if (sizes.iconCount > 0) bottom = l.iconsY - kLineGap;
        l.nameIconStep = sizes.nameIconSize + kIconGap;
        l.leftIconsX   = l.nameX - left;
        l.rightIconsX  = l.nameX + sizes.nameWidth + kIconGap;
        l.nameIconsY   = l.nameY + (sizes.nameHeight - sizes.nameIconSize) * 0.5f;
        l.cursorX      = centerX - sizes.cursorWidth * sizes.cursorTip;
        l.cursorY      = bottom - sizes.cursorHeight;
        return l;
    }

    ScreenBox CenteredOver(const ScreenBox& letters, const ScreenBox& whole)
    {
        ScreenBox moved = letters;
        const float dx  = whole.CenterX() - letters.CenterX();
        moved.minX += dx;
        moved.maxX += dx;
        return moved;
    }

    float CursorBob(double seconds, float height)
    {
        const double phase = std::sin(2.0 * std::numbers::pi * seconds / kBobPeriod);
        return -static_cast<float>((phase + 1.0) * 0.5) * height * kBobShare;
    }

    float DistanceScale(float letterHeight, float screenHeight)
    {
        const float factor = screenHeight > 0.0f ? letterHeight / (screenHeight / kLettersPerScreen) : 1.0f;
        return std::isfinite(factor) ? std::clamp(factor, kMinScale, kMaxScale) : 1.0f;
    }

    int RasterHeight(float pixels, int current)
    {
        const auto drawn = static_cast<float>(current);
        if (current > 0 && pixels <= drawn * kRasterOverscale && pixels * kRasterStep >= drawn) return current;
        float size = kRasterSmallest;
        while (size < pixels)
            size *= kRasterStep;
        return static_cast<int>(std::lround(size));
    }
}
