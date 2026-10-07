#pragma once

#include "argb.h"
#include "game_cursor.h"
#include "screen_box.h"
#include "settings.h"

#include <cstdint>
#include <vector>

namespace headsup
{
    // Frames in a row a name must be drawn before its level line shows, so a one-frame pop never flashes a label.
    constexpr uint32_t kStableFrames = 2;
    // Frames an entity's body counts as drawn after the game last drew it: some frames credit its draws to whoever else
    // is on the stack, which made a level line blink.
    constexpr uint32_t kMeshGraceFrames = 8;
    // Whether the camera sees an entity well enough for its level line and MobDB icons: the game drew its body within
    // kMeshGraceFrames frames and its name for kStableFrames frames in a row.
    bool Steady(uint32_t framesSinceMesh, uint32_t nameFramesInRow);

    // Whether some of a name is on screen. A replaced name shows whenever the game's would, as the game's is hidden.
    bool NameOnScreen(const ScreenBox& plate, float screenWidth, float screenHeight);

    // Which lines an entity's nameplate shows this frame, and which cursor.
    enum class CursorKind : uint8_t
    {
        None,
        Target,
        Locked,    // the target, while locked on
        SubTarget, // the candidate while picking a sub-target
        OutOfRange, // that candidate, out of range of the spell or ability
    };
    struct PlateLines
    {
        bool name      = false;
        bool label     = false;
        int icons      = 0; // MobDB icons
        int leftIcons  = 0; // a player's, either side of the name
        int rightIcons = 0;
        CursorKind cursor = CursorKind::None;
        bool Any() const { return name || label || icons > 0 || cursor != CursorKind::None; }
    };
    struct PlateFacts
    {
        uint16_t index;
        bool replaced; // ReplacesName
        bool steady;   // Steady
        bool alive;
        bool hasLabel;
        int mobIcons, leftIcons, rightIcons; // a player's icons either side of the name
        bool fighting;                       // a mob claimed by you or your party
        bool claimed;                        // a mob anyone has claimed
        bool tooWeak;                        // a mob that cons Too Weak to you
        bool engaged;                        // you are fighting
    };
    // The name when it is replaced, with a player's icons beside it; the level line and MobDB icons of a living mob the
    // camera sees steadily, unless the settings hide them for it (in combat with you, claimed, Too Weak, or while you
    // fight); and the cursor over the target or the sub-target candidate. No icons once their textures have failed.
    PlateLines ChooseLines(const PlateFacts& facts, const Settings& settings, const CursorTargets& targets, bool iconsFailed);

    // The cursor over an entity: the candidate being picked (in or out of range), else the target (locked on or not).
    CursorKind CursorFor(uint16_t index, const Settings& settings, const CursorTargets& targets);

    // A cursor over a target with no game name to hang it on, such as a Telepoint, at the game's arrow anchor.
    struct LoneCursor
    {
        uint16_t index;
        CursorKind kind;
        float x, y; // the anchor on screen
    };
    // The candidate's at the sub anchor while picking, then the target's at the main one, for each without a cursor on
    // its nameplate (withCursor).
    std::vector<LoneCursor> LoneCursors(const CursorTargets& targets, const Settings& settings, const std::vector<uint16_t>& withCursor);

    // The top-left corner of a cursor width by height whose point, tip across its width, sits on the anchor.
    struct CursorSpot
    {
        float x, y;
    };
    CursorSpot CursorAtAnchor(float anchorX, float anchorY, float width, float height, float tip);

    // Measured sizes of a nameplate's lines (0 for a line that is not shown).
    struct LineSizes
    {
        float nameWidth = 0.0f, nameHeight = 0.0f;
        float labelWidth = 0.0f, labelHeight = 0.0f;
        int iconCount  = 0;
        float iconSize = 0.0f;
        float cursorWidth  = 0.0f; // 0 without a target cursor
        float cursorHeight = 0.0f;
        float cursorTip    = 0.5f; // where across its width the cursor points, placed over the center
        int leftIconCount       = 0; // a player's icons either side of the name
        int rightIconCount      = 0;
        float nameIconSize      = 0.0f;
        bool centerNameAndIcons = false; // the name and its icons centered together, rather than the name alone
        int timerCount    = 0; // your placeholder timers' lines
        float timerHeight = 0.0f;
    };

    // Top-left corners of each line, centered on the nameplate. With a name, the name is centered on the game's, and the
    // line above it overlaps its box by 3 px, the font's own space above the letters. Without one, the first line sits
    // 2 px above the game's name. The label, the icon row, your timers and the cursor then each stack 2 px above the
    // line below. Icons are iconStep apart, and timers timerStep, from the top one at timersY, each centered on centerX.
    // A player's icons sit in rows against the name's left and right edges, centered on it.
    struct NameplateLayout
    {
        float centerX;
        float nameX, nameY;
        float labelX, labelY;
        float iconsX, iconsY, iconStep;
        float timersY, timerStep;
        float cursorX, cursorY;
        float leftIconsX, rightIconsX, nameIconsY, nameIconStep;
    };
    NameplateLayout LayoutNameplate(const ScreenBox& plate, const LineSizes& sizes, bool showName);

    // The letters' box moved sideways so it is centered where the whole nameplate is: the game centers a name and the
    // icons beside it together over the entity, so this is where the entity is.
    ScreenBox CenteredOver(const ScreenBox& letters, const ScreenBox& whole);

    // How far up our target cursor floats at this time, in pixels: a slow bob of up to 15% of its height.
    float CursorBob(double seconds, float height);

    // How much to scale text and icons to follow the game's names: letterHeight / (screenHeight / 180), the typical
    // letter, clamped to 0.5x-2.5x. Not rounded, so sizes change smoothly.
    float DistanceScale(float letterHeight, float screenHeight);
    // The pixel height to draw text at when it is shown pixels tall: a size from 6 px in steps of 1.25x, which is then
    // scaled to the shown size. current, the size it is drawn at now, is kept while the shown size is no more than a
    // step below it or 5% above it, so text is redrawn only when its size has really changed.
    int RasterHeight(float pixels, int current);
}
