#include "nameplate.h"
#include "plate_image.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace headsup
{
    namespace
    {
        constexpr float kLettersPerScreen = 180.0f; // the screen is this many typical game letters tall
        constexpr double kBobPeriod       = 1.2;    // seconds
        constexpr float kBobShare         = 0.15f;  // of the cursor's height
        constexpr float kCursorEdgePerHeight = 0.05f; // the cursor's edge at 100 percent
        constexpr float kWholePercent        = 100.0f;
        constexpr float kRasterSmallest   = 6.0f;   // pixels
        constexpr float kRasterStep       = 1.25f;
        constexpr float kRasterOverscale  = 1.05f;  // text is scaled up this much before it is redrawn larger
        // The step 48 px letters reach at 2.5 times their size in an image 1.5 times the screen (180 px). A texture
        // cannot grow with a design's largest sizes, so taller text is this one stretched.
        constexpr int kRasterTallest      = 213;
        constexpr double kScaleEase       = 0.15;   // seconds: two thirds of the way to a new size in this time

        // An image's longer side with its halo on both ends.
        int LongerSide(int width, int height, int halo) { return std::max(width, height) + 2 * halo; }

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

    bool CursorShown(uint16_t index, const Settings& settings, const CursorTargets& targets)
    {
        return settings.cursorStyle != CursorStyle::Vanilla && !targets.gameArrowsShown && Targeted(TargetStateOf(index, targets));
    }

    std::vector<LoneCursor> LoneCursors(const CursorTargets& targets, const Settings& settings,
        const std::vector<uint16_t>& withNameplateCursor)
    {
        std::vector<LoneCursor> lone;
        if (!targets.anchored) return lone;
        auto add = [&](uint16_t index, float x, float y) {
            const bool onNameplate = std::ranges::find(withNameplateCursor, index) != withNameplateCursor.end();
            if (CursorShown(index, settings, targets) && !onNameplate)
                lone.push_back(LoneCursor{index, TargetStateOf(index, targets), x, y});
        };
        // Drawn in this order: on one entity the target's own cursor goes under the candidate's.
        if (const TargetState under = TargetUnderPick(targets.target, targets); under != TargetState::None)
        {
            const bool onNameplate = std::ranges::find(withNameplateCursor, targets.target) != withNameplateCursor.end();
            if (CursorShown(targets.target, settings, targets) && !onNameplate)
                lone.push_back(LoneCursor{targets.target, under, targets.anchorX, targets.anchorY, true});
        }
        add(targets.subTarget, targets.subAnchorX, targets.subAnchorY);
        if (targets.target != targets.subTarget) add(targets.target, targets.anchorX, targets.anchorY);
        return lone;
    }

    bool AnchorOnScreen(float x, float y, float screenWidth, float screenHeight)
    {
        return x > 0.0f && y > 0.0f && x < screenWidth && y < screenHeight;
    }

    CursorSpot CursorAtAnchor(float anchorX, float anchorY, float width, float height, float tip, float raise)
    {
        return CursorSpot{anchorX - width * tip, anchorY - height - raise};
    }

    int CursorPixelWidth(int pixelHeight, int width, int height)
    {
        return WholePixels(static_cast<float>(pixelHeight) * static_cast<float>(width) / static_cast<float>(std::max(1, height)));
    }

    int CursorEdgePixels(int pixelHeight, int percent)
    {
        return WholePixels(static_cast<float>(pixelHeight) * kCursorEdgePerHeight * static_cast<float>(percent) / kWholePercent);
    }

    CursorSpot KeepOnScreen(CursorSpot spot, float width, float height, float screenWidth, float screenHeight, float top)
    {
        return CursorSpot{std::clamp(spot.x, 0.0f, std::max(0.0f, screenWidth - width)),
            std::clamp(spot.y, top, std::max(top, screenHeight - height))};
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

    float CursorBob(double seconds, float height, bool floating)
    {
        if (!floating) return 0.0f;
        const double phase = std::sin(2.0 * std::numbers::pi * seconds / kBobPeriod);
        return -static_cast<float>((phase + 1.0) * 0.5) * height * kBobShare;
    }

    float DistanceScale(float letterHeight, float screenHeight, float minScale, float maxScale)
    {
        const float factor = screenHeight > 0.0f ? letterHeight / (screenHeight / kLettersPerScreen) : 1.0f;
        return std::isfinite(factor) ? std::clamp(factor, minScale, maxScale) : 1.0f;
    }

    float ScaleAtDistance(float yalms, float minScale, float maxScale)
    {
        // The game's name size is 5 / yalms + 0.07 of full size (fitted to a name dump), and full size is 4.21875
        // times DistanceScale's 1: 180 letters a screen over the 42.67 a full-size name's letters make of it.
        constexpr float kPerYalm = 21.09f, kFarthest = 0.3f;
        if (std::isnan(yalms)) return 1.0f;
        return yalms > 0.0f ? std::clamp(kPerYalm / yalms + kFarthest, minScale, maxScale) : maxScale;
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

    bool ScalesWithDistance(const Settings& settings, bool self)
    {
        return settings.scaleWithDistance && (!self || settings.scaleOwnName);
    }

    int PlateRank(uint16_t index, const CursorTargets& targets)
    {
        if (index == 0) return kPlainRank;
        if (index == targets.subTarget) return kPickedRank;
        return index == targets.target ? kTargetRank : kPlainRank;
    }

    uint32_t PlateAlpha(const OpacitySettings& opacity, int rank, bool hasTarget, float distance)
    {
        const int percent = rank != kPlainRank ? opacity.targeted : hasTarget ? opacity.notTargeted : opacity.noTarget;
        const float share = opacity.fadeIn ? FadeShare(distance, opacity.range) : 1.0f;
        return static_cast<uint32_t>(std::lround(static_cast<float>(kOpaqueAlpha) * static_cast<float>(percent) / kWholePercent * share));
    }

    bool DrawnBefore(int rankA, float depthA, int rankB, float depthB)
    {
        return rankA != rankB ? rankA < rankB : depthA > depthB;
    }

    int RasterHeight(float pixels, int current)
    {
        const auto drawn = static_cast<float>(current);
        if (current > 0 && pixels <= drawn * kRasterOverscale && pixels * kRasterStep >= drawn) return current;
        float size = kRasterSmallest;
        while (size < pixels && size < static_cast<float>(kRasterTallest))
            size *= kRasterStep;
        return std::min(static_cast<int>(std::lround(size)), kRasterTallest);
    }

    int FixedRasterHeight(float pixels)
    {
        return WholePixels(std::min(pixels, static_cast<float>(kRasterTallest)));
    }

    bool FitsTexture(int width, int height, int halo, int side)
    {
        return LongerSide(width, height, halo) <= side;
    }

    int FittingRaster(int raster, int width, int height, int halo, int side)
    {
        if (FitsTexture(width, height, halo, side)) return raster;
        const long long fitting = static_cast<long long>(raster) * side / LongerSide(width, height, halo);
        return std::min(raster, std::max(static_cast<int>(kRasterSmallest), static_cast<int>(fitting)));
    }

    std::optional<int> RasterThatFits(int raster, int side, const std::function<ImageSize(int)>& measure)
    {
        // Each pass is at a smaller height, so there are no more passes than heights.
        for (int height = raster;;)
        {
            const ImageSize image = measure(height);
            if (FitsTexture(image.width, image.height, image.halo, side)) return height;
            const int smaller = FittingRaster(height, image.width, image.height, image.halo, side);
            if (smaller >= height) return std::nullopt;
            height = smaller;
        }
    }
}
