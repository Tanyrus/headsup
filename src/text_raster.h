#pragma once

#include "text_image.h"

namespace headsup
{
    // Draws text with GDI's grayscale antialiasing at a character height of pixelHeight, inside a margin of empty pixels
    // on every side. False when GDI cannot create the font or the bitmap.
    bool RasterizeText(const char* text, const char* family, int pixelHeight, bool bold, int margin, Coverage& out);
}
