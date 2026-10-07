#pragma once

#include "screen_box.h"

#include <cmath>

namespace test
{
    inline headsup::ScreenBox Box(float x0, float y0, float x1, float y1)
    {
        headsup::ScreenBox b;
        b.Add(x0, y0);
        b.Add(x1, y1);
        return b;
    }

    inline bool Near(float a, float b, float tolerance = 1e-3f) { return std::fabs(a - b) < tolerance; }
}
