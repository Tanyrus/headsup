#pragma once

#include "native_locate.h"
#include "process_memory.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

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
        int top = 0;
        bool hidden = false;
    };
    // The view holding most of the frame's shown windows, the one with fewer windows on a tie; none holding any.
    std::optional<size_t> ViewOf(std::span<const WindowView> views, std::span<const FrameWindow> frame);
    // The highest top among the frame's shown windows that hang from the top (above the screen's middle), the game's
    // strip left out; none without such a window.
    std::optional<int> FrameTop(std::span<const FrameWindow> frame, int screenBottom);
    // How far every window of the frame moves down (up when negative): the player's shift for its view, else what
    // brings its top to placedUnder; keepClear then pushes it down to clear band, and never up.
    int FrameShift(int frameTop, std::optional<int> chosen, int placedUnder, int band, bool keepClear);
}
