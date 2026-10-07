#include "text_image.h"

#include "argb.h"

#include <algorithm>
#include <cmath>

namespace headsup
{
    Image Outlined(const Coverage& text, int radius, uint32_t color, uint32_t outlineColor)
    {
        Image image{text.width, text.height, std::vector<uint32_t>(text.alpha.size(), 0)};
        auto coverage = [&](int x, int y) { return text.alpha[static_cast<size_t>(y * text.width + x)]; };
        for (int y = 0; y < text.height; ++y)
        {
            for (int x = 0; x < text.width; ++x)
            {
                uint8_t grown = 0;
                for (int dy = -radius; radius > 0 && dy <= radius; ++dy)
                {
                    for (int dx = -radius; dx <= radius; ++dx)
                    {
                        const int nx = x + dx, ny = y + dy;
                        if (dx * dx + dy * dy > radius * radius + radius) continue; // a disc, with its diagonals at 1
                        if (nx >= 0 && ny >= 0 && nx < text.width && ny < text.height) grown = std::max(grown, coverage(nx, ny));
                    }
                }
                const float a     = static_cast<float>(coverage(x, y)) / 255.0f;
                const float o     = static_cast<float>(grown) / 255.0f;
                const float alpha = a + o * (1.0f - a);
                if (alpha <= 0.0f) continue;
                auto mix = [&](int shift) {
                    const float c = static_cast<float>(Channel(color, shift)), oc = static_cast<float>(Channel(outlineColor, shift));
                    return static_cast<uint8_t>(std::lround((c * a + oc * o * (1.0f - a)) / alpha));
                };
                image.argb[static_cast<size_t>(y * text.width + x)] =
                    Argb(static_cast<uint8_t>(std::lround(alpha * 255.0f)), mix(kRedShift), mix(kGreenShift), mix(kBlueShift));
            }
        }
        return image;
    }

    int TextureSide(int pixels)
    {
        int side = 1;
        while (side < pixels)
            side *= 2;
        return side;
    }
}
