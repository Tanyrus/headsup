#include "dat_bytes.h"
#include "game_font.h"
#include "test.h"
#include "window_skin.h"

#include <string>
#include <vector>

using namespace headsup;

namespace
{
    using namespace dat;

    // A picture of one 565 color, the whole of it.
    Bytes Solid(const char* name, int side, uint16_t color)
    {
        Bytes blocks;
        for (int i = 0; i < side * side / 16; ++i)
            blocks = blocks + ColorBlock(color, color, {});
        return Picture(name, side, side, "DXT1", blocks);
    }

    // The shapes chunk the four pictures are drawn by: its name, the pictures, then its first shape, the background,
    // whose four colors run top left, top right, bottom left, bottom right.
    Bytes Shapes(uint8_t top, uint8_t bottom)
    {
        Bytes out;
        PutText(out, "menu    win00   ");
        out.push_back(4);
        for (const char* name : {"MENU    newtex  ", "MENU    vfr1    ", "MENU    hfr1    ", "MENU    corner  "})
            PutText(out, name);
        Put16(out, 1);
        out.push_back(1);
        for (const int v : {0, 0, 128, 0, 0, 128, 128, 128, 128, 128, 0, 0})
            Put16(out, static_cast<uint16_t>(v));
        out.push_back(0);
        for (const uint8_t level : {top, top, bottom, bottom})
            out.insert(out.end(), {level, level, level, 0x40});
        return out;
    }

    constexpr uint16_t kBlue565 = 0x001F, kRed565 = 0xF800, kGreen565 = 0x07E0, kWhite565 = 0xFFFF;

    Bytes SkinDat(bool withDown = true, uint8_t top = 0x40, uint8_t bottom = 0x7F)
    {
        Bytes out = Chunk("win0", 0x01, Bytes(16, 0)) + Chunk("newt", 0x20, Solid("menu    newtex  ", 128, kBlue565)) +
                    Chunk("hfr1", 0x20, Solid("menu    hfr1    ", 32, kRed565));
        if (withDown) out = out + Chunk("vfr1", 0x20, Solid("menu    vfr1    ", 32, kGreen565));
        return out + Chunk("corn", 0x20, Solid("menu    corner  ", 32, kWhite565)) + Chunk("win0", 0x31, Shapes(top, bottom));
    }

    // A picture's first pixel as B, G, R, A.
    std::vector<uint8_t> First(const ColorPicture& p) { return {p.bgra.begin(), p.bgra.begin() + 4}; }
}

TEST(a_skin_is_its_four_pictures_and_the_backgrounds_shade_from_its_shapes)
{
    const Read<WindowSkin> read = ReadWindowSkin(SkinDat());
    CHECK(read.value.has_value());
    const WindowSkin& skin = *read.value;
    CHECK(skin.back.width == 128 && skin.back.height == 128);
    CHECK(skin.across.width == 32 && skin.down.width == 32 && skin.corner.width == 32);
    CHECK((First(skin.back) == std::vector<uint8_t>{255, 0, 0, 255}));
    CHECK((First(skin.across) == std::vector<uint8_t>{0, 0, 255, 255}));
    CHECK((First(skin.down) == std::vector<uint8_t>{0, 255, 0, 255}));
    CHECK((First(skin.corner) == std::vector<uint8_t>{255, 255, 255, 255}));
    // The game's colors are at half: 0x80 is full.
    CHECK_EQ(skin.shadeTop, 0.5f);
    CHECK_EQ(skin.shadeBottom, 127.0f / 128.0f);
}

TEST(a_dat_without_a_piece_is_refused_with_why)
{
    const Read<WindowSkin> read = ReadWindowSkin(SkinDat(false));
    CHECK(!read.value.has_value());
    CHECK(read.failure.find("vfr1") != std::string::npos);
    CHECK(!ReadWindowSkin(Bytes{}).value.has_value());
}

TEST(a_shade_past_full_is_held_to_full)
{
    const Read<WindowSkin> read = ReadWindowSkin(SkinDat(true, 0x90, 0xFF));
    CHECK(read.value.has_value());
    CHECK_EQ(read.value->shadeTop, 1.0f);
    CHECK_EQ(read.value->shadeBottom, 1.0f);
}

TEST(each_window_type_reads_its_own_file)
{
    CHECK_EQ(WindowSkinFile(0), std::string("ROM\\0\\14.DAT"));
    CHECK_EQ(WindowSkinFile(kWindowTypes - 1), std::string("ROM\\0\\21.DAT"));
}

TEST(a_skin_is_read_where_xipivot_would_read_it)
{
    CHECK((GameFileCandidates("[overlays]\n0=A\n", "D:\\d", "C:\\Game", WindowSkinFile(2)) ==
           std::vector<std::string>{"D:\\d\\A\\ROM\\0\\16.DAT", "C:\\Game\\ROM\\0\\16.DAT"}));
}
