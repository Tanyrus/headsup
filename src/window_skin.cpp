#include "window_skin.h"

#include "argb.h"
#include "bar_document.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <utility>
#include <vector>

namespace headsup
{
    namespace
    {
        constexpr int kFirstSkinFile         = 14;
        constexpr const char* kSkinFolder    = "ROM\\0\\";
        constexpr const char* kBackChunk     = "newt";
        constexpr const char* kAcrossChunk   = "hfr1";
        constexpr const char* kDownChunk     = "vfr1";
        constexpr const char* kCornerChunk   = "corn";
        constexpr const char* kShapesChunk   = "win0";

        // The shapes chunk's body: its 16-byte name, a count of the pictures it draws from and their 16-byte names, a
        // shape count, then the shapes, the background first: a flag, twelve int16 corners and rectangle, a byte, and
        // a color a corner as four bytes, top left, top right, bottom left, bottom right.
        constexpr size_t kNameBytes      = 16;
        constexpr size_t kPicturesAt     = kNameBytes;
        constexpr size_t kShapeColorsAt  = 1 + 12 * sizeof(int16_t) + 1;
        constexpr size_t kColorBytes     = 4;
        constexpr size_t kTopLeftColor   = 0, kBottomLeftColor = 2;
        constexpr size_t kShapeBytes     = kShapeColorsAt + 4 * kColorBytes;
        // The game's colors are at half: 0x80 is the picture as it is.
        constexpr float kFullLevel = 128.0f;

        constexpr std::string_view kSkinKind = "skin/";
        constexpr char kSeparator            = '/';
        constexpr int kMaxPiecePixels        = 1024;
        constexpr size_t kBgraBytes          = 4;
        constexpr float kFullByte            = 255.0f;
        // Where the game cuts its pieces: corners at these offsets of the corner picture, edges from these rows or
        // columns of theirs.
        constexpr int kNearCut = 1, kFarCornerCut = 7, kFarEdgeCut = 26;
        constexpr const char* kPieceNames[] = {"back", "top", "bottom", "left", "right", "top-left", "top-right",
            "bottom-left", "bottom-right", "clear"};
        static_assert(std::size(kPieceNames) == static_cast<size_t>(SkinPiece::Clear) + 1);

        struct Cut
        {
            const ColorPicture* picture;
            int x, y, width, height; // the rectangle of it
        };

        Cut CutOf(const WindowSkin& skin, SkinPiece piece)
        {
            const int length = skin.across.width, height = skin.down.height;
            switch (piece)
            {
            case SkinPiece::Back: return {&skin.back, 0, 0, skin.back.width, skin.back.height};
            case SkinPiece::Top: return {&skin.across, 0, kNearCut, length, kFrameEdge};
            case SkinPiece::Bottom: return {&skin.across, 0, kFarEdgeCut, length, kFrameEdge};
            case SkinPiece::Left: return {&skin.down, kNearCut, 0, kFrameEdge, height};
            case SkinPiece::Right: return {&skin.down, kFarEdgeCut, 0, kFrameEdge, height};
            case SkinPiece::TopLeft: return {&skin.corner, kNearCut, kNearCut, kFrameCorner, kFrameCorner};
            case SkinPiece::TopRight: return {&skin.corner, kFarCornerCut, kNearCut, kFrameCorner, kFrameCorner};
            case SkinPiece::BottomLeft: return {&skin.corner, kNearCut, kFarCornerCut, kFrameCorner, kFrameCorner};
            case SkinPiece::BottomRight: return {&skin.corner, kFarCornerCut, kFarCornerCut, kFrameCorner, kFrameCorner};
            case SkinPiece::Clear: break;
            }
            return {nullptr, 0, 0, 0, 0};
        }

        // How much of the source span [from, from + 1) of one axis a pixel at [a, b) of it covers.
        float Overlap(float a, float b, int from) { return std::max(0.0f, std::min(b, static_cast<float>(from + 1)) - std::max(a, static_cast<float>(from))); }

        // A rectangle of a picture drawn width by height: each pixel the mean of what it covers, by alpha, so clear
        // pixels lend no color.
        Image Resampled(const Cut& cut, int width, int height)
        {
            Image image{width, height, std::vector<uint32_t>(static_cast<size_t>(width) * static_cast<size_t>(height))};
            const float sx = static_cast<float>(cut.width) / static_cast<float>(width);
            const float sy = static_cast<float>(cut.height) / static_cast<float>(height);
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                {
                    const float x0 = static_cast<float>(x) * sx, x1 = x0 + sx, y0 = static_cast<float>(y) * sy, y1 = y0 + sy;
                    float sum[4] = {}, area = 0.0f;
                    for (int v = static_cast<int>(y0); v < static_cast<int>(std::ceil(y1)) && v < cut.height; ++v)
                        for (int u = static_cast<int>(x0); u < static_cast<int>(std::ceil(x1)) && u < cut.width; ++u)
                        {
                            const float weight = Overlap(x0, x1, u) * Overlap(y0, y1, v);
                            const uint8_t* pixel = cut.picture->bgra.data() +
                                (static_cast<size_t>(cut.y + v) * static_cast<size_t>(cut.picture->width) + static_cast<size_t>(cut.x + u)) * kBgraBytes;
                            const float alpha = static_cast<float>(pixel[3]) / kFullByte * weight;
                            for (int c = 0; c < 3; ++c)
                                sum[c] += static_cast<float>(pixel[c]) * alpha;
                            sum[3] += alpha;
                            area += weight;
                        }
                    uint32_t argb = 0;
                    if (sum[3] > 0.0f)
                    {
                        for (int c = 0; c < 3; ++c)
                            argb |= static_cast<uint32_t>(std::lround(sum[c] / sum[3])) << (8 * c);
                        argb |= static_cast<uint32_t>(std::lround(sum[3] / area * kFullByte)) << 24;
                    }
                    image.argb[static_cast<size_t>(y * width + x)] = argb;
                }
            return image;
        }

        std::optional<int> Number(std::string_view text)
        {
            int value = 0;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (error != std::errc() || end != text.data() + text.size()) return std::nullopt;
            return value;
        }

        float LevelOf(const uint8_t* color)
        {
            return std::min(1.0f, static_cast<float>(std::max({color[0], color[1], color[2]})) / kFullLevel);
        }
    }

    std::string WindowSkinFile(int type) { return kSkinFolder + std::to_string(kFirstSkinFile + type) + ".DAT"; }

    int SkinPixels(int menuPixels, float scale)
    {
        return std::max(1, static_cast<int>(std::lround(static_cast<float>(menuPixels) * scale)));
    }

    std::string SkinSource(int type, SkinPiece piece, int pixels)
    {
        return std::string(kHostSource) + std::string(kSkinKind) + std::to_string(type) + kSeparator +
               kPieceNames[static_cast<size_t>(piece)] + kSeparator + std::to_string(pixels);
    }

    std::optional<Image> SkinImage(const WindowSkins& skins, std::string_view name)
    {
        const std::string prefix = std::string(kHostSource) + std::string(kSkinKind);
        if (!name.starts_with(prefix)) return std::nullopt;
        name.remove_prefix(prefix.size());
        const size_t first = name.find(kSeparator), second = name.find(kSeparator, first + 1);
        if (first == std::string_view::npos || second == std::string_view::npos) return std::nullopt;
        const std::optional<int> type = Number(name.substr(0, first)), pixels = Number(name.substr(second + 1));
        const std::string_view pieceName = name.substr(first + 1, second - first - 1);
        const auto* named = std::find(std::begin(kPieceNames), std::end(kPieceNames), pieceName);
        if (!type || *type < 0 || *type >= kWindowTypes || !pixels || *pixels < 1 || *pixels > kMaxPiecePixels ||
            named == std::end(kPieceNames) || !skins[static_cast<size_t>(*type)])
            return std::nullopt;
        const auto piece = static_cast<SkinPiece>(named - std::begin(kPieceNames));
        if (piece == SkinPiece::Clear) return Image{1, 1, {0}};
        const Cut cut = CutOf(*skins[static_cast<size_t>(*type)], piece);
        switch (piece)
        {
        case SkinPiece::Top:
        case SkinPiece::Bottom: return Resampled(cut, cut.width, *pixels);
        case SkinPiece::Left:
        case SkinPiece::Right: return Resampled(cut, *pixels, cut.height);
        default: return Resampled(cut, *pixels, *pixels);
        }
    }

    namespace
    {
        constexpr uint8_t kOpaqueEnough = 0x80; // a frame pixel at least this opaque counts toward its color
        constexpr float kGreyChroma     = 0.05f; // a saturation under which a background reads as grey
        constexpr float kAccentChroma   = 0.45f; // the least saturation an accent of a hue is drawn with
        constexpr float kAccentLight = 0.68f, kAccentHoverLight = 0.78f, kSubtitleLight = 0.75f, kDividerLight = 0.35f;
        constexpr float kSubtitleChroma = 0.5f; // of the accent's
        constexpr float kTitleTint = 0.08f, kTextTint = 0.15f; // the accent's share in the text's white
        constexpr float kDarkText  = 0.18f; // a background lighter than this luminance takes dark text
        constexpr uint32_t kLightText = 0xFFFFFFFF, kDarkTextColor = 0xFF101010;
        constexpr uint32_t kHighlightAlpha = 0x33, kTagAlpha = 0x26, kPanelAlpha = 0xF7, kSoftAlpha = 0x47;
        constexpr uint32_t kCardAlpha = 0xB3, kBorderAlpha = 0x40, kRaisedAlpha = 0x66;
        constexpr float kCardShade = 0.6f, kInsetShade = 0.5f; // a card and a tray, of the background
        constexpr float kControlLift = 0.10f, kHoverLift = 0.18f; // a control's lightness over the background's
        constexpr float kControlChroma = 0.6f;                    // of the accent's
        constexpr float kLabelTint     = 0.2f;                    // the background's share in a label's text color

        struct Rgb
        {
            float r = 0, g = 0, b = 0; // 0 to 1
        };
        struct Hsl
        {
            float h = 0, s = 0, l = 0; // h in turns
        };

        Rgb RgbOf(uint32_t argb)
        {
            return {Channel(argb, kRedShift) / kFullByte, Channel(argb, kGreenShift) / kFullByte, Channel(argb, kBlueShift) / kFullByte};
        }
        uint32_t ArgbOf(Rgb c, uint32_t alpha = kOpaqueAlpha)
        {
            const auto byte = [](float v) { return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * kFullByte)); };
            return Argb(static_cast<uint8_t>(alpha), byte(c.r), byte(c.g), byte(c.b));
        }
        Rgb Times(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }
        Rgb Mixed(Rgb a, Rgb b, float share) { return {a.r + (b.r - a.r) * share, a.g + (b.g - a.g) * share, a.b + (b.b - a.b) * share}; }

        Hsl HslOf(Rgb c)
        {
            const float hi = std::max({c.r, c.g, c.b}), lo = std::min({c.r, c.g, c.b}), l = (hi + lo) / 2.0f, d = hi - lo;
            if (d <= 0.0f) return {0.0f, 0.0f, l};
            const float s = d / (1.0f - std::abs(2.0f * l - 1.0f));
            float h       = hi == c.r ? std::fmod((c.g - c.b) / d, 6.0f) : hi == c.g ? (c.b - c.r) / d + 2.0f : (c.r - c.g) / d + 4.0f;
            h             = h / 6.0f;
            return {h < 0.0f ? h + 1.0f : h, s, l};
        }
        Rgb RgbOf(Hsl c)
        {
            const auto channel = [&](float n) {
                const float k = std::fmod(n + c.h * 12.0f, 12.0f), a = c.s * std::min(c.l, 1.0f - c.l);
                return c.l - a * std::max(-1.0f, std::min({k - 3.0f, 9.0f - k, 1.0f}));
            };
            return {channel(0.0f), channel(8.0f), channel(4.0f)};
        }

        // The mean color of a picture's pixels at least this opaque.
        Rgb MeanOf(const ColorPicture& p, uint8_t leastAlpha)
        {
            double sum[3] = {};
            size_t count  = 0;
            for (size_t i = 0; i + kBgraBytes <= p.bgra.size(); i += kBgraBytes)
            {
                if (p.bgra[i + 3] < leastAlpha) continue;
                for (int c = 0; c < 3; ++c)
                    sum[c] += p.bgra[i + static_cast<size_t>(c)];
                ++count;
            }
            if (count == 0) return {};
            const auto mean = [&](int c) { return static_cast<float>(sum[c] / static_cast<double>(count)) / kFullByte; };
            return {mean(2), mean(1), mean(0)};
        }

        float Linear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }
        float Luminance(Rgb c) { return 0.2126f * Linear(c.r) + 0.7152f * Linear(c.g) + 0.0722f * Linear(c.b); }

        // What every color of a skin is worked out from.
        struct Tones
        {
            Rgb top, middle, bottom, frame; // the background as shaded, and the frame
            Hsl hue;                        // the accent's hue and saturation
            Rgb accent, text;
        };
        Tones TonesOf(const WindowSkin& skin)
        {
            const Rgb back = MeanOf(skin.back, 0);
            Tones t;
            t.top    = Times(back, skin.shadeTop);
            t.bottom = Times(back, skin.shadeBottom);
            t.middle = Times(back, (skin.shadeTop + skin.shadeBottom) / 2.0f);
            t.frame  = MeanOf(skin.across, kOpaqueEnough);
            const Hsl base = HslOf(back);
            t.hue    = {base.h, base.s < kGreyChroma ? 0.0f : std::max(base.s, kAccentChroma), 0.0f};
            t.accent = RgbOf(Hsl{t.hue.h, t.hue.s, kAccentLight});
            t.text   = RgbOf(Luminance(t.middle) < kDarkText ? kLightText : kDarkTextColor);
            return t;
        }
        Rgb Shade(const Tones& t, float chromaShare, float light) { return RgbOf(Hsl{t.hue.h, t.hue.s * chromaShare, light}); }
    }

    BarPalette SkinPalette(const WindowSkin& skin)
    {
        const Tones t = TonesOf(skin);
        const uint32_t accent = ArgbOf(t.accent), frame = ArgbOf(t.frame), text = ArgbOf(Mixed(t.text, t.accent, kTextTint));
        return BarPalette{.shape = BarShape::Window, .stripTop = ArgbOf(t.top), .stripMiddle = ArgbOf(t.middle),
            .stripBottom = ArgbOf(t.bottom), .bottomLine = frame, .underLine = 0, .below = 0, .accent = accent,
            .accentEnd = Lighter(frame), .pill = 0, .pillBorder = 0, .title = ArgbOf(Mixed(t.text, t.accent, kTitleTint)),
            .subtitle = ArgbOf(Shade(t, kSubtitleChroma, kSubtitleLight)), .divider = ArgbOf(Shade(t, 1.0f, kDividerLight)),
            .panelTop = ArgbOf(t.top, kPanelAlpha), .panelBottom = ArgbOf(t.bottom, kPanelAlpha), .panelBorder = frame,
            .panelOuter = 0, .text = text, .heading = accent, .tagFill = WithAlpha(accent, kTagAlpha), .tagBorder = 0,
            .tagText = text, .highlight = WithAlpha(accent, kHighlightAlpha), .border = 0, .gradient = true};
    }

    MenuColors SkinMenuColors(const WindowSkin& skin)
    {
        const Tones t       = TonesOf(skin);
        const float light   = HslOf(t.middle).l;
        const uint32_t accent = ArgbOf(t.accent), frame = ArgbOf(t.frame);
        return MenuColors{ArgbOf(t.middle, kPanelAlpha), ArgbOf(Times(t.middle, kCardShade), kCardAlpha),
            ArgbOf(Shade(t, kControlChroma, light + kControlLift)), ArgbOf(Shade(t, kControlChroma, light + kHoverLift)),
            ArgbOf(Times(t.middle, kInsetShade)), WithAlpha(frame, kBorderAlpha), WithAlpha(frame, kRaisedAlpha),
            ArgbOf(Mixed(t.text, t.accent, kTextTint)), ArgbOf(Mixed(t.text, t.accent, kTitleTint)),
            ArgbOf(Mixed(t.text, t.middle, kLabelTint)), ArgbOf(Shade(t, kSubtitleChroma, kSubtitleLight)), accent,
            ArgbOf(RgbOf(Hsl{t.hue.h, t.hue.s, kAccentHoverLight})), WithAlpha(accent, kSoftAlpha), Lighter(frame)};
    }

    Read<WindowSkin> ReadWindowSkin(std::span<const uint8_t> dat)
    {
        WindowSkin skin;
        for (const auto& [chunk, picture] : {std::pair{kBackChunk, &skin.back}, std::pair{kAcrossChunk, &skin.across},
                 std::pair{kDownChunk, &skin.down}, std::pair{kCornerChunk, &skin.corner}})
        {
            const auto body = FindChunk(dat, chunk, kPictureChunkType);
            if (!body) return {std::nullopt, std::string("it holds no ") + chunk};
            Read<ColorPicture> read = ReadColorPicture(*body);
            if (!read.value) return {std::nullopt, std::string(chunk) + ": " + read.failure};
            *picture = std::move(*read.value);
        }
        // Every piece is cut from where the game cuts it, which an overlay's smaller pictures would not hold.
        constexpr int kEdgesSpan = kFarEdgeCut + kFrameEdge, kCornersSpan = kFarCornerCut + kFrameCorner;
        if (skin.across.height < kEdgesSpan || skin.down.width < kEdgesSpan || skin.corner.width < kCornersSpan ||
            skin.corner.height < kCornersSpan)
            return {std::nullopt, "its frame's pictures are too small"};
        const auto shapes = FindChunk(dat, kShapesChunk, kShapesChunkType);
        if (!shapes || shapes->size() <= kPicturesAt) return {std::nullopt, "it holds no shapes"};
        const size_t first = kPicturesAt + 1 + (*shapes)[kPicturesAt] * kNameBytes + sizeof(uint16_t);
        if (shapes->size() < first + kShapeBytes) return {std::nullopt, "its shapes end early"};
        const uint8_t* colors = shapes->data() + first + kShapeColorsAt;
        skin.shadeTop         = LevelOf(colors + kTopLeftColor * kColorBytes);
        skin.shadeBottom      = LevelOf(colors + kBottomLeftColor * kColorBytes);
        return {std::move(skin), {}};
    }
}
