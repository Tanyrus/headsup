#pragma once

#include "native_locate.h"
#include "process_memory.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // A window of the game's own list, as its draw loop will draw it: its record (MCD), key, rectangle in the game's
    // menu resolution, and whether it is hidden (a parent under the list it opened, or one closing).
    struct OpenWindow
    {
        uint32_t mcd = 0;
        WindowKey key{};
        int left = 0, top = 0, right = 0, bottom = 0;
        bool hidden = false;
    };

    // The list the game's window draw loop walks (the object at list: its first dword the first link; a link's +0 the
    // next, +0x10 its window's record, +0x14 a byte set to skip it), in its order: at most out.size() windows and
    // kMaxListLinks links, stopping at anything memory says cannot be read. Returns how many it wrote.
    constexpr size_t kMaxListLinks = 256;
    size_t ReadWindowList(uint32_t list, const ProcessMemory& memory, std::span<OpenWindow> out);

    // A view: the windows the game opens together, as it placed them with HeadsUp unloaded. top is the highest of
    // them; windows is every shown window that hangs from the top, the strip's left out.
    struct WindowView
    {
        std::string_view id, name;
        int top = 0;
        std::span<const WindowKey> windows;
    };
    // A window of the frame: where the game put it, and whether it is hidden.
    struct FrameWindow
    {
        WindowKey key{};
        int left = 0, top = 0;
        bool hidden = false;
    };
    // Whether a window the game put at top hangs from the screen's top (above its middle), and so goes with its view;
    // one lower is placed from the bottom, as the chat log and what sits on it.
    bool HangsFromTop(int top, int screenBottom);
    // The view holding most of the frame's shown windows, the one with fewer windows on a tie; none holding any.
    std::optional<size_t> ViewOf(std::span<const WindowView> views, std::span<const FrameWindow> frame);
    // The game's top row, the strip's: a window it puts higher, as the map's 106 x 14 corner frame at 0, draws nothing.
    constexpr int kGameTopRow = 16;
    // The highest top among the frame's shown windows that hang from the top (above the screen's middle), the game's
    // strip and what sits above its row left out; none without such a window.
    std::optional<int> FrameTop(std::span<const FrameWindow> frame, int screenBottom);
    // How far every window of the frame moves down (up when negative): the player's shift for its view, else what
    // brings its top to placedUnder; keepClear then pushes it down to clear band, and never up.
    int FrameShift(int frameTop, std::optional<int> chosen, int placedUnder, int band, bool keepClear);
    // The game anchors a window starting past the middle of its menu to the right: the command menus are there.
    constexpr bool OnLeftSide(int windowLeft, int screenRight) { return windowLeft < screenRight / 2; }
    struct FrameShifts
    {
        int left = 0, right = 0;
        int For(int windowLeft, int screenRight) const { return OnLeftSide(windowLeft, screenRight) ? left : right; }
        bool operator==(const FrameShifts&) const = default;
    };
    // FrameShift for each side of the screen: the side holding the frame's highest window takes the view's shift, the
    // other what brings its own top to placedUnder, so the command menus on the right stay put when Status opens.
    FrameShifts ShiftsOf(std::span<const FrameWindow> frame, std::optional<int> chosen, int placedUnder, int band,
        bool keepClear, int screenRight, int screenBottom);

    // Whether the game's help shows under the panel: the panel's help line, or the game's own strip, moved under it
    // or where the game put it. Each has its own place for the windows and its own shifts.
    enum class HelpMode
    {
        Off,
        On,
    };
    constexpr size_t kHelpModes = 2;
    HelpMode ModeOf(bool panelHelp, bool hideGameHelp);
    // The views of the game's windows HeadsUp knows, measured from the client (data/window_views.json).
    std::span<const WindowView> WindowViews();
    // The band's bottom, in the game's menu pixels, the views were placed under: the top panel at its defaults.
    int ReferenceBand(HelpMode mode);

    // The player's own shift for a view, by its id.
    struct ViewShift
    {
        std::string id;
        int dy = 0;
        bool operator==(const ViewShift&) const = default;
    };
    // From the settings file, "status:20|items:-6": a view no view of views has goes, and so does one named again; a
    // shift is held to kMaxWindowOffset either way.
    std::vector<ViewShift> ViewShiftsFrom(std::string_view text, std::span<const WindowView> views);
    std::string ViewShiftsText(const std::vector<ViewShift>& shifts);

    // The game's strip (its help and title windows) where the game put it.
    struct StripRows
    {
        int top = 0, bottom = 0;
    };
    // How far the strip moves, and the line the other windows go under.
    struct StripPlace
    {
        int shift = 0, band = 0;
        bool operator==(const StripPlace&) const = default;
    };
    // From the panel's band (0 while it is off) and the strip when it shows: moved, it goes right under the band and
    // the windows under it; left, the windows go under whichever reaches lower, clearance below the strip.
    StripPlace PlaceStrip(int panelBand, std::optional<StripRows> strip, bool moveGameHelp, int clearance);

    // The strip title window's rectangle as Wide Scan's grid label reads it (the game draws the label 3 below its
    // bottom, over the list): moved by the frame's shift less the title's own move, so the label goes with the list.
    void LabelPlace(std::span<int16_t, 4> rect, int frameShift, int titleMoved);
}
