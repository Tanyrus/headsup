#pragma once

#include <algorithm>

namespace headsup
{
    // An axis-aligned screen rectangle in pixels.
    struct ScreenBox
    {
        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        bool valid = false;

        void Add(float x, float y)
        {
            if (!valid)
            {
                minX = maxX = x;
                minY = maxY = y;
                valid       = true;
                return;
            }
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }

        void Add(const ScreenBox& other)
        {
            if (!other.valid) return;
            Add(other.minX, other.minY);
            Add(other.maxX, other.maxY);
        }

        ScreenBox Scaled(float scaleX, float scaleY) const
        {
            ScreenBox r = *this;
            r.minX *= scaleX;
            r.maxX *= scaleX;
            r.minY *= scaleY;
            r.maxY *= scaleY;
            return r;
        }

        float Width() const { return maxX - minX; }
        float Height() const { return maxY - minY; }
        float CenterX() const { return (minX + maxX) * 0.5f; }
        float CenterY() const { return (minY + maxY) * 0.5f; }
    };

    // Below this, a box's height counts as this: a degenerate letter would otherwise scale its padding to nothing.
    constexpr float kMinLetterHeight = 1.0f;

    inline bool Inside(float x, float y, const ScreenBox& box, float pad)
    {
        return box.valid && x >= box.minX - pad && x <= box.maxX + pad && y >= box.minY - pad && y <= box.maxY + pad;
    }
}
