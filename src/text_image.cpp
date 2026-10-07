#include "text_image.h"

#include <algorithm>
#include <cmath>

namespace headsup
{
    Image OutlinedText(const Coverage& text, int radius, uint32_t color, uint32_t outlineColor)
    {
        Image image{text.width, text.height, std::vector<uint32_t>(text.alpha.size(), 0)};
        auto coverage = [&](int x, int y) { return text.alpha[static_cast<size_t>(y * text.width + x)]; };
        auto channel  = [](uint32_t argb, int shift) { return static_cast<float>((argb >> shift) & 0xFF); };
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
                uint32_t argb = static_cast<uint32_t>(std::lround(alpha * 255.0f)) << 24;
                for (const int shift : {16, 8, 0})
                {
                    const float mixed = (channel(color, shift) * a + channel(outlineColor, shift) * o * (1.0f - a)) / alpha;
                    argb |= static_cast<uint32_t>(std::lround(mixed)) << shift;
                }
                image.argb[static_cast<size_t>(y * text.width + x)] = argb;
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
