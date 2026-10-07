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
        constexpr uint16_t kCursorType     = 2;  // in a cursor file's header; icons are 1
        constexpr size_t kDirectorySize    = 6;  // reserved, type, count
        constexpr size_t kEntrySize        = 16; // width, height, colors, reserved, hotspot x, y, image size, offset
        constexpr uint32_t kInfoHeaderSize = 40; // BITMAPINFOHEADER
        constexpr uint32_t kUncompressed   = 0;  // BI_RGB
        constexpr uint32_t kFramesAreCursors = 1; // the animated cursor header's AF_ICON flag
        constexpr size_t kChunkHeader      = 8;  // four-letter id, then size

        bool Is(const uint8_t* at, const char* id) { return std::memcmp(at, id, 4) == 0; }

        bool Opaque(const uint8_t* pixel) { return pixel[3] != 0; }

        // One frame from a cursor file: its first image.
        std::optional<CursorFrame> ReadCursor(const uint8_t* data, size_t size)
        {
            if (data == nullptr || size < kDirectorySize + kEntrySize) return std::nullopt;
            if (ReadAt<uint16_t>(data, 2) != kCursorType || ReadAt<uint16_t>(data, 4) == 0) return std::nullopt;
            const uint16_t hotX = ReadAt<uint16_t>(data, kDirectorySize + 4), hotY = ReadAt<uint16_t>(data, kDirectorySize + 6);
            const uint32_t length = ReadAt<uint32_t>(data, kDirectorySize + 8), offset = ReadAt<uint32_t>(data, kDirectorySize + 12);
            if (offset > size || length > size - offset || length < kInfoHeaderSize) return std::nullopt;
            const uint8_t* dib = data + offset;
            const auto headerSize = ReadAt<uint32_t>(dib, 0);
            const auto width = ReadAt<int32_t>(dib, 4), doubled = ReadAt<int32_t>(dib, 8);
            const auto bpp = ReadAt<uint16_t>(dib, 14);
            if (headerSize != kInfoHeaderSize || ReadAt<uint32_t>(dib, 16) != kUncompressed || width <= 0 || doubled <= 0) return std::nullopt;
            if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) return std::nullopt;
            const auto w = static_cast<uint32_t>(width), h = static_cast<uint32_t>(doubled) / 2; // the color, then the mask
            const uint32_t used = ReadAt<uint32_t>(dib, 32);
            const size_t colors = bpp <= 8 ? (used != 0 ? used : size_t{1} << bpp) : 0;
            const size_t stride = (static_cast<size_t>(w) * bpp + 31) / 32 * 4, maskStride = (w + 31) / 32 * 4;
            const size_t paletteAt = headerSize, pixelsAt = paletteAt + colors * 4, maskAt = pixelsAt + stride * h;
            if (maskAt + maskStride * h > length) return std::nullopt;

            CursorFrame frame;
            CursorImage& image = frame.image;
            image.width = w, image.height = h, image.hotX = hotX, image.hotY = hotY;
            image.bgra.resize(static_cast<size_t>(w) * h * kBytesPerPixel);
            bool anyAlpha = false;
            for (uint32_t y = 0; y < h; ++y)
            {
                const uint8_t* row  = dib + pixelsAt + stride * (h - 1 - y); // rows are kept bottom up
                const uint8_t* mask = dib + maskAt + maskStride * (h - 1 - y);
                for (uint32_t x = 0; x < w; ++x)
                {
                    uint8_t* out = image.bgra.data() + (static_cast<size_t>(y) * w + x) * kBytesPerPixel;
                    if (bpp == 32)
                        std::memcpy(out, row + x * 4, 4), anyAlpha |= out[3] != 0;
                    else if (bpp == 24)
                        std::memcpy(out, row + x * 3, 3);
                    else
                    {
                        const size_t bit   = static_cast<size_t>(x) * bpp;
                        const size_t index = row[bit / 8] >> (8 - bpp - bit % 8) & ((1u << bpp) - 1);
                        if (index >= colors) return std::nullopt;
                        std::memcpy(out, dib + paletteAt + index * 4, 3);
                    }
                    if (bpp != 32) out[3] = (mask[x / 8] >> (7 - x % 8) & 1) != 0 ? 0 : 255;
                }
            }
            if (bpp == 32 && !anyAlpha) // an alpha channel left empty: the mask says what shows
            {
                for (uint32_t y = 0; y < h; ++y)
                    for (uint32_t x = 0; x < w; ++x)
                        image.bgra[(static_cast<size_t>(y) * w + x) * kBytesPerPixel + 3] =
                            (dib[maskAt + maskStride * (h - 1 - y) + x / 8] >> (7 - x % 8) & 1) != 0 ? 0 : 255;
            }
            frame.resource.resize(4 + length);
            std::memcpy(frame.resource.data(), &hotX, 2);
            std::memcpy(frame.resource.data() + 2, &hotY, 2);
            std::memcpy(frame.resource.data() + 4, dib, length);
            return frame;
        }
    }

    std::vector<CursorFrame> ReadCursorFile(const uint8_t* data, size_t size)
    {
        if (data == nullptr || size < 12) return {};
        if (!Is(data, "RIFF"))
        {
            auto frame = ReadCursor(data, size);
            return frame ? std::vector<CursorFrame>{*frame} : std::vector<CursorFrame>{};
        }
        if (!Is(data + 8, "ACON")) return {};
        // Some files give the RIFF size as the whole file's: read to whichever ends first.
        const size_t end = std::min(size, kChunkHeader + static_cast<size_t>(ReadAt<uint32_t>(data, 4)));
        uint32_t defaultJiffies = 0, flags = 0;
        std::vector<uint32_t> rates;
        std::vector<CursorFrame> frames;
        bool header = false;
        auto walk = [&](size_t at, size_t stop, auto& self) -> bool {
            while (at + kChunkHeader <= stop)
            {
                const uint8_t* chunk = data + at;
                const size_t length  = ReadAt<uint32_t>(chunk, 4);
                if (length > stop - at - kChunkHeader) return false;
                const uint8_t* body = chunk + kChunkHeader;
                if (Is(chunk, "anih") && length >= 36)
                    defaultJiffies = ReadAt<uint32_t>(body, 28), flags = ReadAt<uint32_t>(body, 32), header = true;
                else if (Is(chunk, "rate"))
                    for (size_t i = 0; i + 4 <= length; i += 4)
                        rates.push_back(ReadAt<uint32_t>(body, i));
                else if (Is(chunk, "LIST") && length >= 4 && Is(body, "fram"))
                {
                    if (!self(at + kChunkHeader + 4, at + kChunkHeader + length, self)) return false;
                }
                else if (Is(chunk, "icon"))
                {
                    auto frame = ReadCursor(body, length);
                    if (!frame) return false;
                    frames.push_back(std::move(*frame));
                }
                at += kChunkHeader + length + (length & 1);
            }
            return true;
        };
        if (!walk(12, end, walk) || !header || (flags & kFramesAreCursors) == 0 || frames.empty()) return {};
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
            const bool shown = Opaque(a.bgra.data() + at);
            if (shown != Opaque(b.bgra.data() + at)) return false;
            if (shown && std::memcmp(a.bgra.data() + at, b.bgra.data() + at, 3) != 0) return false;
        }
        return true;
    }

    std::vector<uint8_t> ChocoboPointerFile() { return {std::begin(kChocoboPointerFile), std::end(kChocoboPointerFile)}; }

    std::vector<CursorFrame> ChocoboPointer() { return ReadCursorFile(kChocoboPointerFile, sizeof(kChocoboPointerFile)); }
}
