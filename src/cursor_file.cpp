#include "cursor_file.h"

#include "bytes.h"

#include <cmath>
#include <cstring>
#include <optional>

namespace headsup
{
    namespace
    {
#include "generated/pointer.inc"

        constexpr double kJiffiesPerSecond = 60.0;
        constexpr size_t kBytesPerPixel    = 4;
        constexpr size_t kRgbSize          = 3; // blue, green, red: a 24-bit pixel, or a palette color without its pad byte
        constexpr size_t kPaletteEntrySize = 4;
        constexpr uint32_t kMaxCursorSide  = 256; // a cursor file's directory gives each side in a byte, 0 meaning 256

        constexpr size_t kDirectoryType  = 2;
        constexpr size_t kDirectoryCount = 4;
        constexpr size_t kDirectorySize  = 6;
        constexpr uint16_t kCursorType   = 2; // icons are 1
        constexpr size_t kEntryHotX      = 4;
        constexpr size_t kEntryHotY      = 6;
        constexpr size_t kEntryBytes     = 8;
        constexpr size_t kEntryOffset    = 12;
        constexpr size_t kEntrySize      = 16;

        constexpr uint32_t kInfoHeaderSize = 40; // BITMAPINFOHEADER
        constexpr size_t kDibWidth         = 4;
        constexpr size_t kDibHeight        = 8;
        constexpr size_t kDibBitCount      = 14;
        constexpr size_t kDibCompression   = 16;
        constexpr size_t kDibColorsUsed    = 32;
        constexpr uint32_t kUncompressed   = 0; // BI_RGB

        constexpr size_t kIdSize             = 4; // a chunk's or a form's four letters
        constexpr size_t kChunkHeader        = kIdSize + sizeof(uint32_t);
        constexpr size_t kRiffHeaderSize     = kChunkHeader + kIdSize;
        constexpr size_t kAniHeaderSize      = 36;
        constexpr size_t kAniRate            = 28;
        constexpr size_t kAniFlags           = 32;
        constexpr uint32_t kFramesAreCursors = 1; // AF_ICON

        bool Is(const uint8_t* at, const char* id) { return std::memcmp(at, id, kIdSize) == 0; }

        bool IsOpaque(const uint8_t* pixel) { return pixel[3] != 0; }

        // DIB rows are padded to whole 32-bit words.
        size_t RowBytes(uint32_t width, uint32_t bpp) { return (static_cast<size_t>(width) * bpp + 31) / 32 * 4; }

        std::optional<CursorFrame> ReadFirstCursor(const uint8_t* data, size_t size)
        {
            if (data == nullptr || size < kDirectorySize + kEntrySize) return std::nullopt;
            if (ReadAt<uint16_t>(data, kDirectoryType) != kCursorType || ReadAt<uint16_t>(data, kDirectoryCount) == 0)
                return std::nullopt;
            const uint8_t* entry  = data + kDirectorySize;
            const uint16_t hotX   = ReadAt<uint16_t>(entry, kEntryHotX);
            const uint16_t hotY   = ReadAt<uint16_t>(entry, kEntryHotY);
            const uint32_t length = ReadAt<uint32_t>(entry, kEntryBytes);
            const uint32_t offset = ReadAt<uint32_t>(entry, kEntryOffset);
            if (offset > size || length > size - offset || length < kInfoHeaderSize) return std::nullopt;
            const uint8_t* dib    = data + offset;
            const auto headerSize = ReadAt<uint32_t>(dib, 0);
            const auto width      = ReadAt<int32_t>(dib, kDibWidth);
            const auto doubled    = ReadAt<int32_t>(dib, kDibHeight); // the color image, then the mask
            const auto bpp        = ReadAt<uint16_t>(dib, kDibBitCount);
            const uint32_t used   = ReadAt<uint32_t>(dib, kDibColorsUsed);
            if (headerSize != kInfoHeaderSize || ReadAt<uint32_t>(dib, kDibCompression) != kUncompressed) return std::nullopt;
            if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24) return std::nullopt;
            if (width <= 0 || doubled <= 0) return std::nullopt;
            const auto w = static_cast<uint32_t>(width), h = static_cast<uint32_t>(doubled) / 2;
            const bool paletted = bpp <= 8;
            // Bounded before the sizes below are worked out, which in the 32-bit plugin could otherwise wrap.
            if (w > kMaxCursorSide || h > kMaxCursorSide || (paletted && used > (size_t{1} << bpp))) return std::nullopt;
            const size_t colors     = paletted ? (used != 0 ? used : size_t{1} << bpp) : 0;
            const size_t stride     = RowBytes(w, bpp);
            const size_t maskStride = RowBytes(w, 1);
            const size_t pixelsAt   = headerSize + colors * kPaletteEntrySize;
            const size_t maskAt     = pixelsAt + stride * h;
            if (maskAt + maskStride * h > length) return std::nullopt;

            CursorFrame frame;
            CursorImage& image = frame.image;
            image.width        = w;
            image.height       = h;
            image.hotX         = hotX;
            image.hotY         = hotY;
            image.bgra.resize(static_cast<size_t>(w) * h * kBytesPerPixel);
            for (uint32_t y = 0; y < h; ++y)
            {
                const uint8_t* row  = dib + pixelsAt + stride * (h - 1 - y); // rows are kept bottom up
                const uint8_t* mask = dib + maskAt + maskStride * (h - 1 - y);
                for (uint32_t x = 0; x < w; ++x)
                {
                    uint8_t* out = image.bgra.data() + (static_cast<size_t>(y) * w + x) * kBytesPerPixel;
                    if (!paletted)
                        std::memcpy(out, row + x * kRgbSize, kRgbSize);
                    else
                    {
                        const size_t bit   = static_cast<size_t>(x) * bpp;
                        const size_t index = row[bit / 8] >> (8 - bpp - bit % 8) & ((1u << bpp) - 1);
                        if (index >= colors) return std::nullopt;
                        std::memcpy(out, dib + headerSize + index * kPaletteEntrySize, kRgbSize);
                    }
                    out[3] = (mask[x / 8] >> (7 - x % 8) & 1) != 0 ? 0 : 255;
                }
            }
            frame.resource.resize(sizeof(hotX) + sizeof(hotY) + length);
            std::memcpy(frame.resource.data(), &hotX, sizeof(hotX));
            std::memcpy(frame.resource.data() + sizeof(hotX), &hotY, sizeof(hotY));
            std::memcpy(frame.resource.data() + sizeof(hotX) + sizeof(hotY), dib, length);
            return frame;
        }
    }

    std::vector<CursorFrame> ReadCursorFile(const uint8_t* data, size_t size)
    {
        if (data == nullptr || size < kRiffHeaderSize) return {};
        if (!Is(data, "RIFF"))
        {
            auto frame = ReadFirstCursor(data, size);
            return frame ? std::vector<CursorFrame>{*frame} : std::vector<CursorFrame>{};
        }
        if (!Is(data + kChunkHeader, "ACON")) return {};
        // Some files give the RIFF size as the whole file's: read to whichever ends first.
        const size_t end = std::min(size, kChunkHeader + static_cast<size_t>(ReadAt<uint32_t>(data, kIdSize)));
        uint32_t defaultJiffies = 0, flags = 0;
        std::vector<uint32_t> rates;
        std::vector<CursorFrame> frames;
        bool header = false;
        auto walk = [&](size_t at, size_t stop, auto& self) -> bool {
            while (at + kChunkHeader <= stop)
            {
                const uint8_t* chunk = data + at;
                const size_t length  = ReadAt<uint32_t>(chunk, kIdSize);
                if (length > stop - at - kChunkHeader) return false;
                const uint8_t* body = chunk + kChunkHeader;
                if (Is(chunk, "anih") && length >= kAniHeaderSize)
                {
                    defaultJiffies = ReadAt<uint32_t>(body, kAniRate);
                    flags          = ReadAt<uint32_t>(body, kAniFlags);
                    header         = true;
                }
                else if (Is(chunk, "rate"))
                    for (size_t i = 0; i + sizeof(uint32_t) <= length; i += sizeof(uint32_t))
                        rates.push_back(ReadAt<uint32_t>(body, i));
                else if (Is(chunk, "LIST") && length >= kIdSize && Is(body, "fram"))
                {
                    if (!self(at + kChunkHeader + kIdSize, at + kChunkHeader + length, self)) return false;
                }
                else if (Is(chunk, "icon"))
                {
                    auto frame = ReadFirstCursor(body, length);
                    if (!frame) return false;
                    frames.push_back(std::move(*frame));
                }
                at += kChunkHeader + length + (length & 1); // chunks are padded to an even length
            }
            return true;
        };
        if (!walk(kRiffHeaderSize, end, walk) || !header || (flags & kFramesAreCursors) == 0 || frames.empty()) return {};
        for (size_t i = 0; i < frames.size(); ++i)
            frames[i].seconds = static_cast<double>(i < rates.size() ? rates[i] : defaultJiffies) / kJiffiesPerSecond;
        return frames;
    }

    size_t FrameAt(const std::vector<CursorFrame>& frames, double seconds)
    {
        double total = 0.0;
        for (const CursorFrame& f : frames)
            total += f.seconds;
        if (!(total > 0.0)) return 0;
        double t = std::fmod(seconds, total);
        for (size_t i = 0; i < frames.size(); ++i)
        {
            if (t < frames[i].seconds) return i;
            t -= frames[i].seconds;
        }
        return frames.size() - 1;
    }

    CursorImage ImageFromBitmaps(uint32_t width, uint32_t height, uint16_t hotX, uint16_t hotY, const std::vector<uint8_t>& color,
        const std::vector<uint8_t>& mask)
    {
        CursorImage image{width, height, hotX, hotY, color};
        image.bgra.resize(static_cast<size_t>(width) * height * kBytesPerPixel);
        for (size_t p = 0; p * kBytesPerPixel < image.bgra.size(); ++p)
        {
            const size_t at        = p * kBytesPerPixel;
            const bool transparent = at + 2 < mask.size() && (mask[at] | mask[at + 1] | mask[at + 2]) != 0;
            image.bgra[at + 3]     = transparent ? 0 : 255;
        }
        return image;
    }

    bool SameCursor(const CursorImage& a, const CursorImage& b)
    {
        if (a.width != b.width || a.height != b.height || a.hotX != b.hotX || a.hotY != b.hotY) return false;
        if (a.bgra.size() != b.bgra.size()) return false;
        for (size_t at = 0; at < a.bgra.size(); at += kBytesPerPixel)
        {
            const bool shown = IsOpaque(a.bgra.data() + at);
            if (shown != IsOpaque(b.bgra.data() + at)) return false;
            if (shown && std::memcmp(a.bgra.data() + at, b.bgra.data() + at, kRgbSize) != 0) return false;
        }
        return true;
    }

    std::vector<CursorFrame> ChocoboPointer() { return ReadCursorFile(kChocoboPointerFile, sizeof(kChocoboPointerFile)); }
}
