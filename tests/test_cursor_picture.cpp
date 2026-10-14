#include "cursor_picture.h"
#include "test.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    uint32_t Pixel(const Image& image, int x, int y) { return image.argb[static_cast<size_t>(y * image.width + x)]; }

    int Covered(const Image& image)
    {
        int covered = 0;
        for (const uint32_t argb : image.argb)
            covered += Channel(argb, kAlphaShift) != 0 ? 1 : 0;
        return covered;
    }
}

TEST(the_chocobo_is_the_pointers_first_frame_in_its_own_colors)
{
    const std::vector<CursorFrame> frames = ChocoboPointer();
    const CursorImage& frame              = frames.front().image;
    const Image picture                   = ChocoboPicture();
    CHECK_EQ(static_cast<uint32_t>(picture.width), frame.width);
    CHECK_EQ(static_cast<uint32_t>(picture.height), frame.height);
    int opaque = 0;
    for (uint32_t y = 0; y < frame.height; ++y)
    {
        for (uint32_t x = 0; x < frame.width; ++x)
        {
            const uint8_t* bgra = frame.bgra.data() + (static_cast<size_t>(y) * frame.width + x) * 4;
            const uint32_t argb = Pixel(picture, static_cast<int>(x), static_cast<int>(y));
            CHECK_EQ(argb, Argb(bgra[3], bgra[2], bgra[1], bgra[0]));
            opaque += bgra[3] != 0 ? 1 : 0;
        }
    }
    CHECK(opaque > 50); // a bird, not an empty square
}

TEST(a_shape_cursor_is_its_color_inside_its_edge_and_points_where_its_shape_does)
{
    const CursorDrawing triangle = DrawCursor(CursorStyle::Triangle, nullptr, 10, 8, 2, 0xFF00FF00u, 0xFF000000u);
    CHECK(triangle.image.width == 14 && triangle.image.height == 12);
    CHECK_EQ(Pixel(triangle.image, 7, 2), 0xFF00FF00u); // the middle of its wide top
    CHECK_EQ(Pixel(triangle.image, 7, 0), 0xFF000000u); // the edge above it
    CHECK(std::fabs(triangle.tip - 0.5f) < 1e-4f);
    // The feather's quill ends near its left side: within the picture, past the edge's margin.
    const CursorDrawing feather = DrawCursor(CursorStyle::Feather, nullptr, 20, 43, 2, 0xFFFF0000u, 0xFF000000u);
    CHECK(feather.image.width == 24 && feather.image.height == 47);
    CHECK(feather.tip > (2.0f + 0.17f * 20.0f) / 24.0f && feather.tip < (2.0f + 0.21f * 20.0f) / 24.0f);
    // Stretched, it is the width asked for.
    CHECK_EQ(DrawCursor(CursorStyle::Feather, nullptr, 40, 43, 2, 0xFFFF0000u, 0xFF000000u).image.width, 44);
}

TEST(a_picture_cursor_keeps_its_own_colors_whatever_color_the_cursor_has)
{
    const Image chocobo = ChocoboPicture();
    const CursorDrawing red  = DrawCursor(CursorStyle::Chocobo, &chocobo, chocobo.width, chocobo.height, 1, 0xFFFF0000u, 0xFF000000u);
    const CursorDrawing blue = DrawCursor(CursorStyle::Chocobo, &chocobo, chocobo.width, chocobo.height, 1, 0xFF0000FFu, 0xFF000000u);
    CHECK(red.image.argb == blue.image.argb);
    CHECK(red.image.width == chocobo.width + 2 && red.image.height == chocobo.height + 2);
    CHECK(std::fabs(red.tip - 0.5f) < 1e-4f); // a picture stands over the middle of the name
    // At its own size every pixel it covers whole is its own color, one in from the margin.
    int same = 0;
    for (int y = 0; y < chocobo.height; ++y)
        for (int x = 0; x < chocobo.width; ++x)
            if (Channel(Pixel(chocobo, x, y), kAlphaShift) == 0xFF)
            {
                CHECK_EQ(Pixel(red.image, x + 1, y + 1), Pixel(chocobo, x, y));
                ++same;
            }
    CHECK(same > 50);
    CHECK(Covered(red.image) > Covered(chocobo)); // and the edge around it
    // Drawn smaller, it is that size.
    const CursorDrawing small = DrawCursor(CursorStyle::Chocobo, &chocobo, 16, 12, 2, 0, 0xFF000000u);
    CHECK(small.image.width == 20 && small.image.height == 16);
}

TEST(a_file_cursor_that_was_not_read_is_drawn_as_the_feather)
{
    const CursorDrawing missing = DrawCursor(CursorStyle::File, nullptr, 20, 43, 2, 0xFFFF0000u, 0xFF000000u);
    const CursorDrawing feather = DrawCursor(CursorStyle::Feather, nullptr, 20, 43, 2, 0xFFFF0000u, 0xFF000000u);
    CHECK(missing.image.argb == feather.image.argb && missing.tip == feather.tip);
    // Read, it is its picture.
    const Image dot{1, 1, {0xFF123456u}};
    const CursorDrawing file = DrawCursor(CursorStyle::File, &dot, 3, 3, 1, 0xFFFF0000u, 0xFF000000u);
    CHECK(file.image.width == 5 && file.image.height == 5);
    CHECK_EQ(Pixel(file.image, 2, 2), 0xFF123456u);
}

TEST(the_cursors_folder_lists_its_cursor_files_by_name)
{
    // Whatever else is in the folder is left out; a name comes once though it has both kinds of file.
    const std::vector<std::string> names =
        CursorFileNames({"sword.cur", "Gold Arrow.ANI", "readme.txt", "arrow.png", "sword.ani", ".cur", "backup.cur.bak", "axe.Cur"});
    CHECK((names == std::vector<std::string>{"axe", "Gold Arrow", "sword"}));
    CHECK(CursorFileNames({}).empty());
    // Windows opens Arrow.cur for arrow too: two files whose names differ only by case are one name.
    CHECK((CursorFileNames({"arrow.ani", "Arrow.cur"}) == std::vector<std::string>{"Arrow"}));
    // A name the settings would not keep is not offered: picking it would only put the feather back.
    const std::string longest(kMaxCursorFileName, 'a'), tooLong(kMaxCursorFileName + 1, 'b');
    CHECK((CursorFileNames({tooLong + ".cur", longest + ".cur"}) == std::vector<std::string>{longest}));
    CHECK(IsCursorFileName(longest) && !IsCursorFileName(tooLong) && !IsCursorFileName(""));
    for (const char* path : {"..\\x", "a/b", "c:evil"})
        CHECK(!IsCursorFileName(path));
}

TEST(a_cursor_files_bytes_are_its_first_frames_picture_or_nothing)
{
    // A 1x1 cursor at 32 bits: one orange pixel.
    std::vector<uint8_t> file{0, 0, 2, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 48, 0, 0, 0, 22, 0, 0, 0};
    const uint8_t header[40] = {40, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 1, 0, 32, 0};
    file.insert(file.end(), header, header + sizeof(header));
    file.insert(file.end(), {0x10, 0x80, 0xFF, 0xFF, 0, 0, 0, 0}); // blue, green, red, alpha; then the mask's row
    const std::optional<Image> picture = CursorFilePicture(file.data(), file.size());
    CHECK(picture && picture->width == 1 && picture->height == 1);
    CHECK_EQ(picture->argb[0], 0xFFFF8010u);
    const std::vector<uint8_t> text{'n', 'o', 't', ' ', 'a', ' ', 'c', 'u', 'r', 's', 'o', 'r', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!', '!'};
    CHECK(!CursorFilePicture(text.data(), text.size()));
    CHECK(!CursorFilePicture(nullptr, 0));
}
