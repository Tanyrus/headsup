#include "window_skin.h"

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
