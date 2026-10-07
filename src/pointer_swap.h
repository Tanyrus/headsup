#pragma once

#include "Ashita.h"
#include "cursor_file.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // Shows HeadsUp's chocobo in place of FFXI's mouse pointer. FFXiMain.dll loads its pointers at startup and sets one
    // about every frame through SetCursor, so its import of SetCursor is redirected here: each pointer the game sets is
    // compared once with mousenor.ani and mousehit.ani (over menus and things to act on) in the game's folder, and those
    // are swapped for the chocobo. The camera arrows pass through.
    class PointerSwap
    {
    public:
        ~PointerSwap() { Stop(); }
        // Returns why it could not start, or an empty string.
        std::string Start();
        void Stop();
        bool Running() const { return m_Slot != nullptr; }
        // Called each frame: steps the chocobo's animation while it shows.
        void Animate();
        HCURSOR OnSetCursor(HCURSOR cursor);

    private:
        bool Replaced(HCURSOR cursor);
        HCURSOR Frame() const;
        bool Ours(HCURSOR cursor) const;
        void Release();

        std::vector<CursorFrame> m_Replaced;        // the game's pointers the chocobo stands in for, from their files
        std::vector<CursorFrame> m_Chocobo;
        std::vector<HCURSOR> m_Frames;               // the chocobo's frames as Windows cursors
        std::unordered_map<HCURSOR, bool> m_Known;   // whether each pointer the game set is one the chocobo stands in for
        HCURSOR m_GamePointer = nullptr;             // the last of those, to put back
        void** m_Slot        = nullptr;              // FFXiMain.dll's import of SetCursor
        LARGE_INTEGER m_Start{}, m_Frequency{};
    };
}
