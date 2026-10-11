#include "plate_image.h"

#include "argb.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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

            // A part that overlaps none of the others: its share adds to theirs instead of covering them.
            void Add(float cover, uint32_t color)
            {
                if (cover <= 0.0f) return;
                red   += static_cast<float>(Channel(color, kRedShift)) * cover;
                green += static_cast<float>(Channel(color, kGreenShift)) * cover;
                blue  += static_cast<float>(Channel(color, kBlueShift)) * cover;
                alpha += cover;
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
            // Uncovered pixels only ever add zero, so each sum runs over the covered span alone, in the same order: the
            // glow's margins are most of its image.
            std::vector<float> across(c.alpha.size(), 0.0f), plane(c.alpha.size(), 0.0f);
            int top = c.height, bottom = -1, left = c.width, right = -1;
            for (int y = 0; y < c.height; ++y)
            {
                int first = c.width, last = -1;
                for (int x = 0; x < c.width; ++x)
                {
                    if (c.alpha[static_cast<size_t>(y * c.width + x)] == 0) continue;
                    first = std::min(first, x);
                    last  = x;
                }
                if (last < 0) continue;
                top = std::min(top, y), bottom = y, left = std::min(left, first), right = std::max(right, last);
                for (int x = std::max(0, first - taps); x <= std::min(c.width - 1, last + taps); ++x)
                    for (int i = std::max(-taps, first - x); i <= std::min(taps, last - x); ++i)
                        across[static_cast<size_t>(y * c.width + x)] +=
                            Cover(c, static_cast<size_t>(y * c.width + x + i)) * kernel[static_cast<size_t>(i + taps)];
            }
            for (int y = std::max(0, top - taps); y <= std::min(c.height - 1, bottom + taps); ++y)
                for (int x = std::max(0, left - taps); x <= std::min(c.width - 1, right + taps); ++x)
                    for (int i = std::max(-taps, top - y); i <= std::min(taps, bottom - y); ++i)
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

        uint32_t Mix(uint32_t a, uint32_t b, float t)
        {
            auto channel = [&](int shift) {
                const float from = Channel(a, shift), to = Channel(b, shift);
                return static_cast<uint8_t>(std::lround(from + (to - from) * std::clamp(t, 0.0f, 1.0f)));
            };
            return Argb(channel(kAlphaShift), channel(kRedShift), channel(kGreenShift), channel(kBlueShift));
        }

        uint32_t WithAlpha(uint32_t argb, float share)
        {
            const auto alpha =
                static_cast<uint8_t>(std::lround(Channel(argb, kAlphaShift) * std::clamp(share, 0.0f, 1.0f)));
            return (argb & ~kAlphaMask) | static_cast<uint32_t>(alpha) << kAlphaShift;
        }

        // How much of a pixel a rounded box covers: its center's distance inside the edge, plus a half. The distance is
        // from the box's core, the box less its corners' radius, and comes apart into a column's and a row's share.
        class Rounded
        {
        public:
            Rounded(float left, float top, float width, float height, float radius)
                : m_Empty(width <= 0.0f || height <= 0.0f),
                  m_Radius(m_Empty ? 0.0f : std::clamp(radius, 0.0f, std::min(width, height) / 2.0f)),
                  m_CenterX(left + width / 2.0f), m_CenterY(top + height / 2.0f), m_CoreX(width / 2.0f - m_Radius),
                  m_CoreY(height / 2.0f - m_Radius)
            {
            }

            // How far column x's center is past the core, across; 0 in line with it.
            float Across(int x) const { return Past(static_cast<float>(x), m_CenterX, m_CoreX); }
            float Down(int y) const { return Past(static_cast<float>(y), m_CenterY, m_CoreY); }
            float Cover(float across, float down) const
            {
                if (m_Empty) return 0.0f;
                const float outside = std::sqrt(across * across + down * down) - m_Radius;
                return std::clamp(0.5f - outside, 0.0f, 1.0f);
            }
            float Cover(int x, int y) const { return Cover(Across(x), Down(y)); }
            // Whether column x is in line with the core, so its pixels are covered by their row alone.
            bool InCore(int x) const { return m_Empty || Across(x) == 0.0f; }

        private:
            static float Past(float pixel, float center, float core)
            {
                return std::max(std::abs(pixel + 0.5f - center) - core, 0.0f);
            }

            bool m_Empty;
            float m_Radius, m_CenterX, m_CenterY, m_CoreX, m_CoreY;
        };

        // Box's pixels a row at a time: its outline, the inside of its border and the outer ring's two edges cover a
        // pixel by how far across and down it is from their cores, so in the columns in line with every core, a row's
        // pixels are all the same.
        class BoxPainter
        {
        public:
            BoxPainter(int width, int height, const BoxStyle& style)
                : m_Style(style), m_Pad(OuterPad(style)),
                  m_Width(std::max(1, width) + 2 * m_Pad), m_Height(std::max(1, height) + 2 * m_Pad),
                  m_Edge(static_cast<float>(m_Pad)), m_BoxWidth(static_cast<float>(std::max(1, width))),
                  m_BoxHeight(static_cast<float>(std::max(1, height))), m_Gap(static_cast<float>(style.outerGap)),
                  m_Shape(m_Edge, m_Edge, m_BoxWidth, m_BoxHeight, style.radius),
                  m_Inner(m_Edge + 1.0f, m_Edge + 1.0f, m_BoxWidth - 2.0f, m_BoxHeight - 2.0f, style.radius - 1.0f),
                  m_RingOut(m_Edge - m_Gap - 1.0f, m_Edge - m_Gap - 1.0f, m_BoxWidth + 2.0f * m_Gap + 2.0f,
                      m_BoxHeight + 2.0f * m_Gap + 2.0f, style.radius + m_Gap + 1.0f),
                  m_RingIn(m_Edge - m_Gap, m_Edge - m_Gap, m_BoxWidth + 2.0f * m_Gap, m_BoxHeight + 2.0f * m_Gap,
                      style.radius + m_Gap)
            {
                while (m_Middle < m_Width && !Still(m_Middle))
                    ++m_Middle;
                m_MiddleEnd = m_Width;
                while (m_MiddleEnd > m_Middle && !Still(m_MiddleEnd - 1))
                    --m_MiddleEnd;
            }

            int Width() const { return m_Width; }
            int Height() const { return m_Height; }
            // The columns from Middle to MiddleEnd (exclusive) are alike in every row; none when they are equal.
            int Middle() const { return m_Middle; }
            int MiddleEnd() const { return m_MiddleEnd; }

            void StartRow(int y)
            {
                m_Fill        = Mix(m_Style.top, m_Style.bottom, (static_cast<float>(y) - m_Edge + 0.5f) / m_BoxHeight);
                m_ShapeDown   = m_Shape.Down(y);
                m_InnerDown   = m_Inner.Down(y);
                m_RingOutDown = m_RingOut.Down(y);
                m_RingInDown  = m_RingIn.Down(y);
            }

            uint32_t Pixel(int x) const
            {
                const float shape = m_Shape.Cover(m_Shape.Across(x), m_ShapeDown);
                const float inner = m_Style.border != 0 ? m_Inner.Cover(m_Inner.Across(x), m_InnerDown) : shape;
                Paint paint;
                if (m_Style.outer != 0)
                {
                    const float ring = m_RingOut.Cover(m_RingOut.Across(x), m_RingOutDown) -
                                       m_RingIn.Cover(m_RingIn.Across(x), m_RingInDown);
                    paint.Add(ring * Channel(m_Style.outer, kAlphaShift) / kFullCoverage, Opaque(m_Style.outer));
                }
                paint.Add(inner * Channel(m_Fill, kAlphaShift) / kFullCoverage, Opaque(m_Fill));
                if (m_Style.border != 0)
                    paint.Add((shape - inner) * Channel(m_Style.border, kAlphaShift) / kFullCoverage,
                        Opaque(m_Style.border));
                return paint.Color();
            }

        private:
            bool Still(int x) const
            {
                const bool inner = m_Style.border == 0 || m_Inner.InCore(x);
                const bool ring  = m_Style.outer == 0 || (m_RingOut.InCore(x) && m_RingIn.InCore(x));
                return m_Shape.InCore(x) && inner && ring;
            }

            const BoxStyle& m_Style;
            int m_Pad, m_Width, m_Height;
            float m_Edge, m_BoxWidth, m_BoxHeight, m_Gap; // the box's left and top in the image, and its size
            Rounded m_Shape, m_Inner, m_RingOut, m_RingIn;
            int m_Middle = 0, m_MiddleEnd = 0;
            uint32_t m_Fill = 0;
            float m_ShapeDown = 0.0f, m_InnerDown = 0.0f, m_RingOutDown = 0.0f, m_RingInDown = 0.0f;
        };

        // The middle stop of a strip's gradient sits 55% of the way down, as the mockup's CSS has it.
        constexpr float kStripMiddle = 0.55f;
        constexpr float kRimShade    = 0.6f; // a coin's rim, as a share of its color
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
        const int thickness = WholePixels(static_cast<float>(h) * kLinePerHeight);
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

    Image StripColumn(int height, int belowRows, uint32_t top, uint32_t middle, uint32_t bottom, uint32_t bottomLine,
        uint32_t underLine, uint32_t below)
    {
        const int h = std::max(1, height), under = underLine != 0 ? 1 : 0;
        const int fade = below != 0 ? std::max(0, belowRows) : 0;
        Image image{1, h + under + fade, {}};
        image.argb.reserve(static_cast<size_t>(image.height));
        for (int y = 0; y < h - 1; ++y)
        {
            const float t = h > 2 ? static_cast<float>(y) / static_cast<float>(h - 2) : 0.0f;
            image.argb.push_back(t < kStripMiddle ? Mix(top, middle, t / kStripMiddle)
                                                  : Mix(middle, bottom, (t - kStripMiddle) / (1.0f - kStripMiddle)));
        }
        image.argb.push_back(bottomLine);
        if (under != 0) image.argb.push_back(underLine);
        for (int y = 0; y < fade; ++y)
            image.argb.push_back(WithAlpha(below, 1.0f - static_cast<float>(y) / static_cast<float>(fade)));
        return image;
    }

    InkBox FindInk(const Coverage& coverage, uint8_t threshold)
    {
        InkBox ink{coverage.width, coverage.height, 0, 0};
        for (int y = 0; y < coverage.height; ++y)
        {
            const uint8_t* row = coverage.alpha.data() + static_cast<size_t>(y) * static_cast<size_t>(coverage.width);
            for (int x = 0; x < coverage.width; ++x)
                if (row[x] >= threshold)
                {
                    ink.left   = std::min(ink.left, x);
                    ink.top    = std::min(ink.top, y);
                    ink.right  = std::max(ink.right, x + 1);
                    ink.bottom = std::max(ink.bottom, y + 1);
                }
        }
        return ink.Empty() ? InkBox{} : ink;
    }

    std::optional<LineInk> LineInkOf(const InkBox& glyphs, const InkBox& capital)
    {
        if (glyphs.Empty() || capital.Empty()) return std::nullopt;
        return LineInk{
            static_cast<float>(glyphs.top), static_cast<float>(capital.bottom), static_cast<float>(glyphs.bottom)};
    }

    Image Box(int width, int height, const BoxStyle& style)
    {
        BoxPainter painter(width, height, style);
        Image image{painter.Width(), painter.Height(), {}};
        image.argb.resize(static_cast<size_t>(image.width) * static_cast<size_t>(image.height));
        for (int y = 0; y < image.height; ++y)
        {
            painter.StartRow(y);
            uint32_t* row = image.argb.data() + static_cast<size_t>(y) * static_cast<size_t>(image.width);
            for (int x = 0; x < painter.Middle(); ++x)
                row[x] = painter.Pixel(x);
            if (painter.Middle() < painter.MiddleEnd())
                std::fill(row + painter.Middle(), row + painter.MiddleEnd(), painter.Pixel(painter.Middle()));
            for (int x = painter.MiddleEnd(); x < image.width; ++x)
                row[x] = painter.Pixel(x);
        }
        return image;
    }

    int OuterPad(const BoxStyle& style) { return style.outer != 0 ? style.outerGap + 1 : 0; }

    SlicedBox SliceBox(int width, int height, const BoxStyle& style)
    {
        BoxPainter painter(width, height, style);
        const int left = painter.Middle(), right = painter.Width() - painter.MiddleEnd();
        // A middle no wider than the copies of one column would be is drawn whole.
        if (painter.MiddleEnd() - painter.Middle() <= kSliceMiddleColumns)
            return SlicedBox{Box(width, height, style), painter.Width(), painter.Width(), 0};
        SlicedBox sliced{Image{left + kSliceMiddleColumns + right, painter.Height(), {}}, painter.Width(), left, right};
        sliced.image.argb.resize(static_cast<size_t>(sliced.image.width) * static_cast<size_t>(sliced.image.height));
        for (int y = 0; y < sliced.image.height; ++y)
        {
            painter.StartRow(y);
            uint32_t* row = sliced.image.argb.data() + static_cast<size_t>(y) * static_cast<size_t>(sliced.image.width);
            for (int x = 0; x < left; ++x)
                row[x] = painter.Pixel(x);
            std::fill_n(row + left, kSliceMiddleColumns, painter.Pixel(painter.Middle()));
            for (int x = 0; x < right; ++x)
                row[left + kSliceMiddleColumns + x] = painter.Pixel(painter.MiddleEnd() + x);
        }
        return sliced;
    }

    std::vector<SliceSpan> SliceSpans(const SlicedBox& box, float u)
    {
        const float column = u / static_cast<float>(box.image.width); // one image column's share of u
        std::vector<SliceSpan> spans{SliceSpan{0, box.left, 0.0f, static_cast<float>(box.left) * column}};
        if (box.left + box.right >= box.width) return spans;
        const float middle = (static_cast<float>(box.left) + 0.5f) * column;
        spans.push_back(SliceSpan{box.left, box.width - box.left - box.right, middle, middle});
        spans.push_back(SliceSpan{box.width - box.right, box.right, u - static_cast<float>(box.right) * column, u});
        return spans;
    }

    Image Diamond(int size, uint32_t color)
    {
        const int s = std::max(1, size);
        Image image{s, s, std::vector<uint32_t>(static_cast<size_t>(s) * static_cast<size_t>(s), 0)};
        const float half = static_cast<float>(s) / 2.0f;
        for (int y = 0; y < s; ++y)
            for (int x = 0; x < s; ++x)
            {
                const float away = std::abs(static_cast<float>(x) + 0.5f - half) +
                                   std::abs(static_cast<float>(y) + 0.5f - half);
                Paint paint;
                paint.Over(std::clamp(half - away + 0.5f, 0.0f, 1.0f), color);
                image.argb[static_cast<size_t>(y * s + x)] = paint.Color();
            }
        return image;
    }

    Image Coin(int size, uint32_t color)
    {
        const int s = std::max(2, size);
        Image image{s, s, std::vector<uint32_t>(static_cast<size_t>(s) * static_cast<size_t>(s), 0)};
        const uint32_t rim = Mix(kBlack, Opaque(color), kRimShade);
        const float fs = static_cast<float>(s);
        const Rounded face(0.0f, 0.0f, fs, fs, fs / 2.0f), inside(1.0f, 1.0f, fs - 2.0f, fs - 2.0f, fs / 2.0f - 1.0f);
        for (int y = 0; y < s; ++y)
            for (int x = 0; x < s; ++x)
            {
                const float disc = face.Cover(x, y), inner = inside.Cover(x, y);
                Paint paint;
                paint.Add(disc - inner, rim);
                paint.Add(inner, color);
                image.argb[static_cast<size_t>(y * s + x)] = paint.Color();
            }
        return image;
    }

    Image FadingRule(int width, uint32_t color)
    {
        const int w = std::max(1, width);
        Image image{w, 1, {}};
        for (int x = 0; x < w; ++x)
            image.argb.push_back(WithAlpha(color, 1.0f - static_cast<float>(x) / static_cast<float>(w)));
        return image;
    }

    int WholePixels(float size)
    {
        return std::max(1, static_cast<int>(std::lround(size)));
    }

    int TextureSide(int pixels)
    {
        int side = 1;
        while (side < pixels)
            side *= 2;
        return side;
    }

    void CopyToCorner(const void* image, int width, int height, void* rows, int pitch, int sideX, int sideY)
    {
        const auto* source   = static_cast<const uint8_t*>(image);
        const auto rowBytes  = static_cast<size_t>(width) * kBytesPerPixel;
        const auto sideBytes = static_cast<size_t>(sideX) * kBytesPerPixel;
        for (int y = 0; y < sideY; ++y)
        {
            auto* row           = static_cast<uint8_t*>(rows) + static_cast<ptrdiff_t>(y) * pitch;
            const size_t copied = y < height ? rowBytes : 0;
            if (copied != 0) std::memcpy(row, source + static_cast<size_t>(y) * rowBytes, copied);
            std::memset(row + copied, 0, sideBytes - copied);
        }
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
        const int width  = WholePixels(static_cast<float>(height) / shape.aspect);
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
