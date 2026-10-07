#include "shapes.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

namespace headsup
{
    namespace
    {
#include "generated/shapes.inc"

        const ShapePoint kArrowPoints[] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f}};
        constexpr float kArrowAspect    = 0.8f;
        constexpr int kRowSamples       = 4; // per pixel row; columns are covered exactly
    }

    const Shape& ArrowShape()
    {
        static const Shape arrow{kArrowPoints, std::size(kArrowPoints), kArrowAspect};
        return arrow;
    }

    const Shape& FeatherShape()
    {
        static const Shape feather{kFeatherPoints, std::size(kFeatherPoints), kFeatherAspect};
        return feather;
    }

    float ShapeTip(const Shape& shape)
    {
        constexpr float kNearTheBottom = 0.01f; // a point this close to the lowest one is part of the tip
        float lowest = 0.0f;
        for (size_t i = 0; i < shape.count; ++i)
            lowest = std::max(lowest, shape.points[i].y);
        float sum = 0.0f;
        int count = 0;
        for (size_t i = 0; i < shape.count; ++i)
        {
            if (shape.points[i].y < lowest - kNearTheBottom) continue;
            sum += shape.points[i].x;
            ++count;
        }
        return count > 0 ? sum / static_cast<float>(count) : 0.5f;
    }

    Coverage ShapeCoverage(const Shape& shape, int pixelHeight, int margin)
    {
        const int height = std::max(1, pixelHeight);
        const int width  = std::max(1, static_cast<int>(std::lround(static_cast<float>(height) / shape.aspect)));
        std::vector<float> covered(static_cast<size_t>(width) * static_cast<size_t>(height), 0.0f);
        std::vector<std::pair<float, int>> crossings; // x in pixels, and the edge's winding direction
        for (int y = 0; y < height; ++y)
        {
            for (int row = 0; row < kRowSamples; ++row)
            {
                const float sampleY = (static_cast<float>(y) + (static_cast<float>(row) + 0.5f) / kRowSamples) / static_cast<float>(height);
                crossings.clear();
                for (size_t i = 0; i < shape.count; ++i)
                {
                    const ShapePoint& a = shape.points[i];
                    const ShapePoint& b = shape.points[(i + 1) % shape.count];
                    if ((a.y <= sampleY) == (b.y <= sampleY)) continue;
                    const float t = (sampleY - a.y) / (b.y - a.y);
                    crossings.emplace_back((a.x + t * (b.x - a.x)) * static_cast<float>(width), b.y > a.y ? 1 : -1);
                }
                std::sort(crossings.begin(), crossings.end());
                int winding = 0; // nonzero fill
                for (size_t k = 0; k + 1 < crossings.size(); ++k)
                {
                    winding += crossings[k].second;
                    if (winding == 0) continue;
                    const float from = std::clamp(crossings[k].first, 0.0f, static_cast<float>(width));
                    const float to   = std::clamp(crossings[k + 1].first, 0.0f, static_cast<float>(width));
                    for (int x = static_cast<int>(from); x < width && static_cast<float>(x) < to; ++x)
                    {
                        const float overlap = std::min(to, static_cast<float>(x + 1)) - std::max(from, static_cast<float>(x));
                        covered[static_cast<size_t>(y * width + x)] += overlap / kRowSamples;
                    }
                }
            }
        }
        Coverage c{width + 2 * margin, height + 2 * margin, {}};
        c.alpha.assign(static_cast<size_t>(c.width) * static_cast<size_t>(c.height), 0);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                c.alpha[static_cast<size_t>((y + margin) * c.width + x + margin)] =
                    static_cast<uint8_t>(std::lround(std::min(covered[static_cast<size_t>(y * width + x)], 1.0f) * 255.0f));
        return c;
    }
}
