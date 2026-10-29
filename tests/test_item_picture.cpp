#include "item_picture.h"
#include "test.h"

#include <vector>

using namespace headsup;

namespace
{
    void Put16(std::vector<uint8_t>& out, uint32_t v)
    {
        out.push_back(static_cast<uint8_t>(v));
        out.push_back(static_cast<uint8_t>(v >> 8));
    }
    void Put32(std::vector<uint8_t>& out, uint32_t v)
    {
        Put16(out, v & 0xFFFF);
        Put16(out, v >> 16);
    }

    // A 2x2 picture of 8 bits a pixel, rows bottom up and padded to four bytes: black, red / red, green from the top.
    std::vector<uint8_t> TwoByTwo(bool fileHeader)
    {
        std::vector<uint8_t> dib;
        Put32(dib, 40);
        Put32(dib, 2);
        Put32(dib, 2);
        Put16(dib, 1);
        Put16(dib, 8);
        for (int i = 0; i < 6; ++i) Put32(dib, 0); // no compression, sizes, resolution, a full palette
        for (int i = 0; i < 256; ++i)
            Put32(dib, i == 1 ? 0x00FF0000u : i == 2 ? 0x0000FF00u : 0u); // as the bytes B, G, R, 0
        for (const uint8_t pixel : {1, 2, 0, 0, 0, 1, 0, 0}) dib.push_back(pixel);
        if (!fileHeader) return dib;
        std::vector<uint8_t> file = {'B', 'M'};
        Put32(file, static_cast<uint32_t>(14 + dib.size()));
        Put32(file, 0);
        Put32(file, 14 + 40 + 1024);
        file.insert(file.end(), dib.begin(), dib.end());
        return file;
    }
}

TEST(an_items_picture_is_read_with_its_black_clear)
{
    for (const bool fileHeader : {false, true})
    {
        const std::vector<uint8_t> bmp = TwoByTwo(fileHeader);
        const std::optional<Image> picture = ItemPicture(bmp.data(), bmp.size());
        CHECK(picture.has_value());
        CHECK(picture->width == 2 && picture->height == 2);
        CHECK_EQ(picture->argb[0] >> 24, 0u);           // top left, black: clear
        CHECK_EQ(picture->argb[1], 0xFFFF0000u);        // top right, red
        CHECK_EQ(picture->argb[2], 0xFFFF0000u);        // bottom left, red
        CHECK_EQ(picture->argb[3], 0xFF00FF00u);        // bottom right, green
    }
}

TEST(a_cut_short_items_picture_is_none)
{
    const std::vector<uint8_t> bmp = TwoByTwo(false);
    CHECK(!ItemPicture(bmp.data(), bmp.size() - 1).has_value());
    CHECK(!ItemPicture(bmp.data(), 20).has_value());
    CHECK(!ItemPicture(nullptr, 100).has_value());
}
