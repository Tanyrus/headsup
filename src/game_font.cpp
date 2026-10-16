#include "game_font.h"

#include "bytes.h"
#include "fonts.h"
#include "ini_text.h"

#include <algorithm>
#include <utility>

namespace headsup
{
    namespace
    {
        // A DAT chunk's header: its four-letter name, then a word of its type (the low 7 bits) and its size in
        // 16-byte units (the next 19), header included.
        constexpr size_t kChunkHeaderBytes = 16, kChunkNameBytes = 4, kChunkSizeUnit = 16;
        constexpr uint32_t kChunkTypeMask  = 0x7F, kChunkSizeMask = 0x7FFFF;
        constexpr int kChunkSizeShift      = 7;

        // A picture chunk's body: a flag, a 16-byte name, a BITMAPINFOHEADER, the four-letter code backwards, the
        // data's size and a dword, then the blocks.
        constexpr uint8_t kPictureFlag       = 0xA1;
        constexpr size_t kNameAt             = 1, kNameBytes = 16;
        constexpr size_t kInfoHeaderAt       = kNameAt + kNameBytes;
        constexpr uint32_t kInfoHeaderBytes  = 40;
        constexpr size_t kWidthAt            = kInfoHeaderAt + 4, kHeightAt = kInfoHeaderAt + 8;
        constexpr size_t kCodeAt             = kInfoHeaderAt + kInfoHeaderBytes;
        constexpr size_t kBlocksAt           = kCodeAt + 12;
        constexpr std::string_view kDxt3Code = "3TXD", kDxt1Code = "1TXD";

        // Direct3D's blocks: 4 x 4 pixels, the first in the lowest bits. DXT1 is two 565 colors and a 2-bit pick a
        // pixel; DXT3 is 16 alphas of 4 bits before such a block, which then always has four colors.
        constexpr int kBlockSide           = 4;
        constexpr int kBlockPixels         = kBlockSide * kBlockSide;
        constexpr size_t kDxt3BlockBytes   = 16, kDxt1BlockBytes = 8, kAlphaBytes = 8;
        constexpr int kAlphaBits           = 4;
        constexpr uint32_t kAlphaMask      = 0xF, kAlphaScale = 17; // four bits of alpha to eight
        constexpr int kPickBits            = 2;
        constexpr uint32_t kPickMask       = 3, kClearPick = 3;
        constexpr size_t kSecondColorAt    = 2, kPicksAt = 4;
        constexpr uint32_t kFull           = 255;
        constexpr int kRedShift = 11, kGreenShift = 5;
        constexpr uint32_t kFiveBits = 31, kSixBits = 63;

        // A shapes chunk's body: its name, a count of the pictures it draws from and their 16-byte names, the
        // shape count, then each shape: a flag of 1, its box's four corners, the picture's rectangle as width,
        // height, u and v, a byte of 0 and four colors. Every shape after the first opens with four bytes and the
        // name of its picture; the first draws from the last picture named.
        constexpr size_t kPicturesCountAt = 16;
        constexpr size_t kShapeOpenBytes  = 4;
        constexpr uint8_t kShapeFlag      = 1, kShapeEnd = 0;
        constexpr size_t kShapeValues     = 12;
        constexpr size_t kShapeColorBytes = 16;
        constexpr size_t kShapeBytes      = 1 + kShapeValues * sizeof(int16_t) + 1 + kShapeColorBytes;
        enum ShapeValue : size_t
        {
            kLeft,
            kTop,
            kBottomRightX = 6,
            kBottomRightY,
            kRectWidth,
            kRectHeight,
            kRectU,
            kRectV,
        };

        constexpr std::string_view kFontFile      = "ROM\\119\\51.DAT";
        constexpr std::string_view kPivotSettings = "settings", kPivotRoot = "root_path", kPivotOverlays = "overlays";

        uint32_t Channel8(uint32_t value, uint32_t max) { return value * kFull / max; }

        // The brightest channel of a 565 color: the picture's letters are white and their edge black.
        uint32_t Brightness(uint16_t color)
        {
            return std::max({Channel8(color >> kRedShift & kFiveBits, kFiveBits), Channel8(color >> kGreenShift & kSixBits, kSixBits),
                Channel8(color & kFiveBits, kFiveBits)});
        }

        // Each pixel of a block: its brightness times its alpha.
        std::array<uint8_t, kBlockPixels> BlockFill(const uint8_t* block, bool dxt3)
        {
            std::array<uint32_t, kBlockPixels> alpha{};
            alpha.fill(kFull);
            if (dxt3)
            {
                for (int k = 0; k < kBlockPixels; ++k)
                    alpha[static_cast<size_t>(k)] = (block[k / 2] >> (k % 2 * kAlphaBits) & kAlphaMask) * kAlphaScale;
                block += kAlphaBytes;
            }
            const auto c0 = ReadAt<uint16_t>(block, 0), c1 = ReadAt<uint16_t>(block, kSecondColorAt);
            const uint32_t b0 = Brightness(c0), b1 = Brightness(c1);
            const bool fourColors = dxt3 || c0 > c1;
            const std::array<uint32_t, 4> colors{b0, b1, fourColors ? (2 * b0 + b1) / 3 : (b0 + b1) / 2, fourColors ? (b0 + 2 * b1) / 3 : 0};
            const auto picks = ReadAt<uint32_t>(block, kPicksAt);
            std::array<uint8_t, kBlockPixels> fill{};
            for (int k = 0; k < kBlockPixels; ++k)
            {
                const uint32_t pick = picks >> (k * kPickBits) & kPickMask;
                if (!fourColors && pick == kClearPick) alpha[static_cast<size_t>(k)] = 0;
                fill[static_cast<size_t>(k)] = static_cast<uint8_t>(colors[pick] * alpha[static_cast<size_t>(k)] / kFull);
            }
            return fill;
        }

        // A file under a folder, which may end with its separator already.
        std::string Under(std::string_view folder, std::string_view file)
        {
            std::string path(folder);
            if (!path.empty() && path.back() != '\\' && path.back() != '/') path += '\\';
            return path += file;
        }
    }

    std::optional<GameFontVariant> GameFontVariantOf(std::string_view family)
    {
        if (SameFamily(family, kGameFontName)) return GameFontVariant::Game;
        if (SameFamily(family, kGameFontNarrowName)) return GameFontVariant::Narrow;
        return std::nullopt;
    }

    std::optional<std::span<const uint8_t>> FindChunk(std::span<const uint8_t> dat, std::string_view name, uint8_t type)
    {
        for (size_t at = 0; at + kChunkHeaderBytes <= dat.size();)
        {
            const auto word   = ReadAt<uint32_t>(dat.data(), at + kChunkNameBytes);
            const size_t size = (word >> kChunkSizeShift & kChunkSizeMask) * kChunkSizeUnit;
            if (size < kChunkHeaderBytes || size > dat.size() - at) return std::nullopt;
            if ((word & kChunkTypeMask) == type && std::string_view(reinterpret_cast<const char*>(dat.data() + at), kChunkNameBytes) == name)
                return dat.subspan(at + kChunkHeaderBytes, size - kChunkHeaderBytes);
            at += size;
        }
        return std::nullopt;
    }

    Read<FontPicture> ReadFontPicture(std::span<const uint8_t> body)
    {
        if (body.size() < kBlocksAt || body[0] != kPictureFlag || ReadAt<uint32_t>(body.data(), kInfoHeaderAt) != kInfoHeaderBytes)
            return {std::nullopt, "its picture is not one"};
        const std::string_view code(reinterpret_cast<const char*>(body.data() + kCodeAt), kDxt3Code.size());
        if (code != kDxt3Code && code != kDxt1Code) return {std::nullopt, "its picture is not DXT1 or DXT3"};
        const bool dxt3   = code == kDxt3Code;
        const auto width  = ReadAt<int32_t>(body.data(), kWidthAt);
        const auto height = ReadAt<int32_t>(body.data(), kHeightAt);
        if (width <= 0 || height <= 0 || width > kMaxPictureSide || height > kMaxPictureSide || width % kBlockSide != 0 ||
            height % kBlockSide != 0)
            return {std::nullopt, "its picture is " + std::to_string(width) + " x " + std::to_string(height)};
        const size_t blocksWide = static_cast<size_t>(width / kBlockSide), blocksTall = static_cast<size_t>(height / kBlockSide);
        const size_t blockBytes = dxt3 ? kDxt3BlockBytes : kDxt1BlockBytes;
        if (body.size() - kBlocksAt < blocksWide * blocksTall * blockBytes) return {std::nullopt, "its picture ends early"};
        FontPicture picture;
        picture.name.assign(reinterpret_cast<const char*>(body.data() + kNameAt), kNameBytes);
        picture.width  = width;
        picture.height = height;
        picture.fill.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
        for (size_t by = 0; by < blocksTall; ++by)
            for (size_t bx = 0; bx < blocksWide; ++bx)
            {
                const auto fill = BlockFill(body.data() + kBlocksAt + (by * blocksWide + bx) * blockBytes, dxt3);
                for (int k = 0; k < kBlockPixels; ++k)
                    picture.fill[(by * kBlockSide + static_cast<size_t>(k / kBlockSide)) * static_cast<size_t>(width) + bx * kBlockSide +
                                 static_cast<size_t>(k % kBlockSide)] = fill[static_cast<size_t>(k)];
            }
        return {std::move(picture), {}};
    }

    Read<std::array<GlyphShape, kGlyphCount>> ReadGlyphShapes(std::span<const uint8_t> body, std::string_view picture, int width,
        int height)
    {
        const auto refused = [](std::string why) { return Read<std::array<GlyphShape, kGlyphCount>>{std::nullopt, std::move(why)}; };
        if (body.size() <= kPicturesCountAt) return refused("its shapes end early");
        size_t at = kPicturesCountAt + 1 + body[kPicturesCountAt] * kNameBytes;
        if (body.size() < at + sizeof(uint16_t)) return refused("its shapes end early");
        if (ReadAt<uint16_t>(body.data(), at) < kGlyphCount) return refused("it has too few shapes");
        at += sizeof(uint16_t);
        std::array<GlyphShape, kGlyphCount> glyphs{};
        for (size_t i = 0; i < kGlyphCount; ++i)
        {
            if (i > 0)
            {
                if (body.size() < at + kShapeOpenBytes + kNameBytes) return refused("its shapes end early");
                if (std::string_view(reinterpret_cast<const char*>(body.data() + at + kShapeOpenBytes), kNameBytes) != picture)
                    return refused("a letter's shape draws from another picture");
                at += kShapeOpenBytes + kNameBytes;
            }
            if (body.size() < at + kShapeBytes) return refused("its shapes end early");
            if (body[at] != kShapeFlag || body[at + kShapeBytes - kShapeColorBytes - 1] != kShapeEnd) return refused("a letter's shape is not one");
            const auto value = [&](size_t v) { return static_cast<int>(ReadAt<int16_t>(body.data(), at + 1 + v * sizeof(int16_t))); };
            GlyphShape& g = glyphs[i];
            g             = GlyphShape{value(kLeft), value(kTop), value(kBottomRightX), value(kBottomRightY), value(kRectU),
                            value(kRectV), value(kRectWidth), value(kRectHeight)};
            if (g.right <= g.left || g.bottom <= g.top || g.width <= 0 || g.height <= 0 || g.u < 0 || g.v < 0 ||
                g.u + g.width > width || g.v + g.height > height)
                return refused("a letter's shape is out of its picture");
            at += kShapeBytes;
        }
        return {glyphs, {}};
    }

    Read<GameFontFace> ReadGameFont(std::span<const uint8_t> dat)
    {
        const auto pictureChunk = FindChunk(dat, kFontChunk, kPictureChunkType);
        const auto shapesChunk  = FindChunk(dat, kFontChunk, kShapesChunkType);
        if (!pictureChunk || !shapesChunk) return {std::nullopt, "it holds no font"};
        Read<FontPicture> picture = ReadFontPicture(*pictureChunk);
        if (!picture.value) return {std::nullopt, std::move(picture.failure)};
        auto shapes = ReadGlyphShapes(*shapesChunk, picture.value->name, picture.value->width, picture.value->height);
        if (!shapes.value) return {std::nullopt, std::move(shapes.failure)};
        return {GameFontFace{std::move(*picture.value), *shapes.value}, {}};
    }

    std::vector<std::string> GameFontCandidates(std::string_view pivotIni, std::string_view defaultRoot, std::string_view gameFolder)
    {
        std::string root = IniValue(pivotIni, kPivotSettings, kPivotRoot).value_or(std::string());
        if (root.empty()) root = defaultRoot;
        std::vector<std::string> files;
        for (int number = 0;; ++number)
        {
            const std::optional<std::string> overlay = IniValue(pivotIni, kPivotOverlays, std::to_string(number));
            if (!overlay || overlay->empty()) break;
            files.push_back(Under(Under(root, *overlay), kFontFile));
        }
        files.push_back(Under(gameFolder, kFontFile));
        return files;
    }
}
