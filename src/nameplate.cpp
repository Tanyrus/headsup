#include "nameplate.h"
#include "plate_layout.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr float kLettersPerScreen = 180.0f; // the screen is this many typical game letters tall
        constexpr float kMinScale         = 0.5f;
        constexpr float kMaxScale         = 2.5f;
        constexpr double kBobPeriod       = 1.2;    // seconds
        constexpr float kBobShare         = 0.15f;  // of the cursor's height
        constexpr float kRasterSmallest   = 6.0f;   // pixels
        constexpr float kRasterStep       = 1.25f;
        constexpr float kRasterOverscale  = 1.05f;  // text is scaled up this much before it is redrawn larger
        constexpr double kScaleEase       = 0.15;   // seconds: two thirds of the way to a new size in this time

        float RowWidth(int count, float size)
        {
            return count > 0 ? static_cast<float>(count) * size + static_cast<float>(count - 1) * kIconGap : 0.0f;
        }

        int RowCount(int count) { return (count + kIconsPerRow - 1) / kIconsPerRow; }
        // A group's first row, its widest: its icons, the gaps between them and, for badges, each one's overhang.
        float FirstRowWidth(int count, float step)
        {
            return count > 0 ? static_cast<float>(std::min(count, kIconsPerRow)) * step - kIconGap : 0.0f;
        }

        CursorKind CursorFor(uint16_t index, const Settings& settings, const CursorTargets& targets)
        {
            if (!settings.replaceCursor || targets.gameArrowsShown || index == 0) return CursorKind::None;
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
        lines.infoIconCount = mobLines && !iconsFailed ? info.infoIcons.count : 0;
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

    CursorSpot KeepOnScreen(CursorSpot spot, float width, float height, float screenWidth, float screenHeight)
    {
        return CursorSpot{std::clamp(spot.x, 0.0f, std::max(0.0f, screenWidth - width)),
            std::clamp(spot.y, 0.0f, std::max(0.0f, screenHeight - height))};
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
        const float overhang = sizes.iconSize * kBadgeOverhang;
        const float hang     = sizes.infoIconCount > 0 ? sizes.iconSize * kBadgeHang : 0.0f;
        l.iconSize           = sizes.iconSize;
        // Each group against its edge of the row, which lines up with the level line (the name when there is none); on a
        // shared side the badges take the edge and the detection icons sit a gap inside the badges' first row, their
        // widest. The row widens evenly when its groups need the room.
        const float detectionStep = sizes.iconSize + kIconGap, badgeStep = detectionStep + overhang;
        const float detectionWidth = FirstRowWidth(sizes.mobIconCount, detectionStep);
        const float badgeWidth     = FirstRowWidth(sizes.infoIconCount, badgeStep);
        const bool shared          = sizes.detectionLeft == sizes.badgesLeft;
        float leftWidth = 0.0f, rightWidth = 0.0f;
        for (const auto& [left, width] : {std::pair{sizes.detectionLeft, detectionWidth}, std::pair{sizes.badgesLeft, badgeWidth}})
        {
            float& side = left ? leftWidth : rightWidth;
            if (width > 0.0f) side += (side > 0.0f ? kIconGap : 0.0f) + width;
        }
        const float between = leftWidth > 0.0f && rightWidth > 0.0f ? kIconGap : 0.0f;
        const float text    = sizes.labelWidth > 0.0f ? sizes.labelWidth : showName ? sizes.nameWidth : 0.0f;
        const float groups  = leftWidth + between + rightWidth;
        const float row     = sizes.iconPlace == IconPlace::Center ? groups : std::max(text, groups);
        l.iconsLeftX        = centerX - row * 0.5f;
        l.iconsRightX       = centerX + row * 0.5f;
        // Inline, the row's edges are the level line's ends pushed out by each side's groups.
        const bool inline_ = sizes.iconPlace == IconPlace::Inline && sizes.labelHeight > 0.0f;
        if (inline_)
        {
            l.iconsLeftX  = l.labelX - (leftWidth > 0.0f ? kIconGap * 2.0f + leftWidth : 0.0f);
            l.iconsRightX = l.labelX + sizes.labelWidth + (rightWidth > 0.0f ? kIconGap * 2.0f + rightWidth : 0.0f);
        }
        const float inside  = shared && badgeWidth > 0.0f ? badgeWidth + kIconGap : 0.0f;
        l.detection         = IconRow{sizes.mobIconCount, detectionStep, inside, sizes.detectionLeft};
        l.info              = IconRow{sizes.infoIconCount, badgeStep, sizes.badgesLeft ? 0.0f : overhang, sizes.badgesLeft};
        l.iconRowStep  = sizes.iconSize + hang + kLineGap;
        const int rows = std::max(RowCount(sizes.mobIconCount), RowCount(sizes.infoIconCount));
        if (inline_)
        {
            // Centered on the level line; rows past the first wrap above it.
            l.iconsY = l.labelY + (sizes.labelHeight - sizes.iconSize) * 0.5f;
            if (rows > 0) bottom = std::min(bottom, l.iconsY - static_cast<float>(rows - 1) * l.iconRowStep - kLineGap);
        }
        else
        {
            l.iconsY = bottom - hang - sizes.iconSize;
            if (rows > 0) bottom = l.iconsY - static_cast<float>(rows - 1) * l.iconRowStep - kLineGap;
        }
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

    IconSpot IconAt(const NameplateLayout& l, const IconRow& row, int i)
    {
        const int line    = i / kIconsPerRow;
        const int column  = i % kIconsPerRow;
        const int inLine  = std::min(kIconsPerRow, row.count - line * kIconsPerRow);
        const float width = static_cast<float>(inLine) * l.iconSize + static_cast<float>(inLine - 1) * (row.step - l.iconSize);
        const float startX = row.left ? l.iconsLeftX + row.inset : l.iconsRightX - row.inset - width;
        return IconSpot{startX + static_cast<float>(column) * row.step, l.iconsY - static_cast<float>(line) * l.iconRowStep};
    }

    IconSpot BadgeAt(IconSpot icon, float iconSize, float width, float height)
    {
        return IconSpot{icon.x + iconSize * (1.0f + kBadgeOverhang) - width, icon.y + iconSize * (1.0f + kBadgeHang) - height};
    }

    SquareQuad PlaceSquarePixels(float x, float y, float width, float height, float halo, float fit, float toX,
        float toY)
    {
        const float around = halo * fit / toY;
        return SquareQuad{x - around, y - around, width * fit * toX / toY, height * fit};
    }

    PlateBox BoxOf(const NameplateLayout& l, const LineSizes& sizes, bool showName)
    {
        PlateBox box{l.centerX, 0.0f, l.centerX, 0.0f};
        bool any = false;
        const auto add = [&](float left, float top, float width, float height) {
            box.left   = std::min(box.left, left);
            box.right  = std::max(box.right, left + width);
            box.top    = any ? std::min(box.top, top) : top;
            box.bottom = any ? std::max(box.bottom, top + height) : top + height;
            any        = true;
        };
        if (showName)
        {
            const float right = l.rightIconsX + RowWidth(sizes.rightIconCount, sizes.playerIconSize);
            const float left  = sizes.leftIconCount > 0 ? l.leftIconsX : l.nameX;
            add(left, l.nameY, std::max(l.nameX + sizes.nameWidth, sizes.rightIconCount > 0 ? right : 0.0f) - left,
                sizes.nameHeight);
        }
        if (sizes.ornamentHeight > 0.0f) add(l.ornamentX, l.ornamentY, sizes.ornamentWidth, sizes.ornamentHeight);
        if (sizes.labelHeight > 0.0f) add(l.labelX, l.labelY, sizes.labelWidth, sizes.labelHeight);
        if (const int rows = std::max(RowCount(sizes.mobIconCount), RowCount(sizes.infoIconCount)); rows > 0)
        {
            const float top = l.iconsY - static_cast<float>(rows - 1) * l.iconRowStep;
            add(l.iconsLeftX, top, l.iconsRightX - l.iconsLeftX, l.iconsY + l.iconSize - top);
        }
        return box;
    }

    std::vector<float> StackRises(const std::vector<PlateBox>& boxes, float gap)
    {
        std::vector<float> rises(boxes.size(), 0.0f);
        for (size_t i = 0; i < boxes.size(); ++i)
        {
            const PlateBox& mine = boxes[i];
            float rise           = 0.0f;
            // Each pass that finds a plate in the way rises further, so it ends within a pass for every plate placed.
            for (bool risen = true; risen;)
            {
                risen = false;
                for (size_t j = 0; j < i; ++j)
                {
                    const PlateBox& other = boxes[j];
                    const float top = other.top - rises[j], bottom = other.bottom - rises[j];
                    if (mine.left >= other.right || other.left >= mine.right) continue;
                    if (mine.top - rise >= bottom + gap || top >= mine.bottom - rise + gap) continue;
                    // Rounding can find a plate just cleared still in the way: only a higher rise is a new one.
                    const float clear = mine.bottom - (top - gap);
                    if (clear <= rise) continue;
                    rise  = clear;
                    risen = true;
                }
            }
            rises[i] = rise;
        }
        return rises;
    }

    float EaseRise(float shown, float target, double seconds)
    {
        constexpr float kSettled = 0.5f; // pixels
        const float next = shown + (target - shown) * static_cast<float>(1.0 - std::exp(-std::max(0.0, seconds) / kScaleEase));
        return std::fabs(target - next) < kSettled ? target : next;
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

    float ScaleAtDistance(float yalms)
    {
        // The game's name size is 5 / yalms + 0.07 of full size (fitted to a name dump), and full size is 4.21875
        // times DistanceScale's 1: 180 letters a screen over the 42.67 a full-size name's letters make of it.
        constexpr float kPerYalm = 21.09f, kFarthest = 0.3f;
        if (std::isnan(yalms)) return 1.0f;
        return yalms > 0.0f ? std::clamp(kPerYalm / yalms + kFarthest, kMinScale, kMaxScale) : kMaxScale;
    }

    float EaseScale(float shown, float target, double seconds)
    {
        if (shown <= 0.0f) return target;
        return shown + (target - shown) * static_cast<float>(1.0 - std::exp(-std::max(0.0, seconds) / kScaleEase));
    }

    bool RedrawNow(bool drawn, double spentMs)
    {
        return !drawn || spentMs < kRedrawBudgetMs;
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

    uint32_t PlateAlpha(const Settings& settings, EntityKind kind, int rank, bool hasTarget)
    {
        if (kind != EntityKind::Mob || rank != kPlainRank || !hasTarget) return kOpaqueAlpha;
        return static_cast<uint32_t>(std::lround(static_cast<float>(kOpaqueAlpha) *
                                                 static_cast<float>(settings.untargetedOpacity) / 100.0f));
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
