#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace headsup
{
    // A cursor's picture: BGRA rows top to bottom, alpha 0 where it is transparent, and its hotspot, the pixel that
    // points.
    struct CursorImage
    {
        uint32_t width = 0, height = 0;
        uint16_t hotX = 0, hotY = 0;
        std::vector<uint8_t> bgra;
    };

    struct CursorFrame
    {
        CursorImage image;
        std::vector<uint8_t> resource; // the hotspot, then the image as the file keeps it: what CreateIconFromResourceEx takes
        double seconds = 0.0;          // how long it shows; 0 for a cursor that does not animate
    };

    // The frames of a cursor file (.cur, one frame) or an animated cursor (.ani); empty for anything else. Frames are 1,
    // 4, 8, 24 or 32 bits per pixel.
    std::vector<CursorFrame> ReadCursorFile(const uint8_t* data, size_t size);

    // The frame showing `seconds` after the animation started, round and round.
    size_t FrameAt(const std::vector<CursorFrame>& frames, double seconds);

    // A cursor's picture from its color and mask bitmaps as GetDIBits gives them at 32 bits, top to bottom: the mask is
    // white where the cursor is transparent.
    CursorImage ImageFromBitmaps(uint32_t width, uint32_t height, uint16_t hotX, uint16_t hotY, const std::vector<uint8_t>& color,
        const std::vector<uint8_t>& mask);

    // The same size and hotspot, transparent in the same places, and the same colors everywhere else.
    bool SameCursor(const CursorImage& a, const CursorImage& b);

    // HeadsUp's chocobo mouse pointer, a player's rebuild of the PlayOnline Viewer's (third_party/playonline-chocobo): the
    // file, and its frames.
    std::vector<uint8_t> ChocoboPointerFile();
    std::vector<CursorFrame> ChocoboPointer();
}
