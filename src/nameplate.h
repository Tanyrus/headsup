#pragma once

#include "game_cursor.h"
#include "pose.h"
#include "screen_box.h"
#include "settings.h"
#include "tracker.h"

#include <cstdint>
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

    enum class CursorKind : uint8_t
    {
        None,
        Target,
        Locked,
        SubTarget,  // the candidate while picking a sub-target
        OutOfRange, // that candidate, out of range of the spell or ability
    };
    struct PlateLines
    {
        bool name          = false;
        bool label         = false;
        int mobIconCount   = 0;
        int infoIconCount  = 0; // the badge group
        int leftIconCount  = 0;
        int rightIconCount = 0;
        CursorKind cursor  = CursorKind::None;
        bool Any() const { return name || label || mobIconCount > 0 || infoIconCount > 0 || cursor != CursorKind::None; }
    };
    PlateLines ChooseLines(const ActorInfo& info, bool selfEngaged, const Settings& settings,
        const CursorTargets& targets, bool iconsFailed);

    // A target with no game name to hang a cursor on, such as a Telepoint, gets its cursor at the game's arrow anchor.
    struct LoneCursor
    {
        uint16_t index;
        CursorKind kind;
        float x, y;
    };
    std::vector<LoneCursor> LoneCursors(const CursorTargets& targets, const Settings& settings,
        const std::vector<uint16_t>& withNameplateCursor);

    struct CursorSpot
    {
        float x, y;
    };
    CursorSpot CursorAtAnchor(float anchorX, float anchorY, float width, float height, float tip);
    // A cursor box moved the least that keeps all of it inside a screen this size.
    CursorSpot KeepOnScreen(CursorSpot spot, float width, float height, float screenWidth, float screenHeight);

    // 0 for a line that is not shown.
    struct LineSizes
    {
        float nameWidth = 0.0f, nameHeight = 0.0f;
        float labelWidth = 0.0f, labelHeight = 0.0f;
        float ornamentWidth = 0.0f, ornamentHeight = 0.0f;
        int mobIconCount = 0;
        float iconSize   = 0.0f;
        int infoIconCount  = 0;
        bool detectionLeft = true, badgesLeft = false;
        IconPlace iconPlace = IconPlace::Edges;
        float cursorWidth  = 0.0f;
        float cursorHeight = 0.0f;
        float cursorTip    = 0.5f; // where across its width the cursor points
        int leftIconCount       = 0;
        int rightIconCount      = 0;
        float playerIconSize    = 0.0f;
        bool centerNameAndIcons = false;
        int timerCount    = 0;
        float timerHeight = 0.0f;
    };

    // How many icons a row of either group holds; the rest wrap onto rows above.
    constexpr int kIconsPerRow = 8;
    // A badge's text height, and how far its box reaches past its icon's right and bottom edges, as shares of the icon size.
    constexpr float kBadgeShare = 0.65f, kBadgeOverhang = 0.35f, kBadgeHang = 0.3f;

    // One group of the icon row: detection icons or the badge group. Both read left to right; a left group starts its
    // inset past the row's left edge, a right one ends its inset short of the row's right edge.
    struct IconRow
    {
        int count  = 0;
        float step = 0.0f, inset = 0.0f;
        bool left  = false;
    };
    struct IconSpot
    {
        float x, y;
    };

    struct NameplateLayout
    {
        float centerX;
        float nameX, nameY;
        float labelX, labelY;
        float ornamentX, ornamentY;
        float iconsY, iconRowStep, iconSize; // iconsY: the top of the row nearest the level line
        float iconsLeftX, iconsRightX;       // the row's edges: the level line's, or wider when its groups need it
        IconRow detection, info;
        float timersY, timerStep;
        CursorSpot cursor;
        float leftIconsX, rightIconsX, playerIconsY, playerIconStep;
    };
    NameplateLayout LayoutNameplate(const ScreenBox& plate, const LineSizes& sizes, bool showName);
    // The i-th icon of a group, wrapping every kIconsPerRow onto the row above.
    IconSpot IconAt(const NameplateLayout& l, const IconRow& row, int i);
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

    // A plate's outline on screen: its name with a player's icons, its level line, ornament and icon rows.
    struct PlateBox
    {
        float left, top, right, bottom;
    };
    PlateBox BoxOf(const NameplateLayout& l, const LineSizes& sizes, bool showName);
    // How far up each plate goes so none covers another: in order, the first keeping its place, each that would cover
    // a plate already placed rises until its bottom is gap pixels over every one it would touch. Plates side by side
    // are left alone.
    std::vector<float> StackRises(const std::vector<PlateBox>& boxes, float gap);
    // A plate's rise seconds later, on its way to where it should be: most of the way in a fraction of a second, and
    // there once within half a pixel.
    float EaseRise(float shown, float target, double seconds);

    float CursorBob(double seconds, float height);

    // Not rounded, so sizes change as smoothly as the game's names.
    float DistanceScale(float letterHeight, float screenHeight);
    // The size DistanceScale gives a game's name this many yalms from the camera, without the game's steps (it sizes
    // its names in eightieths, by its depth from the camera, so they change in jumps and as the camera turns): 1 for
    // a distance that is not a number.
    float ScaleAtDistance(float yalms);
    // The game steps its name size, by 5% at a time far away; a plate eases toward each step. A plate with no size yet
    // (0) takes the size at once.
    float EaseScale(float shown, float target, double seconds);
    // A frame's time for redrawing textures whose look changed, at a size step or a menu change; the rest keep their old
    // texture a frame longer. One with nothing to show yet is always drawn.
    constexpr double kRedrawBudgetMs = 3.0;
    bool RedrawNow(bool drawn, double spentMs);
    // Whether this plate's sizes follow the game's name size; self is your own.
    bool ScalesWithDistance(const Settings& settings, EntityKind kind, bool self);
    // Plates of a higher rank draw over every plate of a lower one: your target's, and over it the one you are picking.
    constexpr int kPlainRank = 0, kTargetRank = 1, kPickedRank = 2;
    int PlateRank(uint16_t index, const CursorTargets& targets);
    // A plate's alpha, out of kOpaqueAlpha: a mob's that is not picked fades to the menu's opacity while you have a
    // target.
    uint32_t PlateAlpha(const Settings& settings, EntityKind kind, int rank, bool hasTarget);
    // Within a rank, farther plates first, so nearer ones cover them.
    bool DrawnBefore(int rankA, float depthA, int rankB, float depthB);
    // The name size the menu sets for this kind of name; self is your own.
    int NameSize(const Settings& settings, EntityKind kind, bool self);
    // Heights come in steps, and current (the height drawn now) is kept until the shown size falls a step below it or
    // rises a little above it, so text is redrawn only when its size really changes and never flickers between two.
    int RasterHeight(float pixels, int current);
}
