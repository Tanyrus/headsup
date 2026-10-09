#include "plate_image.h"

#include "argb.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace headsup
{
    namespace
    {
#include "generated/shapes.inc"

        const ShapePoint kArrowPoints[] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.5f, 1.0f}};
        constexpr float kArrowAspect    = 0.8f;
        constexpr int kRowSamples       = 4; // per pixel row; columns are covered exactly
        constexpr uint8_t kFullAlpha    = 0xFF;
        constexpr float kFullCoverage   = static_cast<float>(kFullAlpha);
        constexpr float kSigmaPerBlur   = 0.5f; // a CSS blur radius is two standard deviations
        constexpr float kTapsPerSigma   = 3.0f;
        constexpr float kGlowOpacity    = 0.6f; // the mockup's glow color is at 60%
        // The ornament's line, from the mockup's CSS gradient: clear at the ends, the full color 30% of the way in, and
        // lighter from there to the center, where it is Lighter's half way to white.
        constexpr float kLineFadeIn     = 0.3f;
        constexpr float kLineBrightSpan = 0.2f; // either side of the center
        constexpr float kLineWhitest    = 0.5f;
        constexpr float kLinePerHeight  = 1.0f / 9.0f; // a 1 px line in a 9 px ornament

        // Color built up back to front, premultiplied by alpha.
        struct Paint
        {
            float alpha = 0.0f, red = 0.0f, green = 0.0f, blue = 0.0f;

            void Over(float cover, uint32_t color)
            {
                if (cover <= 0.0f) return;
                red   = static_cast<float>(Channel(color, kRedShift)) * cover + red * (1.0f - cover);
                green = static_cast<float>(Channel(color, kGreenShift)) * cover + green * (1.0f - cover);
                blue  = static_cast<float>(Channel(color, kBlueShift)) * cover + blue * (1.0f - cover);
                alpha = cover + alpha * (1.0f - cover);
            }

            uint32_t Color() const
            {
                if (alpha <= 0.0f) return 0;
                auto channel = [&](float value) {
                    return static_cast<uint8_t>(std::lround(std::clamp(value / alpha, 0.0f, kFullCoverage)));
                };
                return Argb(static_cast<uint8_t>(std::lround(alpha * kFullCoverage)), channel(red), channel(green), channel(blue));
            }
        };

        float Cover(const Coverage& c, size_t i) { return static_cast<float>(c.alpha[i]) / kFullCoverage; }

        // Each pixel's coverage, from 0 to 1, blurred by a Gaussian in two passes.
        std::vector<float> Blurred(const Coverage& c, float blur)
        {
            const float sigma = blur * kSigmaPerBlur;
            const int taps    = static_cast<int>(std::ceil(sigma * kTapsPerSigma));
            std::vector<float> kernel(static_cast<size_t>(2 * taps + 1));
            float sum = 0.0f;
            for (int i = -taps; i <= taps; ++i)
                sum += kernel[static_cast<size_t>(i + taps)] = std::exp(-0.5f * static_cast<float>(i * i) / (sigma * sigma));
            for (float& k : kernel)
                k /= sum;
            std::vector<float> across(c.alpha.size(), 0.0f), plane(c.alpha.size(), 0.0f);
            for (int y = 0; y < c.height; ++y)
                for (int x = 0; x < c.width; ++x)
                    for (int i = std::max(-taps, -x); i <= std::min(taps, c.width - 1 - x); ++i)
                        across[static_cast<size_t>(y * c.width + x)] +=
                            Cover(c, static_cast<size_t>(y * c.width + x + i)) * kernel[static_cast<size_t>(i + taps)];
            for (int y = 0; y < c.height; ++y)
                for (int x = 0; x < c.width; ++x)
                    for (int i = std::max(-taps, -y); i <= std::min(taps, c.height - 1 - y); ++i)
                        plane[static_cast<size_t>(y * c.width + x)] +=
                            across[static_cast<size_t>((y + i) * c.width + x)] * kernel[static_cast<size_t>(i + taps)];
            return plane;
        }

        uint32_t Whiter(uint32_t argb, float share)
        {
            auto channel = [&](int shift) {
                const float c = Channel(argb, shift);
                return static_cast<uint8_t>(std::lround(c + (kFullCoverage - c) * share));
            };
            return Opaque(channel(kRedShift), channel(kGreenShift), channel(kBlueShift));
        }
    }

    int BlurMargin(float blur)
    {
        return static_cast<int>(std::ceil(blur));
    }

    Image Styled(const Coverage& coverage, const Layers& layers)
    {
        Image image{coverage.width, coverage.height, std::vector<uint32_t>(coverage.alpha.size(), 0)};
        const std::vector<float> glow   = layers.glowBlur > 0.0f ? Blurred(coverage, layers.glowBlur) : std::vector<float>{};
        const std::vector<float> shadow = layers.shadowBlur > 0.0f ? Blurred(coverage, layers.shadowBlur) : std::vector<float>{};
        for (int y = 0; y < coverage.height; ++y)
        {
            for (int x = 0; x < coverage.width; ++x)
            {
                const auto i = static_cast<size_t>(y * coverage.width + x);
                Paint paint;
                if (!glow.empty()) paint.Over(std::min(1.0f, glow[i] * kGlowOpacity * layers.glowStrength), layers.glowColor);
                if (!shadow.empty())
                {
                    const int above = y - layers.shadowDrop;
                    auto strong = [&](float cover) { return std::min(1.0f, cover * layers.shadowStrength); };
                    if (above >= 0 && above < coverage.height)
                        paint.Over(strong(shadow[static_cast<size_t>(above * coverage.width + x)]), layers.shadowColor);
                    paint.Over(strong(shadow[i]), layers.shadowColor);
                }
                paint.Over(Cover(coverage, i), x >= layers.markX ? layers.markColor : layers.color);
                image.argb[i] = paint.Color();
            }
        }
        return image;
    }

    Image Outlined(const Coverage& coverage, int radius, uint32_t color, uint32_t outlineColor)
    {
        Image image{coverage.width, coverage.height, std::vector<uint32_t>(coverage.alpha.size(), 0)};
        for (int y = 0; y < coverage.height; ++y)
        {
            for (int x = 0; x < coverage.width; ++x)
            {
                uint8_t most = 0;
                for (int dy = -radius; dy <= radius && most < kFullAlpha; ++dy)
                {
                    for (int dx = -radius; dx <= radius; ++dx)
                    {
                        const int nx = x + dx, ny = y + dy;
                        if (dx * dx + dy * dy > radius * radius + radius) continue; // a disc, with its diagonals at 1
                        if (nx < 0 || ny < 0 || nx >= coverage.width || ny >= coverage.height) continue;
                        most = std::max(most, coverage.alpha[static_cast<size_t>(ny * coverage.width + nx)]);
                    }
                }
                const auto i = static_cast<size_t>(y * coverage.width + x);
                Paint paint;
                paint.Over(static_cast<float>(most) / kFullCoverage, outlineColor);
                paint.Over(Cover(coverage, i), color);
                image.argb[i] = paint.Color();
            }
        }
        return image;
    }

    Image Ornament(int width, int height, float shadowBlur, float shadowStrength, uint32_t color, uint32_t shadowColor)
    {
        const int margin = BlurMargin(shadowBlur);
        const int w = std::max(1, width), h = std::max(1, height);
        Coverage gem{w + 2 * margin, h + 2 * margin, {}};
        gem.alpha.assign(static_cast<size_t>(gem.width) * static_cast<size_t>(gem.height), 0);
        const float centerX = static_cast<float>(margin) + static_cast<float>(w) / 2.0f;
        const float centerY = static_cast<float>(margin) + static_cast<float>(h) / 2.0f;
        const float half    = static_cast<float>(h) / 2.0f;
        for (int y = 0; y < gem.height; ++y)
        {
            for (int x = 0; x < gem.width; ++x)
            {
                // Covered by how far inside the diamond the pixel's center is, so its slopes are smooth.
                const float away  = std::abs(static_cast<float>(x) + 0.5f - centerX) + std::abs(static_cast<float>(y) + 0.5f - centerY);
                const float cover = std::clamp(half - away + 0.5f, 0.0f, 1.0f);
                gem.alpha[static_cast<size_t>(y * gem.width + x)] = static_cast<uint8_t>(std::lround(cover * kFullCoverage));
            }
        }
        const std::vector<float> shadow = shadowBlur > 0.0f ? Blurred(gem, shadowBlur) : std::vector<float>(gem.alpha.size(), 0.0f);
        const int thickness = std::max(1, static_cast<int>(std::lround(static_cast<float>(h) * kLinePerHeight)));
        const int lineTop   = margin + h / 2 - thickness / 2;
        const uint32_t gemColor = Lighter(color);
        Image image{gem.width, gem.height, std::vector<uint32_t>(gem.alpha.size(), 0)};
        for (int y = 0; y < gem.height; ++y)
        {
            for (int x = 0; x < gem.width; ++x)
            {
                const auto i = static_cast<size_t>(y * gem.width + x);
                Paint paint;
                if (x >= margin && x < margin + w && y >= lineTop && y < lineTop + thickness)
                {
                    const float along = (static_cast<float>(x - margin) + 0.5f) / static_cast<float>(w);
                    const float fromEnd = std::min(along, 1.0f - along);
                    const float lighten = kLineWhitest * std::max(0.0f, 1.0f - std::abs(along - 0.5f) / kLineBrightSpan);
                    paint.Over(std::min(1.0f, fromEnd / kLineFadeIn), Whiter(color, lighten));
                }
                paint.Over(std::min(1.0f, shadow[i] * shadowStrength), shadowColor);
                paint.Over(Cover(gem, i), gemColor);
                image.argb[i] = paint.Color();
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
