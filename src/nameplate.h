#pragma once

#include "game_cursor.h"
#include "pose.h"
#include "screen_box.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace headsup
{
    // A body counts as drawn for this many frames after the game last drew it: some frames credit an entity's body draws
    // to whoever else is on the stack.
    constexpr uint32_t kMeshGraceFrames = 8;

    // Some of a name is enough: a replaced name must show whenever the game's would, as the game's is hidden.
    bool NameOnScreen(const ScreenBox& plate, float screenWidth, float screenHeight);

    // A posed player's name comes down to their head.
    ScreenBox PlaceName(const ScreenBox& name, const Camera* camera, const WorldPoint& feet, Pose pose);

    // Whether HeadsUp draws a cursor for this entity: your target or the one you are picking, with the cursor on and
    // the game's own arrows hooked away.
    bool CursorShown(uint16_t index, const Settings& settings, const CursorTargets& targets);

    // A target with no game name to hang a cursor on, such as a Telepoint, gets its cursor at the game's arrow anchor.
    struct LoneCursor
    {
        uint16_t index;
        TargetState target;
        float x, y;
        bool under = false; // the target's own cursor, under the candidate's on the same entity
    };
    std::vector<LoneCursor> LoneCursors(const CursorTargets& targets, const Settings& settings,
        const std::vector<uint16_t>& withNameplateCursor);

    struct CursorSpot
    {
        float x, y;
    };
    CursorSpot CursorAtAnchor(float anchorX, float anchorY, float width, float height, float tip);
    // The game's arrow over a candidate sits 10 of its 32 pixels above the one over the target: on a nameplate the
    // candidate's cursor rises this share of its height over the target's own.
    constexpr float kPickedCursorRise = 10.0f / 32.0f;
    // A cursor box moved the least that keeps all of it inside a screen this size.
    CursorSpot KeepOnScreen(CursorSpot spot, float width, float height, float screenWidth, float screenHeight);

    // How many icons a row of a run holds; the rest wrap onto rows above.
    constexpr int kIconsPerRow = 8;
    // A badge's text height, and how far its box reaches past its icon's right and bottom edges, as shares of the icon size.
    constexpr float kBadgeShare = 0.65f, kBadgeOverhang = 0.35f, kBadgeHang = 0.3f;

    struct IconSpot
    {
        float x, y;
    };
    // Where a badge of this size goes on its icon.
    IconSpot BadgeAt(IconSpot icon, float iconSize, float width, float height);

    // A texture drawn with square pixels (text, the ornament, the cursor) as a quad in the scene image: its corner in
    // screen pixels, halo included, and its size in the image's. The image is toX by toY times the screen and these
    // are sized by toY, so the width takes toX / toY: the game's copy to the screen then gives the letters their shape
    // back whatever the image's shape (a 2048 x 2048 scene on a 1920 x 1080 screen drew them 1.78 times too wide).
    struct SquareQuad
    {
        float x, y, width, height;
        bool operator==(const SquareQuad&) const = default;
    };
    SquareQuad PlaceSquarePixels(float x, float y, float width, float height, float halo, float fit, float toX,
        float toY);

    // A plate's outline on screen: its lines with their icons and the ornament.
    struct PlateBox
    {
        float left, top, right, bottom;
    };
    // How far up each plate goes so none covers another: in order, the first keeping its place, each that would cover
    // a plate already placed rises until its bottom is gap pixels over every one it would touch. Plates side by side
    // are left alone.
    std::vector<float> StackRises(const std::vector<PlateBox>& boxes, float gap);
    // A plate's rise seconds later, on its way to where it should be: most of the way in a fraction of a second, and
    // there once within half a pixel.
    float EaseRise(float shown, float target, double seconds);

    float CursorBob(double seconds, float height);

    // Not rounded, so sizes change as smoothly as the game's names; kept within the two limits.
    float DistanceScale(float letterHeight, float screenHeight, float minScale, float maxScale);
    // The size DistanceScale gives a game's name this many yalms from the camera, without the game's steps (it sizes
    // its names in eightieths, by its depth from the camera, so they change in jumps and as the camera turns): 1 for
    // a distance that is not a number.
    float ScaleAtDistance(float yalms, float minScale, float maxScale);
    // The game steps its name size, by 5% at a time far away; a plate eases toward each step. A plate with no size yet
    // (0) takes the size at once.
    float EaseScale(float shown, float target, double seconds);
    // A frame's time for redrawing textures whose look changed, at a size step or a menu change; the rest keep their old
    // texture a frame longer. One with nothing to show yet is always drawn.
    constexpr double kRedrawBudgetMs = 3.0;
    bool RedrawNow(bool drawn, double spentMs);
    // Whether a plate's size follows the distance: yours only with scaleOwnName on.
    bool ScalesWithDistance(const Settings& settings, bool self);
    // Plates of a higher rank draw over every plate of a lower one: your target's, and over it the one you are picking.
    constexpr int kPlainRank = 0, kTargetRank = 1, kPickedRank = 2;
    int PlateRank(uint16_t index, const CursorTargets& targets);
    // A plate's alpha, out of kOpaqueAlpha: a mob's that is not picked fades to the menu's opacity while you have a
    // target.
    uint32_t PlateAlpha(const Settings& settings, EntityKind kind, int rank, bool hasTarget);
    // Within a rank, farther plates first, so nearer ones cover them.
    bool DrawnBefore(int rankA, float depthA, int rankB, float depthB);
    // Heights come in steps, and current (the height drawn now) is kept until the shown size falls a step below it or
    // rises a little above it, so text is redrawn only when its size really changes and never flickers between two.
    // The steps end at a tallest one, which text shown taller is stretched from.
    int RasterHeight(float pixels, int current);
    // The height text that keeps its size is drawn at: the size in whole pixels, to RasterHeight's tallest.
    int FixedRasterHeight(float pixels);
    // Whether an image whose ink is width by height, with halo pixels around it, fits a texture of this side.
    bool FitsTexture(int width, int height, int halo, int side);
    // The height to try so the image fits: raster when the image drawn at it fits, else raster scaled down by what the
    // image is over, in whole pixels, not below the smallest step and never above raster.
    int FittingRaster(int raster, int width, int height, int halo, int side);
    // An image as measured at one height: its ink box and the halo around it.
    struct ImageSize
    {
        int width = 0, height = 0, halo = 0;
    };
    // The tallest height, raster at most, at which the image fits a texture of this side, or nothing when it fits at
    // none. Letters are whole pixels wide at every height, so a scaled-down image is measured and scaled again.
    std::optional<int> RasterThatFits(int raster, int side, const std::function<ImageSize(int)>& measure);
}
