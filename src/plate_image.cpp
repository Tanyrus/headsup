#include "plate_image.h"

#include "argb.h"
#include "plate_settings.h"

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

        // The share of the pixel at (x, y) that a shape covers, sampled at samples points across it each way.
        template <typename Inside>
        float Covered(int x, int y, int samples, const Inside& inside)
        {
            const auto count = static_cast<float>(samples);
            int covered      = 0;
            for (int sy = 0; sy < samples; ++sy)
                for (int sx = 0; sx < samples; ++sx)
                    covered += inside(static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / count,
                        static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) / count);
            return static_cast<float>(covered) / (count * count);
        }

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

        private:
            static float Past(float pixel, float center, float core)
            {
                return std::max(std::abs(pixel + 0.5f - center) - core, 0.0f);
            }

            bool m_Empty;
            float m_Radius, m_CenterX, m_CenterY, m_CoreX, m_CoreY;
        };

        // The middle stop of a strip's gradient sits 55% of the way down, as the mockup's CSS has it.
        constexpr float kStripMiddle = 0.55f;
        constexpr float kRimShade    = 0.6f; // a coin's rim, as a share of its color
        constexpr int kStarPoints    = 5;
        constexpr float kStarNotch   = 0.381966f; // a pentagram's notches, (3 - sqrt 5) / 2 of its points' radius
        constexpr int kPixelSamples  = 8;         // across a pixel, each way: the star's and the reset arrow's
        constexpr float kPi          = 3.14159265f;
        // The reset arrow, in shares of its square: its ring's middle from the center and its thickness, the eighth
        // of a turn left open, and the head's length and half its base.
        constexpr float kResetRadius = 0.32f, kResetStroke = 0.11f, kResetGap = kPi / 4.0f;
        constexpr float kResetHeadLength = 0.26f, kResetHeadHalf = 0.17f;
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
                paint.Over(Cover(coverage, i), layers.color);
                image.argb[i] = paint.Color();
            }
        }
        return image;
    }

    namespace
    {
        // The outline grown around a coverage, under the color colorAt gives each covered pixel.
        template <typename ColorAt>
        Image OutlinedWith(const Coverage& coverage, int radius, uint32_t outlineColor, ColorAt colorAt)
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
                    paint.Over(Cover(coverage, i), colorAt(x, y));
                    image.argb[i] = paint.Color();
                }
            }
            return image;
        }
    }

    Image Outlined(const Coverage& coverage, int radius, uint32_t color, uint32_t outlineColor)
    {
        return OutlinedWith(coverage, radius, outlineColor, [&](int, int) { return color; });
    }

    namespace
    {
        // A mark's size and room, in shares of its text's height: across (a diamond, corner to corner, a little
        // more than the others, which fill their box), the room either side, and how far down the line its middle is.
        constexpr float kMarkShare = 0.3f, kDiamondShare = 0.4f, kMarkRoom = 0.1f, kMarkMiddle = 0.57f;
        constexpr int kMarkSamples = 4; // across a pixel, each way

        float MarkAcross(DashMark mark, int textHeight)
        {
            return static_cast<float>(WholePixels(static_cast<float>(textHeight) * (mark == DashMark::Diamond ? kDiamondShare : kMarkShare)));
        }
    }

    int MarkWidth(DashMark mark, int textHeight)
    {
        return static_cast<int>(MarkAcross(mark, textHeight)) + 2 * WholePixels(static_cast<float>(textHeight) * kMarkRoom);
    }

    Coverage MarkCoverage(DashMark mark, int textHeight, int margin)
    {
        const int height = std::max(1, textHeight), width = MarkWidth(mark, height);
        Coverage c{width + 2 * margin, height + 2 * margin, {}};
        c.alpha.assign(static_cast<size_t>(c.width) * static_cast<size_t>(c.height), 0);
        const float half = MarkAcross(mark, height) / 2.0f;
        // Its middle on the middle of a row, so that it is the same above and below.
        const float middleX = static_cast<float>(c.width) / 2.0f;
        const float middleY = static_cast<float>(margin) + std::floor(static_cast<float>(height) * kMarkMiddle) + 0.5f;
        const auto inside   = [&](float x, float y) {
            const float dx = std::fabs(x - middleX), dy = std::fabs(y - middleY);
            switch (mark)
            {
            case DashMark::Dot: return dx * dx + dy * dy <= half * half;
            case DashMark::Square: return dx <= half && dy <= half;
            case DashMark::Diamond: return dx + dy <= half;
            case DashMark::None: break;
            }
            return false;
        };
        for (int y = 0; y < c.height; ++y)
            for (int x = 0; x < c.width; ++x)
                c.alpha[static_cast<size_t>(y * c.width + x)] =
                    static_cast<uint8_t>(std::lround(kFullCoverage * Covered(x, y, kMarkSamples, inside)));
        return c;
    }

    namespace
    {
        constexpr int kItalicLeanShare = 4; // of the text's height

        // How far the piece after this one starts from where this one does.
        int Advance(int width, std::span<const int> room, size_t piece, size_t count)
        {
            return width - (piece + 1 < count && piece < room.size() ? room[piece] : 0);
        }
    }

    int ItalicLean(int pixelHeight, uint8_t style)
    {
        return HasStyle(style, FontStyle::Italic) ? pixelHeight / kItalicLeanShare : 0;
    }

    int JoinedWidth(std::span<const int> widths, std::span<const int> room)
    {
        int width = 0;
        for (size_t i = 0; i < widths.size(); ++i)
            width += Advance(widths[i], room, i, widths.size());
        return width;
    }

    Coverage Joined(const std::vector<Coverage>& pieces, std::span<const int> room, int margin)
    {
        std::vector<int> widths;
        Coverage joined{2 * margin, 2 * margin, {}};
        for (const Coverage& piece : pieces)
        {
            widths.push_back(piece.width - 2 * margin);
            joined.height = std::max(joined.height, piece.height);
        }
        joined.width += JoinedWidth(widths, room);
        joined.alpha.assign(static_cast<size_t>(joined.width) * static_cast<size_t>(joined.height), 0);
        int left = 0;
        for (size_t i = 0; i < pieces.size(); ++i)
        {
            const Coverage& piece = pieces[i];
            // Margins and rooms overlap their neighbors: whatever a letter leans into them stays.
            for (int y = 0; y < piece.height; ++y)
                for (int x = 0; x < piece.width; ++x)
                {
                    uint8_t& into = joined.alpha[static_cast<size_t>(y * joined.width + left + x)];
                    into          = std::max(into, piece.alpha[static_cast<size_t>(y * piece.width + x)]);
                }
            left += Advance(widths[i], room, i, pieces.size());
        }
        return joined;
    }

    void Underline(Coverage& text, int top, int thickness, int margin)
    {
        const int bottom = std::min(top + thickness, text.height);
        for (int y = std::max(top, 0); y < bottom; ++y)
            for (int x = margin; x < text.width - margin; ++x)
                text.alpha[static_cast<size_t>(y * text.width + x)] = kFullAlpha;
    }

    Image Resized(const Image& picture, int width, int height)
    {
        const int w = std::max(1, width), h = std::max(1, height);
        Image out{w, h, std::vector<uint32_t>(static_cast<size_t>(w) * static_cast<size_t>(h), 0)};
        if (picture.width <= 0 || picture.height <= 0) return out;
        const float stepX = static_cast<float>(picture.width) / static_cast<float>(w);
        const float stepY = static_cast<float>(picture.height) / static_cast<float>(h);
        // How much of old pixel `at` lies between from and to.
        const auto share = [](int at, float from, float to) {
            return std::max(0.0f, std::min(to, static_cast<float>(at + 1)) - std::max(from, static_cast<float>(at)));
        };
        for (int y = 0; y < h; ++y)
        {
            const float top = static_cast<float>(y) * stepY, bottom = top + stepY;
            for (int x = 0; x < w; ++x)
            {
                const float left = static_cast<float>(x) * stepX, right = left + stepX;
                Paint sum;
                for (int sy = static_cast<int>(top); sy < picture.height && static_cast<float>(sy) < bottom; ++sy)
                {
                    for (int sx = static_cast<int>(left); sx < picture.width && static_cast<float>(sx) < right; ++sx)
                    {
                        const uint32_t argb = picture.argb[static_cast<size_t>(sy * picture.width + sx)];
                        const float area    = share(sx, left, right) * share(sy, top, bottom) / (stepX * stepY);
                        sum.Add(area * static_cast<float>(Channel(argb, kAlphaShift)) / kFullCoverage, argb);
                    }
                }
                out.argb[static_cast<size_t>(y * w + x)] = sum.Color();
            }
        }
        return out;
    }

    Image Edged(const Image& picture, int radius, uint32_t edgeColor)
    {
        Coverage covered{picture.width + 2 * radius, picture.height + 2 * radius, {}};
        covered.alpha.assign(static_cast<size_t>(covered.width) * static_cast<size_t>(covered.height), 0);
        for (int y = 0; y < picture.height; ++y)
            for (int x = 0; x < picture.width; ++x)
                covered.alpha[static_cast<size_t>((y + radius) * covered.width + x + radius)] =
                    Channel(picture.argb[static_cast<size_t>(y * picture.width + x)], kAlphaShift);
        // Only a pixel inside the picture is covered, so only there is its color asked for.
        return OutlinedWith(covered, radius, edgeColor, [&](int x, int y) {
            const bool inside = x >= radius && y >= radius && x < radius + picture.width && y < radius + picture.height;
            return inside ? picture.argb[static_cast<size_t>((y - radius) * picture.width + x - radius)] : 0u;
        });
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

    int OuterPad(const BoxStyle& style) { return style.outer != 0 ? style.outerGap + 1 : 0; }

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

    Image Star(int size, uint32_t color)
    {
        const int s = std::max(1, size);
        Image image{s, s, std::vector<uint32_t>(static_cast<size_t>(s) * static_cast<size_t>(s), 0)};
        const float half = static_cast<float>(s) / 2.0f, sector = kPi / kStarPoints; // from a point to its notch
        const float notchX = half * kStarNotch * std::sin(sector), notchY = half * kStarNotch * std::cos(sector);
        // From the center, x to the right and y up. Folded into the sector between the top point and the notch beside
        // it, the star is the center's side of the line through those two.
        const auto inside = [&](float x, float y) {
            float angle = std::fmod(std::abs(std::atan2(x, y)), 2.0f * sector);
            if (angle > sector) angle = 2.0f * sector - angle;
            const float radius = std::hypot(x, y);
            return notchX * (radius * std::cos(angle) - half) - (notchY - half) * radius * std::sin(angle) <= 0.0f;
        };
        // From the middle, with y up.
        const auto centered = [&](float x, float y) { return inside(x - half, half - y); };
        for (int y = 0; y < s; ++y)
            for (int x = 0; x < s; ++x)
            {
                Paint paint;
                paint.Over(Covered(x, y, kPixelSamples, centered), color);
                image.argb[static_cast<size_t>(y * s + x)] = paint.Color();
            }
        return image;
    }

    Image ResetArrow(int size, uint32_t color)
    {
        const int s = std::max(1, size);
        Image image{s, s, std::vector<uint32_t>(static_cast<size_t>(s) * static_cast<size_t>(s), 0)};
        const float side = static_cast<float>(s), half = side / 2.0f;
        const float radius = side * kResetRadius, stroke = side * kResetStroke / 2.0f;
        // The head's corners, in pixels from the image's corner: its tip left of the ring's top, its base across it.
        const float tipX = half - side * kResetHeadLength, topY = half - radius, base = side * kResetHeadHalf;
        const auto inside = [&](float x, float y) {
            const float dx = x - half, dy = y - half;
            // Counterclockwise from straight up as the screen shows it, y running down.
            const float turn = std::atan2(-dx, -dy);
            const bool gap   = turn >= 0.0f && turn < kResetGap;
            if (std::fabs(std::hypot(dx, dy) - radius) <= stroke && !gap) return true;
            // The head narrows evenly from its base at the ring's top to its tip.
            return x >= tipX && x <= half && std::fabs(y - topY) * (half - tipX) <= base * (x - tipX);
        };
        for (int y = 0; y < s; ++y)
            for (int x = 0; x < s; ++x)
            {
                Paint paint;
                paint.Over(Covered(x, y, kPixelSamples, inside), color);
                image.argb[static_cast<size_t>(y * s + x)] = paint.Color();
            }
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

    int TextTextureSide(uint32_t maxWidth, uint32_t maxHeight)
    {
        constexpr uint32_t kLargest = 4096; // by 1024 is 16 MB; a 16384 px side, which devices allow, is 64 MB
        const uint32_t smaller      = std::min(maxWidth, maxHeight);
        if (smaller == 0) return kFallbackTextureSide;
        const int side = static_cast<int>(std::min(smaller, kLargest));
        return TextureSide(side) == side ? side : TextureSide(side) / 2;
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

    int ShapeWidth(const Shape& shape, int pixelHeight)
    {
        return WholePixels(static_cast<float>(std::max(1, pixelHeight)) / shape.aspect);
    }

    Coverage ShapeCoverage(const Shape& shape, int pixelWidth, int pixelHeight, int margin)
    {
        const int height = std::max(1, pixelHeight);
        const int width  = std::max(1, pixelWidth);
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
