#pragma once

#include "Ashita.h"
#include "cursor_file.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // Shows another pointer (the chocobo, or a cursor file's) in place of FFXI's mouse pointer. FFXiMain.dll loads its
    // pointers at startup and sets one about every frame through SetCursor, so its import of SetCursor is redirected
    // here: each pointer the game sets is compared once with mousenor.ani and mousehit.ani (over menus and things to act
    // on) in the game's folder, and those are swapped for the frames shown. The camera arrows pass through.
    class PointerSwap
    {
    public:
        ~PointerSwap() { Stop(); }
        // Shows the frames, or with none the game's own pointers, at a percent of their size. Returns why it could not
        // start, or an empty string; with frames that are empty it does not.
        std::string Start(std::optional<std::vector<CursorFrame>> shown, int percent);
        void Stop();
        bool Running() const { return m_Slot != nullptr; }
        // Called each frame: steps the animation while it shows.
        void Animate();
        HCURSOR OnSetCursor(HCURSOR cursor);

    private:
        // Which of the game's pointer frames the cursor is, if one.
        std::optional<size_t> Replaced(HCURSOR cursor);
        HCURSOR Frame() const;
        bool Ours(HCURSOR cursor) const;
        void Release();

        std::vector<CursorFrame> m_Replaced;        // the game's pointers the frames stand in for, from their files
        std::vector<CursorFrame> m_Shown;
        std::vector<HCURSOR> m_Frames;               // those as Windows cursors
        std::unordered_map<HCURSOR, std::optional<size_t>> m_Known; // which of m_Replaced each pointer the game set is
        bool m_Own = false; // the game's own pointers are shown, each frame in place of itself
        HCURSOR m_GamePointer = nullptr;             // the last of those, to put back
        void** m_Slot        = nullptr;              // FFXiMain.dll's import of SetCursor
        LARGE_INTEGER m_Start{}, m_Frequency{};
    };
}
