#include "game_font.h"

#include "bytes.h"
#include "fonts.h"
#include "ini_text.h"
#include "plate_settings.h"
#include "utf16.h"

#include <algorithm>
#include <cmath>
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

        constexpr char32_t kUnknownGlyph = U'?';
        constexpr float kUnderlineUnits  = 10.5f; // a unit under the capitals' baseline, at 9.6
        constexpr float kUnderlineThick  = 1.0f;
        constexpr float kBoldUnits       = 0.5f;  // about a pixel of the picture
        constexpr float kSharpEdgePixels = 1.0f;
        constexpr float kFullCoverage    = 255.0f;

        struct Scale
        {
            float x, y; // pixels a unit
        };
        Scale ScaleOf(GameFontVariant variant, int pixelHeight)
        {
            const float y = static_cast<float>(pixelHeight) / kEmUnits;
            return {variant == GameFontVariant::Game ? y * kGameStretch : y, y};
        }

        int LineHeight(Scale scale) { return static_cast<int>(std::lround(kLineUnits * scale.y)); }

        // Units from the run's start to each glyph, and to its end after the last.
        std::vector<int> Pens(const GameFontFace& face, const std::u32string& codes)
        {
            std::vector<int> pens{0};
            for (const char32_t code : codes)
                pens.push_back(pens.back() + face.glyphs[GlyphOf(code)].right);
            return pens;
        }

        // The picture's pixels a tent filter takes for one pixel drawn: centered at s in the picture's pixels, reaching
        // radius of them, with their weights and the sum of every weight, outside the glyph's rectangle included.
        struct Taps
        {
            int first = 0;
            std::vector<float> weights;
            float total = 0.0f;
        };
        Taps TapsAt(float s, float radius)
        {
            Taps taps;
            taps.first = static_cast<int>(std::floor(s - radius));
            for (int i = taps.first; i <= static_cast<int>(std::ceil(s + radius)); ++i)
            {
                const float weight = std::max(0.0f, 1.0f - std::abs(static_cast<float>(i) + 0.5f - s) / radius);
                taps.weights.push_back(weight);
                taps.total += weight;
            }
            return taps;
        }

        // The brightest of a glyph's pixels: the font keeps its small letters, the punctuation, at about 60%.
        uint8_t Peak(const FontPicture& picture, const GlyphShape& g)
        {
            uint8_t peak = 0;
            for (int v = g.v; v < g.v + g.height; ++v)
            {
                const uint8_t* line = picture.fill.data() + static_cast<size_t>(v) * static_cast<size_t>(picture.width);
                peak                = std::max(peak, *std::max_element(line + g.u, line + g.u + g.width));
            }
            return peak;
        }

        // A glyph's rectangle of the picture drawn into the box from (x0, y0) to (x1, y1) of the coverage, its brightest
        // pixel full, each pixel the higher of what it had and the glyph.
        void DrawGlyph(Coverage& out, const FontPicture& picture, const GlyphShape& g, float x0, float y0, float x1, float y1)
        {
            const uint8_t peak = Peak(picture, g);
            if (peak == 0) return;
            const float fx = (x1 - x0) / static_cast<float>(g.width), fy = (y1 - y0) / static_cast<float>(g.height);
            const float rx = std::max(1.0f, 1.0f / fx), ry = std::max(1.0f, 1.0f / fy);
            const float magnification = std::min(fx, fy);
            const int left = std::max(0, static_cast<int>(std::floor(x0 - fx))), right = std::min(out.width, static_cast<int>(std::ceil(x1 + fx)));
            const int top = std::max(0, static_cast<int>(std::floor(y0 - fy))), bottom = std::min(out.height, static_cast<int>(std::ceil(y1 + fy)));
            std::vector<Taps> columns;
            for (int x = left; x < right; ++x)
                columns.push_back(TapsAt((static_cast<float>(x) + 0.5f - x0) / fx, rx));
            for (int y = top; y < bottom; ++y)
            {
                const Taps rows = TapsAt((static_cast<float>(y) + 0.5f - y0) / fy, ry);
                for (int x = left; x < right; ++x)
                {
                    const Taps& cols = columns[static_cast<size_t>(x - left)];
                    float sum        = 0.0f;
                    for (size_t j = 0; j < rows.weights.size(); ++j)
                    {
                        const int v = rows.first + static_cast<int>(j);
                        if (v < 0 || v >= g.height || rows.weights[j] == 0.0f) continue;
                        const uint8_t* line = picture.fill.data() + static_cast<size_t>(g.v + v) * static_cast<size_t>(picture.width) + g.u;
                        for (size_t i = 0; i < cols.weights.size(); ++i)
                        {
                            const int u = cols.first + static_cast<int>(i);
                            if (u >= 0 && u < g.width) sum += rows.weights[j] * cols.weights[i] * line[u];
                        }
                    }
                    const uint8_t value = Sharpened(sum / (rows.total * cols.total) / static_cast<float>(peak), magnification);
                    uint8_t& pixel      = out.alpha[static_cast<size_t>(y) * static_cast<size_t>(out.width) + static_cast<size_t>(x)];
                    pixel               = std::max(pixel, value);
                }
            }
        }

        // Leans every row of the line right by its share of lean: the top row all of it, the bottom row none.
        void Lean(Coverage& out, int lean, int lineHeight, int margin)
        {
            const int last = std::max(1, lineHeight - 1);
            for (int y = 0; y < out.height; ++y)
            {
                const int row   = std::clamp(y - margin, 0, last);
                const int shift = static_cast<int>(std::lround(static_cast<float>(lean * (last - row)) / static_cast<float>(last)));
                if (shift == 0) continue;
                uint8_t* line = out.alpha.data() + static_cast<size_t>(y) * static_cast<size_t>(out.width);
                std::copy_backward(line, line + out.width - shift, line + out.width);
                std::fill(line, line + shift, uint8_t{0});
            }
        }

        uint32_t Channel8(uint32_t value, uint32_t max) { return value * kFull / max; }

        // The brightest channel of a 565 color: the picture's letters are white and their edge black.
        uint32_t Brightness(uint16_t color)
        {
            return std::max({Channel8(color >> kRedShift & kFiveBits, kFiveBits), Channel8(color >> kGreenShift & kSixBits, kSixBits),
                Channel8(color & kFiveBits, kFiveBits)});
        }

        // The pearl before a name is the square the font's shapes draw the linkshell icon from, in the status
        // icons' picture.
        constexpr std::string_view kStatusChunk   = "usta";
        constexpr std::string_view kStatusPicture = "menu    ustatshd";
        constexpr int kPearlU = 0, kPearlV = 64, kPearlSourceSide = 32;
        constexpr size_t kBgraBytes = 4;

        // What a picture chunk's header says, or why it is no picture.
        struct PictureHeader
        {
            std::string name;
            int width = 0, height = 0;
            bool dxt3 = false;
            size_t blocksWide = 0, blocksTall = 0, blockBytes = 0;
        };
        Read<PictureHeader> ReadPictureHeader(std::span<const uint8_t> body)
        {
            if (body.size() < kBlocksAt || body[0] != kPictureFlag || ReadAt<uint32_t>(body.data(), kInfoHeaderAt) != kInfoHeaderBytes)
                return {std::nullopt, "its picture is not one"};
            const std::string_view code(reinterpret_cast<const char*>(body.data() + kCodeAt), kDxt3Code.size());
            if (code != kDxt3Code && code != kDxt1Code) return {std::nullopt, "its picture is not DXT1 or DXT3"};
            PictureHeader header;
            header.dxt3   = code == kDxt3Code;
            header.width  = ReadAt<int32_t>(body.data(), kWidthAt);
            header.height = ReadAt<int32_t>(body.data(), kHeightAt);
            if (header.width <= 0 || header.height <= 0 || header.width > kMaxPictureSide || header.height > kMaxPictureSide ||
                header.width % kBlockSide != 0 || header.height % kBlockSide != 0)
                return {std::nullopt, "its picture is " + std::to_string(header.width) + " x " + std::to_string(header.height)};
            header.blocksWide = static_cast<size_t>(header.width / kBlockSide);
            header.blocksTall = static_cast<size_t>(header.height / kBlockSide);
            header.blockBytes = header.dxt3 ? kDxt3BlockBytes : kDxt1BlockBytes;
            if (body.size() - kBlocksAt < header.blocksWide * header.blocksTall * header.blockBytes)
                return {std::nullopt, "its picture ends early"};
            header.name.assign(reinterpret_cast<const char*>(body.data() + kNameAt), kNameBytes);
            return {std::move(header), {}};
        }

        // Where a block's pixel k goes in a picture width pixels wide.
        size_t PixelOf(const PictureHeader& header, size_t bx, size_t by, int k)
        {
            return (by * kBlockSide + static_cast<size_t>(k / kBlockSide)) * static_cast<size_t>(header.width) + bx * kBlockSide +
                   static_cast<size_t>(k % kBlockSide);
        }

        // A block's alphas: DXT3's four bits each, DXT1's full.
        std::array<uint32_t, kBlockPixels> BlockAlphas(const uint8_t* block, bool dxt3)
        {
            std::array<uint32_t, kBlockPixels> alpha{};
            alpha.fill(kFull);
            if (dxt3)
                for (int k = 0; k < kBlockPixels; ++k)
                    alpha[static_cast<size_t>(k)] = (block[k / 2] >> (k % 2 * kAlphaBits) & kAlphaMask) * kAlphaScale;
            return alpha;
        }

        // Each pixel of a block: its brightness times its alpha.
        std::array<uint8_t, kBlockPixels> BlockFill(const uint8_t* block, bool dxt3)
        {
            std::array<uint32_t, kBlockPixels> alpha = BlockAlphas(block, dxt3);
            if (dxt3) block += kAlphaBytes;
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

        // Each pixel of a block in its colors, blue, green, red and alpha; DXT1's clear pick is clear black.
        std::array<std::array<uint8_t, kBgraBytes>, kBlockPixels> BlockColors(const uint8_t* block, bool dxt3)
        {
            const std::array<uint32_t, kBlockPixels> alpha = BlockAlphas(block, dxt3);
            if (dxt3) block += kAlphaBytes;
            const auto c0 = ReadAt<uint16_t>(block, 0), c1 = ReadAt<uint16_t>(block, kSecondColorAt);
            const auto bgr = [](uint16_t c) {
                return std::array<uint32_t, 3>{Channel8(c & kFiveBits, kFiveBits), Channel8(c >> kGreenShift & kSixBits, kSixBits),
                    Channel8(c >> kRedShift & kFiveBits, kFiveBits)};
            };
            const std::array<uint32_t, 3> a = bgr(c0), b = bgr(c1);
            const bool fourColors = dxt3 || c0 > c1;
            std::array<std::array<uint32_t, 3>, 4> colors{a, b, a, b};
            for (size_t c = 0; c < 3; ++c)
            {
                colors[2][c] = fourColors ? (2 * a[c] + b[c]) / 3 : (a[c] + b[c]) / 2;
                colors[3][c] = fourColors ? (a[c] + 2 * b[c]) / 3 : 0;
            }
            const auto picks = ReadAt<uint32_t>(block, kPicksAt);
            std::array<std::array<uint8_t, kBgraBytes>, kBlockPixels> out{};
            for (int k = 0; k < kBlockPixels; ++k)
            {
                const uint32_t pick = picks >> (k * kPickBits) & kPickMask;
                const auto& color   = colors[pick];
                out[static_cast<size_t>(k)] = {static_cast<uint8_t>(color[0]), static_cast<uint8_t>(color[1]), static_cast<uint8_t>(color[2]),
                    static_cast<uint8_t>(!fourColors && pick == kClearPick ? 0 : alpha[static_cast<size_t>(k)])};
            }
            return out;
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

    size_t GlyphOf(char32_t code)
    {
        return code >= kFirstGlyph && code <= kLastGlyph ? code - kFirstGlyph : kUnknownGlyph - kFirstGlyph;
    }

    void GameFontExtent(const GameFontFace& face, GameFontVariant variant, const std::string& utf8, int pixelHeight,
        uint8_t style, int& width, int& height)
    {
        const Scale scale = ScaleOf(variant, pixelHeight);
        width  = static_cast<int>(std::lround(static_cast<float>(Pens(face, CodePoints(utf8)).back()) * scale.x)) + ItalicLean(pixelHeight, style);
        height = LineHeight(scale);
    }

    Coverage GameFontRasterize(const GameFontFace& face, GameFontVariant variant, const std::string& utf8, int pixelHeight,
        uint8_t style, int tracking, int margin)
    {
        const std::u32string codes  = CodePoints(utf8);
        const Scale scale           = ScaleOf(variant, pixelHeight);
        const std::vector<int> pens = Pens(face, codes);
        const int count             = static_cast<int>(codes.size());
        const int lean              = ItalicLean(pixelHeight, style);
        const int lineHeight        = LineHeight(scale);
        Coverage out;
        out.width  = static_cast<int>(std::lround(static_cast<float>(pens.back()) * scale.x)) + tracking * count + lean + 2 * margin;
        out.height = lineHeight + 2 * margin;
        out.alpha.assign(static_cast<size_t>(out.width) * static_cast<size_t>(out.height), 0);
        const int bold = HasStyle(style, FontStyle::Bold) ? std::max(1, static_cast<int>(std::lround(scale.x * kBoldUnits))) : 0;
        for (int i = 0; i < count; ++i)
        {
            const GlyphShape& g = face.glyphs[GlyphOf(codes[static_cast<size_t>(i)])];
            const auto origin   = static_cast<float>(margin + std::lround(static_cast<float>(pens[static_cast<size_t>(i)]) * scale.x) + tracking * i);
            const float y0 = static_cast<float>(margin) + static_cast<float>(g.top) * scale.y;
            const float y1 = static_cast<float>(margin) + static_cast<float>(g.bottom) * scale.y;
            for (int strike = 0; strike <= bold; strike += std::max(1, bold))
                DrawGlyph(out, face.picture, g, origin + static_cast<float>(strike) + static_cast<float>(g.left) * scale.x, y0,
                    origin + static_cast<float>(strike) + static_cast<float>(g.right) * scale.x, y1);
        }
        if (lean > 0) Lean(out, lean, lineHeight, margin);
        return out;
    }

    void GameFontUnderlineBand(int pixelHeight, int& top, int& thickness)
    {
        const float unit = static_cast<float>(pixelHeight) / kEmUnits;
        top              = static_cast<int>(std::lround(kUnderlineUnits * unit));
        thickness        = std::max(1, static_cast<int>(std::lround(kUnderlineThick * unit)));
    }

    uint8_t Sharpened(float coverage, float magnification)
    {
        if (magnification > 1.0f) coverage = (coverage - 0.5f) * magnification / kSharpEdgePixels + 0.5f;
        return static_cast<uint8_t>(std::lround(std::clamp(coverage, 0.0f, 1.0f) * kFullCoverage));
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
        Read<PictureHeader> header = ReadPictureHeader(body);
        if (!header.value) return {std::nullopt, std::move(header.failure)};
        const PictureHeader& h = *header.value;
        FontPicture picture{h.name, h.width, h.height, std::vector<uint8_t>(static_cast<size_t>(h.width) * static_cast<size_t>(h.height))};
        for (size_t by = 0; by < h.blocksTall; ++by)
            for (size_t bx = 0; bx < h.blocksWide; ++bx)
            {
                const auto fill = BlockFill(body.data() + kBlocksAt + (by * h.blocksWide + bx) * h.blockBytes, h.dxt3);
                for (int k = 0; k < kBlockPixels; ++k)
                    picture.fill[PixelOf(h, bx, by, k)] = fill[static_cast<size_t>(k)];
            }
        return {std::move(picture), {}};
    }

    Read<ColorPicture> ReadColorPicture(std::span<const uint8_t> body)
    {
        Read<PictureHeader> header = ReadPictureHeader(body);
        if (!header.value) return {std::nullopt, std::move(header.failure)};
        const PictureHeader& h = *header.value;
        ColorPicture picture{h.name, h.width, h.height,
            std::vector<uint8_t>(static_cast<size_t>(h.width) * static_cast<size_t>(h.height) * kBgraBytes)};
        for (size_t by = 0; by < h.blocksTall; ++by)
            for (size_t bx = 0; bx < h.blocksWide; ++bx)
            {
                const auto colors = BlockColors(body.data() + kBlocksAt + (by * h.blocksWide + bx) * h.blockBytes, h.dxt3);
                for (int k = 0; k < kBlockPixels; ++k)
                    std::copy(colors[static_cast<size_t>(k)].begin(), colors[static_cast<size_t>(k)].end(),
                        picture.bgra.begin() + static_cast<std::ptrdiff_t>(PixelOf(h, bx, by, k) * kBgraBytes));
            }
        return {std::move(picture), {}};
    }

    Read<ColorPicture> ReadGamePearl(std::span<const uint8_t> dat)
    {
        const auto chunk = FindChunk(dat, kStatusChunk, kPictureChunkType);
        if (!chunk) return {std::nullopt, "it holds no status icons"};
        Read<ColorPicture> sheet = ReadColorPicture(*chunk);
        if (!sheet.value) return sheet;
        if (sheet.value->name != kStatusPicture) return {std::nullopt, "its status icons are named " + sheet.value->name};
        if (sheet.value->width < kPearlU + kPearlSourceSide || sheet.value->height < kPearlV + kPearlSourceSide)
            return {std::nullopt, "its status icons are too small to hold the pearl"};
        const size_t row = static_cast<size_t>(kPearlSourceSide) * kBgraBytes;
        ColorPicture pearl{sheet.value->name, kPearlSourceSide, kPearlSourceSide, {}};
        pearl.bgra.reserve(row * kPearlSourceSide);
        for (int y = 0; y < kPearlSourceSide; ++y)
        {
            const auto from = sheet.value->bgra.begin() +
                              static_cast<std::ptrdiff_t>((static_cast<size_t>(kPearlV + y) * static_cast<size_t>(sheet.value->width) + kPearlU) * kBgraBytes);
            pearl.bgra.insert(pearl.bgra.end(), from, from + static_cast<std::ptrdiff_t>(row));
        }
        bool seen = false;
        for (size_t p = kBgraBytes - 1; p < pearl.bgra.size(); p += kBgraBytes)
            seen = seen || pearl.bgra[p] != 0;
        if (!seen) return {std::nullopt, "its pearl shows nothing"};
        return {std::move(pearl), {}};
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
