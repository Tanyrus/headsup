#pragma once

#include "screen_box.h"

#include <cstdint>
#include <span>

namespace headsup
{
    // The name routine's stack frame at the hook (esp there): below its sub esp,0x6C4 and four pushes, so the caller's
    // return address and the routine's five arguments sit at its top.
    constexpr uint32_t kNameFrameBytes = 0x6EC;
    constexpr uint32_t kMaxNameBytes   = 36;

    struct NameFrame
    {
        uint32_t caller = 0; // the return address
        uint32_t actor  = 0;
        uint32_t length = 0; // bytes of text
        float x = 0.0f, y = 0.0f, depth = 0.0f, scaleX = 0.0f, scaleY = 0.0f;
        uint32_t text  = 0; // where the game keeps the name's bytes
        uint32_t color = 0, shellColor = 0;
    };
    NameFrame ReadNameFrame(const uint8_t* frame);

    // The game's name routine at work, called from where it always is with a name of a sensible length: only then may
    // anything the frame points at be read.
    bool FromNameRoutine(const NameFrame& frame, std::span<const uint32_t> callerReturns);
    // A size and a place to draw at.
    bool Placeable(const NameFrame& frame);

    // A name as the game was about to draw it, in back-buffer pixels. The box spans the letters from their top down
    // their height, and has no width: the frame's x is the center of the whole name, the game's icons included.
    struct DrawnName
    {
        ScreenBox box;
        float depth    = 0.0f;
        uint32_t color = 0; // as drawn, opaque
    };
    // toBackBuffer: back-buffer pixels per pixel of the image the game draws names into.
    DrawnName NameFromFrame(const NameFrame& frame, float toBackBufferX, float toBackBufferY);

    // The game lays out a name with a line break itself, so HeadsUp leaves those to it.
    bool SingleLine(const uint8_t* text, uint32_t length);
}
