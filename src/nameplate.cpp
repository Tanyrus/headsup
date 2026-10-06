#include "nameplate.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace aggroglow
{
    void ScreenBox::Add(float x, float y)
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

    void ScreenBox::Add(const ScreenBox& other)
    {
        if (!other.valid) return;
        Add(other.minX, other.minY);
        Add(other.maxX, other.maxY);
    }

    ScreenBox ScreenBox::Scaled(float scaleX, float scaleY) const
    {
        ScreenBox r = *this;
        r.minX *= scaleX;
        r.maxX *= scaleX;
        r.minY *= scaleY;
        r.maxY *= scaleY;
        return r;
    }

    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount)
    {
        switch (primitiveType)
        {
            case 1: return primitiveCount;     // point list
            case 2: return primitiveCount * 2; // line list
            case 3: return primitiveCount + 1; // line strip
            case 4: return primitiveCount * 3; // triangle list
            case 5:                            // triangle strip
            case 6: return primitiveCount + 2; // triangle fan
            default: return 0;
        }
    }

    bool WorldTextBox(const void* vertices, uint32_t stride, uint32_t count, ScreenBox& box)
    {
        box = ScreenBox{};
        if (vertices == nullptr || stride < 16 || count == 0 || count > kMaxTextVertices) return false;
        const auto* bytes = static_cast<const uint8_t*>(vertices);
        for (uint32_t i = 0; i < count; ++i)
        {
            float xyz[3];
            std::memcpy(xyz, bytes + static_cast<size_t>(i) * stride, sizeof(xyz));
            if (!(xyz[2] > 0.0f && xyz[2] < 1.0f) || !std::isfinite(xyz[0]) || !std::isfinite(xyz[1]))
            {
                box = ScreenBox{};
                return false;
            }
            box.Add(xyz[0], xyz[1]);
        }
        return true;
    }

    ScreenBox NameplateFromGlyphs(std::vector<ScreenBox> glyphs)
    {
        glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(),
                         [](const ScreenBox& g) {
                             return !g.valid || g.maxX - g.minX > kMaxGlyphSize || g.maxY - g.minY > kMaxGlyphSize;
                         }),
            glyphs.end());
        if (glyphs.size() < kMinGlyphs) return ScreenBox{};

        // The main text line: the median glyph's vertical center and height.
        std::vector<float> centers, heights;
        for (const ScreenBox& g : glyphs)
        {
            centers.push_back((g.minY + g.maxY) * 0.5f);
            heights.push_back(g.maxY - g.minY);
        }
        std::nth_element(centers.begin(), centers.begin() + centers.size() / 2, centers.end());
        std::nth_element(heights.begin(), heights.begin() + heights.size() / 2, heights.end());
        const float lineCenter = centers[centers.size() / 2];
        const float height     = std::max(heights[heights.size() / 2], 1.0f);
        glyphs.erase(std::remove_if(glyphs.begin(), glyphs.end(),
                         [&](const ScreenBox& g) { return std::fabs((g.minY + g.maxY) * 0.5f - lineCenter) > height * 0.5f; }),
            glyphs.end());

        // Runs of glyphs left to right, broken by gaps wider than two glyph heights; the largest run is the name.
        std::sort(glyphs.begin(), glyphs.end(), [](const ScreenBox& a, const ScreenBox& b) { return a.minX < b.minX; });
        ScreenBox best, run;
        size_t bestCount = 0, runCount = 0;
        for (const ScreenBox& g : glyphs)
        {
            if (run.valid && g.minX - run.maxX > 2.0f * height)
            {
                if (runCount > bestCount) best = run, bestCount = runCount;
                run      = ScreenBox{};
                runCount = 0;
            }
            run.Add(g);
            ++runCount;
        }
        if (runCount > bestCount) best = run, bestCount = runCount;
        return bestCount >= kMinGlyphs ? best : ScreenBox{};
    }

    bool LabelVisible(const ScreenBox* plate, uint32_t meshDraws, uint32_t plateFramesInRow, float screenWidth, float screenHeight)
    {
        if (plate == nullptr || !plate->valid || meshDraws == 0 || plateFramesInRow < kStableFrames) return false;
        return plate->minX >= 0.0f && plate->minY >= 0.0f && plate->maxX <= screenWidth && plate->maxY <= screenHeight;
    }

    void PlaceAbove(const ScreenBox& box, float width, float height, float gap, float& x, float& y)
    {
        x = (box.minX + box.maxX) * 0.5f - width * 0.5f;
        y = box.minY - gap - height;
    }
}
