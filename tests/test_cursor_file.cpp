#include "cursor_file.h"
#include "test.h"

#include <cstring>
#include <iterator>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
#include "generated/pointer.inc"

    const std::vector<uint8_t> kChocoboFile(std::begin(kChocoboPointerFile), std::end(kChocoboPointerFile));

    // BGRA of a pixel, top-left origin.
    std::vector<uint8_t> Pixel(const CursorImage& image, uint32_t x, uint32_t y)
    {
        const size_t at = (static_cast<size_t>(y) * image.width + x) * 4;
        return {image.bgra[at], image.bgra[at + 1], image.bgra[at + 2], image.bgra[at + 3]};
    }

    void Put16(std::vector<uint8_t>& out, uint16_t v) { out.insert(out.end(), {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8)}); }
    void Put32(std::vector<uint8_t>& out, uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            out.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
    void PutChunk(std::vector<uint8_t>& out, const char* id, const std::vector<uint8_t>& body)
    {
        out.insert(out.end(), id, id + 4);
        Put32(out, static_cast<uint32_t>(body.size()));
        out.insert(out.end(), body.begin(), body.end());
        if (body.size() % 2 != 0) out.push_back(0);
    }

    // A frame's BITMAPINFOHEADER: its height counts the color image and then the mask.
    std::vector<uint8_t> InfoHeader(uint32_t width, uint32_t height, uint16_t bpp, uint32_t colorsUsed)
    {
        std::vector<uint8_t> dib;
        Put32(dib, 40);
        Put32(dib, width);
        Put32(dib, height * 2);
        Put16(dib, 1);
        Put16(dib, bpp);
        for (int unused = 0; unused < 4; ++unused) // compression (none), image size and resolution
            Put32(dib, 0);
        Put32(dib, colorsUsed);
        Put32(dib, 0);
        return dib;
    }

    std::vector<uint8_t> CursorFile(const std::vector<uint8_t>& dib, uint32_t width, uint32_t height, uint16_t hotX, uint16_t hotY)
    {
        std::vector<uint8_t> file;
        Put16(file, 0);
        Put16(file, 2); // a cursor
        Put16(file, 1);
        file.insert(file.end(), {static_cast<uint8_t>(width), static_cast<uint8_t>(height), 0, 0}); // 0 is 256
        Put16(file, hotX);
        Put16(file, hotY);
        Put32(file, static_cast<uint32_t>(dib.size()));
        Put32(file, 22);
        file.insert(file.end(), dib.begin(), dib.end());
        return file;
    }

    // A 2x2 cursor file at 8 or 4 bits, hotspot (1, 0): palette 0 red, 1 green, 2 blue; rows top to bottom are
    // (red, green) and (blue, transparent).
    std::vector<uint8_t> TinyCursor(uint16_t bpp = 8)
    {
        std::vector<uint8_t> dib = InfoHeader(2, 2, bpp, 3);
        const uint8_t palette[] = {0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 0};
        dib.insert(dib.end(), palette, palette + sizeof(palette));
        // Rows bottom up, each padded to 4 bytes: (blue, red), then (red, green); at 4 bits the first pixel is the high half.
        const uint8_t eight[] = {2, 0, 0, 0, 0, 1, 0, 0}, four[] = {0x20, 0, 0, 0, 0x01, 0, 0, 0};
        const uint8_t* pixels = bpp == 4 ? four : eight;
        dib.insert(dib.end(), pixels, pixels + 8);
        const uint8_t mask[] = {0x40, 0, 0, 0, 0x00, 0, 0, 0}; // bottom row: the second pixel transparent
        dib.insert(dib.end(), mask, mask + sizeof(mask));
        return CursorFile(dib, 2, 2, 1, 0);
    }

    // Every pixel palette color 0 and none masked out.
    std::vector<uint8_t> PlainCursor(uint32_t width, uint32_t height, uint16_t bpp, uint32_t colorsUsed)
    {
        std::vector<uint8_t> dib  = InfoHeader(width, height, bpp, colorsUsed);
        const size_t rowBytes     = (width * bpp + 31) / 32 * 4;
        const size_t maskRowBytes = (width + 31) / 32 * 4;
        dib.resize(dib.size() + colorsUsed * 4 + (rowBytes + maskRowBytes) * height);
        return CursorFile(dib, width, height, 0, 0);
    }
}

TEST(a_16_color_cursor_reads_each_pixel_from_its_half_byte)
{
    const std::vector<uint8_t> file = TinyCursor(4);
    const auto frames = ReadCursorFile(file.data(), file.size());
    CHECK_EQ(frames.size(), 1u);
    CHECK(Pixel(frames[0].image, 0, 0) == std::vector<uint8_t>({0, 0, 255, 255}));  // red
    CHECK(Pixel(frames[0].image, 1, 0) == std::vector<uint8_t>({0, 255, 0, 255}));  // green
    CHECK(Pixel(frames[0].image, 0, 1) == std::vector<uint8_t>({255, 0, 0, 255}));  // blue
}

TEST(a_cursor_file_decodes_to_its_colors_hotspot_and_transparency)
{
    const std::vector<uint8_t> file = TinyCursor();
    const auto frames = ReadCursorFile(file.data(), file.size());
    CHECK_EQ(frames.size(), 1u);
    const CursorImage& image = frames[0].image;
    CHECK(image.width == 2 && image.height == 2 && image.hotX == 1 && image.hotY == 0);
    CHECK(Pixel(image, 0, 0) == std::vector<uint8_t>({0, 0, 255, 255}));   // red
    CHECK(Pixel(image, 1, 0) == std::vector<uint8_t>({0, 255, 0, 255}));   // green
    CHECK(Pixel(image, 0, 1) == std::vector<uint8_t>({255, 0, 0, 255}));   // blue
    CHECK_EQ(Pixel(image, 1, 1)[3], 0);                                     // transparent
    CHECK_EQ(frames[0].seconds, 0.0);
    // CreateIconFromResourceEx takes a cursor as its hotspot, then the image as the file stores it.
    CHECK_EQ(frames[0].resource.size(), 4 + file.size() - 22);
    CHECK(frames[0].resource[0] == 1 && frames[0].resource[1] == 0 && frames[0].resource[2] == 0 && frames[0].resource[3] == 0);
    CHECK(std::memcmp(frames[0].resource.data() + 4, file.data() + 22, file.size() - 22) == 0);
}

TEST(a_cursor_larger_than_its_format_allows_is_not_read)
{
    // In the 32-bit plugin, a larger side or palette could wrap the size checks and read past the frame.
    auto frames = [](const std::vector<uint8_t>& file) { return ReadCursorFile(file.data(), file.size()).size(); };
    CHECK_EQ(frames(PlainCursor(256, 256, 1, 2)), 1u); // a cursor file's largest
    CHECK_EQ(frames(PlainCursor(257, 1, 1, 2)), 0u);
    CHECK_EQ(frames(PlainCursor(1, 257, 1, 2)), 0u);
    CHECK_EQ(frames(PlainCursor(2, 2, 8, 256)), 1u);
    CHECK_EQ(frames(PlainCursor(2, 2, 8, 257)), 0u); // more colors than 8 bits can pick from
}

TEST(the_chocobo_pointer_has_two_frames_each_shown_five_sixths_of_a_second)
{
    // From third_party/playonline-chocobo/chocobo.ani, read independently with Python: a rate chunk of 50 jiffies per
    // frame, 32x32 24-bit frames with hotspots (5,6) and (4,6), orange at (10,10) and transparent at the corner.
    const auto frames = ChocoboPointer();
    CHECK_EQ(frames.size(), 2u);
    CHECK(frames[0].image.hotX == 5 && frames[0].image.hotY == 6);
    CHECK(frames[1].image.hotX == 4 && frames[1].image.hotY == 6);
    for (const CursorFrame& f : frames)
    {
        CHECK(f.image.width == 32 && f.image.height == 32);
        CHECK(f.seconds > 0.8333 && f.seconds < 0.8334);
        CHECK(Pixel(f.image, 10, 10) == std::vector<uint8_t>({37, 170, 255, 255}));
        CHECK_EQ(Pixel(f.image, 0, 0)[3], 0);
        CHECK_EQ(Pixel(f.image, 20, 20)[3], 0);
        CHECK_EQ(f.resource.size(), 4u + 3240u);
    }
}

TEST(an_animation_without_a_rate_chunk_uses_its_header_rate)
{
    // The chocobo's two frames, in a file that gives 10 jiffies per frame in its header and no rate chunk.
    const std::vector<uint8_t>& chocobo = kChocoboFile;
    std::vector<uint8_t> icons;
    for (size_t at = 0; at + 8 <= chocobo.size(); ++at)
    {
        if (std::memcmp(chocobo.data() + at, "icon", 4) != 0) continue;
        uint32_t size = 0;
        std::memcpy(&size, chocobo.data() + at + 4, 4);
        PutChunk(icons, "icon", std::vector<uint8_t>(chocobo.begin() + static_cast<long>(at) + 8, chocobo.begin() + static_cast<long>(at + 8 + size)));
        at += 8 + size - 1;
    }
    std::vector<uint8_t> header;
    for (const uint32_t v : {36u, 2u, 2u, 0u, 0u, 0u, 1u, 10u, 1u})
        Put32(header, v);
    std::vector<uint8_t> list = {'f', 'r', 'a', 'm'};
    list.insert(list.end(), icons.begin(), icons.end());
    std::vector<uint8_t> body = {'A', 'C', 'O', 'N'};
    PutChunk(body, "anih", header);
    PutChunk(body, "LIST", list);
    std::vector<uint8_t> file;
    PutChunk(file, "RIFF", body);
    const auto frames = ReadCursorFile(file.data(), file.size());
    CHECK_EQ(frames.size(), 2u);
    CHECK(frames[1].seconds > 0.1666 && frames[1].seconds < 0.1667);
}

TEST(the_frame_shown_follows_the_clock_and_loops)
{
    const auto frames = ChocoboPointer(); // 5/6 of a second each
    CHECK_EQ(FrameAt(frames, 0.0), 0u);
    CHECK_EQ(FrameAt(frames, 0.80), 0u);
    CHECK_EQ(FrameAt(frames, 0.86), 1u);
    CHECK_EQ(FrameAt(frames, 1.60), 1u);
    CHECK_EQ(FrameAt(frames, 1.70), 0u); // round again
    CHECK_EQ(FrameAt(std::vector<CursorFrame>(1), 5.0), 0u); // a still cursor
}

TEST(only_whole_cursor_files_are_read)
{
    const std::vector<uint8_t>& chocobo = kChocoboFile;
    CHECK(ReadCursorFile(chocobo.data(), 100).empty());               // cut short
    CHECK(ReadCursorFile(nullptr, 0).empty());
    const uint8_t gif[] = {'G', 'I', 'F', '8', '9', 'a', 0, 0, 0, 0, 0, 0};
    CHECK(ReadCursorFile(gif, sizeof(gif)).empty());
    std::vector<uint8_t> raw(chocobo.begin(), chocobo.end());
    raw[12 + 8 + 32] = 0; // the header's flags: frames as raw bitmaps rather than cursors
    CHECK(ReadCursorFile(raw.data(), raw.size()).empty());
    std::vector<uint8_t> tiny = TinyCursor();
    tiny[2] = 1; // an icon, which has no hotspot
    CHECK(ReadCursorFile(tiny.data(), tiny.size()).empty());
}

TEST(a_cursor_from_windows_bitmaps_is_transparent_where_its_mask_is_white)
{
    // 2x1 as GetDIBits gives it at 32 bits: an orange pixel, then one the mask leaves out.
    const std::vector<uint8_t> color = {37, 170, 255, 0, 9, 9, 9, 0};
    const std::vector<uint8_t> mask  = {0, 0, 0, 0, 255, 255, 255, 0};
    const CursorImage image = ImageFromBitmaps(2, 1, 5, 6, color, mask);
    CHECK(image.width == 2 && image.height == 1 && image.hotX == 5 && image.hotY == 6);
    CHECK(Pixel(image, 0, 0) == std::vector<uint8_t>({37, 170, 255, 255}));
    CHECK_EQ(Pixel(image, 1, 0)[3], 0);
}

TEST(two_cursors_match_by_hotspot_shape_and_visible_colors)
{
    const CursorImage chocobo = ChocoboPointer()[0].image;
    CHECK(SameCursor(chocobo, chocobo));
    CHECK(!SameCursor(chocobo, ChocoboPointer()[1].image)); // the other frame: its hotspot moved
    CursorImage recolored = chocobo;
    recolored.bgra[(10 * 32 + 10) * 4] ^= 1;
    CHECK(!SameCursor(chocobo, recolored));
    CursorImage hidden = chocobo;
    hidden.bgra[(10 * 32 + 10) * 4 + 3] = 0;
    CHECK(!SameCursor(chocobo, hidden));
    CursorImage underneath = chocobo; // the color of a transparent pixel is never seen
    underneath.bgra[0] = 77;
    CHECK(SameCursor(chocobo, underneath));
    CursorImage moved = chocobo;
    moved.hotX = 6;
    CHECK(!SameCursor(chocobo, moved));
}
