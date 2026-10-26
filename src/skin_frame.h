#pragma once

#include <cstdint>
#include <string>

namespace headsup
{
    // A box drawn as the game draws its windows, in the art of a Window Type (window_skin.h): its pieces at the game's
    // menu pixels times scale, the background shaded from shadeTop at its top to shadeBottom at its foot.
    struct SkinFrame
    {
        int type    = 0;
        float scale = 1.0f;
        float shadeTop = 1.0f, shadeBottom = 1.0f;
        uint32_t edge = 0; // the even border's color, the frame's; 0 for none
        bool operator==(const SkinFrame&) const = default;
    };
    constexpr float kSkinEdge = 2.0f; // pixels: the border HeadsUp draws in the frame's color
    // The RCSS that draws it: the shade over the background repeated, in an even border of kSkinEdge all round. The
    // art's own frame is not drawn: its edges are a line thicker in the middle.
    std::string SkinRules(const SkinFrame& frame);
}
