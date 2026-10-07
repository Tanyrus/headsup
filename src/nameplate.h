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
    // Frames in a row a name must be drawn before its level line shows, so a one-frame pop never flashes a label.
    constexpr uint32_t kStableFrames = 2;
    // Some frames credit an entity's body draws to whoever else is on the stack, which made a level line blink.
    constexpr uint32_t kMeshGraceFrames = 8;
    bool Steady(uint32_t framesSinceMesh, uint32_t nameFramesInRow);

    // Some of a name is enough: a replaced name must show whenever the game's would, as the game's is hidden.
    bool NameOnScreen(const ScreenBox& plate, float screenWidth, float screenHeight);

    // The game centers a name's letters and the icons beside them together over the entity, and a posed player's name
    // comes down to their head.
    ScreenBox PlaceName(const ScreenBox& letters, const ScreenBox* whole, const Camera* camera, const WorldPoint& feet, Pose pose);

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
        int leftIconCount  = 0;
        int rightIconCount = 0;
        CursorKind cursor  = CursorKind::None;
        bool Any() const { return name || label || mobIconCount > 0 || cursor != CursorKind::None; }
    };
    PlateLines ChooseLines(const ActorInfo& info, bool steady, bool selfEngaged, const Settings& settings,
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

    // 0 for a line that is not shown.
    struct LineSizes
    {
        float nameWidth = 0.0f, nameHeight = 0.0f;
        float labelWidth = 0.0f, labelHeight = 0.0f;
        int mobIconCount = 0;
        float iconSize   = 0.0f;
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

    struct NameplateLayout
    {
        float centerX;
        float nameX, nameY;
        float labelX, labelY;
        float iconsX, iconsY, iconStep;
        float timersY, timerStep;
        CursorSpot cursor;
        float leftIconsX, rightIconsX, playerIconsY, playerIconStep;
    };
    NameplateLayout LayoutNameplate(const ScreenBox& plate, const LineSizes& sizes, bool showName);

    float CursorBob(double seconds, float height);

    // Not rounded, so sizes change as smoothly as the game's names.
    float DistanceScale(float letterHeight, float screenHeight);
    // Heights come in steps, and current (the height drawn now) is kept until the shown size falls a step below it or
    // rises a little above it, so text is redrawn only when its size really changes and never flickers between two.
    int RasterHeight(float pixels, int current);
}
