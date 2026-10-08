#include "name_frame.h"
#include "test.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

using namespace headsup;

namespace
{
    // Return addresses of the two calls into the name routine, with the client loaded at 0x034C0000, as /hu namedump
    // found them on 2026-10-07: RVAs 0x0D08A8 and 0x175711.
    const uint32_t kCallers[] = {0x035908A8, 0x03635711};

    template <typename T>
    void Put(std::vector<uint8_t>& frame, uint32_t offset, T value)
    {
        std::memcpy(frame.data() + offset, &value, sizeof(value));
    }

    NameFrame GoodName()
    {
        NameFrame frame;
        frame.caller = kCallers[0];
        frame.actor  = 0x0C2A9F00;
        frame.length = 7;
        frame.x      = 1280.5f;
        frame.y      = 600.25f;
        frame.depth  = 0.9876f;
        frame.scaleX = 1.5f;
        frame.scaleY = 1.25f;
        frame.text   = 0x0BADF00D;
        return frame;
    }

    bool FromRoutine(const NameFrame& frame) { return FromNameRoutine(frame, kCallers); }

    const float kNan      = std::numeric_limits<float>::quiet_NaN();
    const float kInfinity = std::numeric_limits<float>::infinity();
}

TEST(a_frame_is_read_at_the_routines_offsets)
{
    // The words of the frame the hook captured for the player Carrott (/hu namedump, 2026-10-07). The /hu drawdump taken
    // with nobody moving put the game's letters, in back-buffer pixels (2/3 of the scene's), centered with the
    // linkshell icon at x 1279.7, topped at y 656.9 and at depth 0.98281.
    std::vector<uint8_t> bytes(kNameFrameBytes, 0);
    Put(bytes, 0x04, uint32_t{0x3116EA10});
    Put(bytes, 0x18, uint32_t{9});
    Put(bytes, 0x30, uint32_t{0x44EFF009});
    Put(bytes, 0x34, uint32_t{0x447654BF});
    Put(bytes, 0x38, uint32_t{0x3F7B99B4});
    Put(bytes, 0x3C, uint32_t{0x40BA2EDD});
    Put(bytes, 0x48, uint32_t{0x3F7B99B4});
    Put(bytes, 0x4C, uint32_t{0x41070000});
    Put(bytes, 0x50, uint32_t{0x4097E000});
    Put(bytes, 0x6D4, uint32_t{0x035908A8});
    Put(bytes, 0x6D8, uint32_t{0x0084EC38});
    Put(bytes, 0x6DC, uint32_t{0x0084EC48});
    Put(bytes, 0x6E0, uint32_t{0x3F700000});
    Put(bytes, 0x6E4, uint32_t{0xC0808080});
    Put(bytes, 0x6E8, uint32_t{0x001F0F0F});

    const NameFrame frame = ReadNameFrame(bytes.data());
    CHECK_EQ(frame.caller, 0x035908A8u);
    CHECK_EQ(frame.actor, 0x3116EA10u);
    CHECK_EQ(frame.length, 9u);
    CHECK(std::fabs(frame.x / 1.5f - 1279.7f) < 0.1f);
    CHECK(std::fabs(frame.y / 1.5f - 656.9f) < 0.1f);
    CHECK(std::fabs(frame.depth - 0.98281f) < 0.00001f);
    CHECK_EQ(frame.scaleX, 8.4375f);
    CHECK_EQ(frame.scaleY, 4.74609375f);
    CHECK_EQ(frame.text, 0x0084EC48u);
    CHECK_EQ(frame.color, 0xC0808080u);
    CHECK_EQ(frame.shellColor, 0x001F0F0Fu);
}

TEST(a_name_drawn_by_either_caller_passes)
{
    NameFrame frame = GoodName();
    CHECK(FromRoutine(frame));
    frame.caller = kCallers[1];
    CHECK(FromRoutine(frame));
}

TEST(a_name_drawn_from_anywhere_else_is_refused)
{
    NameFrame frame = GoodName();
    frame.caller    = 0x035908A3; // the call itself, not where it returns
    CHECK(!FromRoutine(frame));
}

TEST(names_of_one_to_thirty_six_bytes_pass)
{
    NameFrame frame = GoodName();
    frame.length    = 1;
    CHECK(FromRoutine(frame));
    frame.length = 36;
    CHECK(FromRoutine(frame));
}

TEST(an_empty_or_longer_name_is_refused)
{
    NameFrame frame = GoodName();
    frame.length    = 0;
    CHECK(!FromRoutine(frame));
    frame.length = 37;
    CHECK(!FromRoutine(frame));
}

TEST(a_good_name_has_a_size_and_a_place)
{
    CHECK(Placeable(GoodName()));
}

TEST(a_name_with_no_size_is_refused)
{
    for (const float scale : {0.0f, -1.25f, kNan})
    {
        NameFrame wide = GoodName();
        wide.scaleX    = scale;
        CHECK(!Placeable(wide));
        NameFrame tall = GoodName();
        tall.scaleY    = scale;
        CHECK(!Placeable(tall));
    }
}

TEST(a_place_that_is_not_a_number_is_refused)
{
    NameFrame frame = GoodName();
    frame.x         = kNan;
    CHECK(!Placeable(frame));
    frame   = GoodName();
    frame.y = kInfinity;
    CHECK(!Placeable(frame));
    frame       = GoodName();
    frame.depth = kNan;
    CHECK(!Placeable(frame));
}

TEST(a_line_break_anywhere_in_the_name_makes_it_multi_line)
{
    const uint8_t last[]  = {'Z', 'o', 'n', 'e', '\n'};
    const uint8_t first[] = {'\n', 'Z', 'o', 'n', 'e'};
    const uint8_t plain[] = {'Z', 'o', 'n', 'e', '\n'}; // the break lies past the name's length
    CHECK(!SingleLine(last, 5));
    CHECK(!SingleLine(first, 5));
    CHECK(SingleLine(plain, 4));
}

namespace
{
    // Back-buffer pixels per scene pixel in those dumps: a 2560x1440 back buffer and a 3840x2160 scene.
    constexpr float kToBackBuffer = 2560.0f / 3840.0f;

    bool Near(float a, float b, float within) { return std::fabs(a - b) <= within; }
}

TEST(a_drawn_name_sits_where_the_games_letters_were)
{
    // Carrott's frame from /hu namedump; the /hu drawdump of the same still scene measured the game's letters at
    // (1186.9,656.9)-(1456.9,688.5), and the whole name with its linkshell icon from x 1102.5 to 1456.9, white.
    NameFrame carrott;
    carrott.x      = 1919.5011f;
    carrott.y      = 985.3241f;
    carrott.depth  = 0.982814f;
    carrott.scaleX = 8.4375f;
    carrott.scaleY = 4.74609375f;
    carrott.color  = 0xC0808080;

    const DrawnName name = NameFromFrame(carrott, kToBackBuffer, kToBackBuffer);
    CHECK(name.box.valid);
    CHECK(Near(name.box.CenterX(), (1102.5f + 1456.9f) / 2.0f, 0.1f));
    CHECK(Near(name.box.minY, 656.9f, 0.1f));
    CHECK(Near(name.box.maxY, 688.5f, 0.2f));
    CHECK_EQ(name.box.Width(), 0.0f);
    CHECK_EQ(name.depth, 0.982814f);
    CHECK_EQ(name.color, 0xFFFFFFFFu);
}

TEST(a_drawn_name_keeps_the_games_color_for_it)
{
    // Lombaria, an NPC: the frame's half-intensity green, which the drawdump saw drawn as FFC2FFC2, with letters
    // 16.9 pixels tall at its distance.
    NameFrame lombaria;
    lombaria.x      = 2615.1401f;
    lombaria.y      = 570.0106f;
    lombaria.depth  = 0.991222f;
    lombaria.scaleX = 4.5f;
    lombaria.scaleY = 2.53125f;
    lombaria.color  = 0xC0618061;

    const DrawnName name = NameFromFrame(lombaria, kToBackBuffer, kToBackBuffer);
    CHECK_EQ(name.color, 0xFFC2FFC2u);
    CHECK(Near(name.box.Height(), 16.9f, 0.1f));
    CHECK(Near(name.box.minY, 380.0f, 0.1f));
}

TEST(each_axis_is_scaled_by_its_own_factor)
{
    NameFrame frame;
    frame.x      = 800.0f;
    frame.y      = 400.0f;
    frame.scaleY = 2.0f;
    const DrawnName name = NameFromFrame(frame, 0.5f, 0.25f);
    CHECK_EQ(name.box.CenterX(), 400.0f);
    CHECK_EQ(name.box.minY, 100.0f);
    CHECK_EQ(name.box.Height(), 5.0f); // 10 units of 2 pixels, at a quarter
}
