#pragma once

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
        bool operator==(const SkinFrame&) const = default;
    };
    // The RCSS decorator that draws it: the frame on top, the shade under it, the background repeated under that.
    std::string SkinDecorator(const SkinFrame& frame);
    // The same without the frame, as the game draws its strips across the screen (the help bar).
    std::string SkinFillDecorator(const SkinFrame& frame);
}
