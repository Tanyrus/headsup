#include "nameplate.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace headsup
{
    namespace
    {
        constexpr float kNameOverlap      = 3.0f;   // pixels: the fonts' own space above the letters is gap enough
        constexpr float kLineGap          = 2.0f;   // pixels
        constexpr float kIconGap          = 2.0f;   // pixels
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

        CursorKind CursorFor(uint16_t index, const Settings& settings, const CursorTargets& targets)
        {
            if (!settings.replaceCursor || index == 0) return CursorKind::None;
            if (index == targets.subTarget) return targets.outOfRange ? CursorKind::OutOfRange : CursorKind::SubTarget;
            if (index == targets.target) return targets.locked ? CursorKind::Locked : CursorKind::Target;
            return CursorKind::None;
        }
    }

    bool NameOnScreen(const ScreenBox& plate, float screenWidth, float screenHeight)
    {
        return plate.valid && plate.maxX > 0.0f && plate.maxY > 0.0f && plate.minX < screenWidth && plate.minY < screenHeight;
    }

    ScreenBox PlaceName(const ScreenBox& name, const Camera* camera, const WorldPoint& feet, Pose pose)
    {
        ScreenBox placed = name;
        if (camera == nullptr) return placed;
        const float lower = PosedNameRow(*camera, feet, placed.maxY, pose) - placed.maxY;
        placed.minY += lower;
        placed.maxY += lower;
        return placed;
    }

    PlateLines ChooseLines(const ActorInfo& info, bool selfEngaged, const Settings& settings,
        const CursorTargets& targets, bool iconsFailed)
    {
        PlateLines lines;
        lines.name          = ReplacesName(settings, info);
        const bool hidden   = (settings.hideClaimedByParty && info.claimedByParty) || (settings.hideClaimed && info.claimed) ||
                            (settings.hideTooWeak && info.tooWeak) || (settings.hideWhileEngaged && selfEngaged);
        const bool mobLines = info.alive && !hidden;
        lines.label         = mobLines && info.label.text[0] != '\0';
        lines.mobIconCount  = mobLines && settings.showIcons && !iconsFailed ? info.mobIcons.count : 0;
        const bool showPlayerIcons = lines.name && settings.showPlayerIcons && !iconsFailed;
        lines.leftIconCount        = showPlayerIcons ? info.playerIcons.left.count : 0;
        lines.rightIconCount       = showPlayerIcons ? info.playerIcons.right.count : 0;
        lines.cursor               = CursorFor(info.index, settings, targets);
        return lines;
    }

    std::vector<LoneCursor> LoneCursors(const CursorTargets& targets, const Settings& settings,
        const std::vector<uint16_t>& withNameplateCursor)
    {
        std::vector<LoneCursor> lone;
        if (!targets.anchored) return lone;
        auto add = [&](uint16_t index, float x, float y) {
            const CursorKind kind  = CursorFor(index, settings, targets);
            const bool onNameplate = std::ranges::find(withNameplateCursor, index) != withNameplateCursor.end();
            if (kind != CursorKind::None && !onNameplate) lone.push_back(LoneCursor{index, kind, x, y});
        };
        add(targets.subTarget, targets.subAnchorX, targets.subAnchorY);
        if (targets.target != targets.subTarget) add(targets.target, targets.anchorX, targets.anchorY);
        return lone;
    }

    CursorSpot CursorAtAnchor(float anchorX, float anchorY, float width, float height, float tip)
    {
        return CursorSpot{anchorX - width * tip, anchorY - height};
    }

    NameplateLayout LayoutNameplate(const ScreenBox& plate, const LineSizes& sizes, bool showName)
    {
        NameplateLayout l{};
        const float centerX        = plate.CenterX();
        l.centerX                  = centerX;
        float bottom               = plate.minY - kLineGap;
        auto rowAndGap = [&](int count) { return count > 0 ? RowWidth(count, sizes.playerIconSize) + kIconGap : 0.0f; };
        const float left = rowAndGap(sizes.leftIconCount), right = rowAndGap(sizes.rightIconCount);
        if (showName)
        {
            const float shift = sizes.centerNameAndIcons ? (left - right) * 0.5f : 0.0f;
            l.nameX           = centerX - sizes.nameWidth * 0.5f + shift;
            l.nameY           = plate.CenterY() - sizes.nameHeight * 0.5f;
            bottom            = l.nameY + kNameOverlap;
        }
        if (sizes.ornamentHeight > 0.0f)
        {
            l.ornamentX = centerX - sizes.ornamentWidth * 0.5f;
            l.ornamentY = bottom - kLineGap - sizes.ornamentHeight;
            bottom      = l.ornamentY - kLineGap;
        }
        if (sizes.labelHeight > 0.0f)
        {
            l.labelX = centerX - sizes.labelWidth * 0.5f;
            l.labelY = bottom - sizes.labelHeight;
            bottom   = l.labelY - kLineGap;
        }
        l.iconsX   = centerX - RowWidth(sizes.mobIconCount, sizes.iconSize) * 0.5f;
        l.iconsY   = bottom - sizes.iconSize;
        l.iconStep = sizes.iconSize + kIconGap;
        if (sizes.mobIconCount > 0) bottom = l.iconsY - kLineGap;
        l.timerStep = sizes.timerHeight + kLineGap;
        l.timersY   = bottom + kLineGap - static_cast<float>(sizes.timerCount) * l.timerStep;
        if (sizes.timerCount > 0) bottom = l.timersY - kLineGap;
        l.playerIconStep = sizes.playerIconSize + kIconGap;
        l.leftIconsX     = l.nameX - left;
        l.rightIconsX    = l.nameX + sizes.nameWidth + kIconGap;
        l.playerIconsY   = l.nameY + (sizes.nameHeight - sizes.playerIconSize) * 0.5f;
        l.cursor         = CursorAtAnchor(centerX, bottom, sizes.cursorWidth, sizes.cursorHeight, sizes.cursorTip);
        return l;
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

    bool ScalesWithDistance(const Settings& settings, EntityKind kind, bool self)
    {
        if (!settings.scaleWithDistance) return false;
        if (self) return settings.scaleOwnName;
        return kind != EntityKind::Player || settings.scalePlayerNames;
    }

    int PlateRank(uint16_t index, const CursorTargets& targets)
    {
        if (index == 0) return kPlainRank;
        if (index == targets.subTarget) return kPickedRank;
        return index == targets.target ? kTargetRank : kPlainRank;
    }

    bool DrawnBefore(int rankA, float depthA, int rankB, float depthB)
    {
        return rankA != rankB ? rankA < rankB : depthA > depthB;
    }

    int NameSize(const Settings& settings, EntityKind kind, bool self)
    {
        if (self) return settings.selfNameSize;
        switch (kind)
        {
            case EntityKind::Mob: return settings.mobNameSize;
            case EntityKind::Player: return settings.playerNameSize;
            case EntityKind::Npc: return settings.npcNameSize;
        }
        return settings.mobNameSize;
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
