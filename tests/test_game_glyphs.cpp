#include "game_glyphs.h"
#include "boxes.h"
#include "test.h"

#include <cmath>
#include <vector>

using namespace headsup;
using test::Box;
using test::Near;

namespace
{
    // XYZRHW | DIFFUSE | TEX1, 28 bytes: the layout of the game's nameplate glyphs.
    struct Vertex
    {
        float x, y, z, rhw;
        uint32_t color;
        float u, v;
    };
    static_assert(sizeof(Vertex) == 28);

    // A glyph quad as a 4-vertex triangle strip.
    std::vector<Vertex> Quad(float x0, float y0, float x1, float y1, float z)
    {
        return {{x0, y0, z, 1.0f, 0xFFFFFFFF, 0, 0}, {x1, y0, z, 1.0f, 0xFFFFFFFF, 1, 0},
            {x0, y1, z, 1.0f, 0xFFFFFFFF, 0, 1}, {x1, y1, z, 1.0f, 0xFFFFFFFF, 1, 1}};
    }

    // "Brutal Sheep"-like nameplate: a lead quad and 11 letters, 7 px wide, 10 px tall, at y 200.
    std::vector<ScreenBox> Name(float x)
    {
        std::vector<ScreenBox> glyphs{Box(x - 7.0f, 200.0f, x, 210.0f)};
        for (int i = 0; i < 11; ++i)
        {
            const float left = x + static_cast<float>(i) * 7.0f + (i >= 6 ? 4.0f : 0.0f); // a space after the sixth letter
            glyphs.push_back(Box(left, 200.0f, left + 7.0f, 210.0f));
        }
        return glyphs;
    }
}

TEST(vertex_counts_by_primitive_type)
{
    CHECK_EQ(VertexCount(kTriangleStrip, 2), 4u); // one glyph quad
    CHECK_EQ(VertexCount(kTriangleList, 2), 6u);
    CHECK_EQ(VertexCount(kTriangleFan, 3), 5u);
    CHECK_EQ(VertexCount(kLineList, 3), 6u);
    CHECK_EQ(VertexCount(kLineStrip, 3), 4u);
    CHECK_EQ(VertexCount(kPointList, 7), 7u);
    CHECK_EQ(VertexCount(0, 2), 0u);
}

TEST(nameplate_glyph_gives_its_box)
{
    // A nameplate letter: drawn inside the scene, at depth 0.997.
    auto quad = Quad(389.016f, 601.077f, 396.38f, 610.281f, 0.997f);
    quad[2].z = 0.998f;
    ScreenBox box;
    float depth = 0.0f;
    CHECK(WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
    CHECK(box.valid);
    CHECK(Near(box.minX, 389.016f) && Near(box.maxX, 396.38f));
    CHECK(Near(box.minY, 601.077f) && Near(box.maxY, 610.281f));
    CHECK(Near(depth, 0.998f)); // the farthest corner
}

TEST(hud_text_at_depth_zero_is_rejected)
{
    // The target bar's copy of a mob's name: HUD text at depth 0.
    const auto quad = Quad(45.5f, 881.5f, 50.5f, 891.5f, 0.0f);
    ScreenBox box;
    float depth = 0.0f;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
    CHECK(!box.valid);
}

TEST(any_vertex_outside_the_scene_depth_rejects_the_draw)
{
    auto quad = Quad(10, 10, 20, 20, 0.5f);
    quad[3].z = 1.0f;
    ScreenBox box;
    float depth = 0.0f;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
    quad[3].z = std::nanf("");
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
    quad[3].z = 0.5f;
    quad[3].x = INFINITY;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box, depth));
}

TEST(a_ui_quad_gives_its_box)
{
    ScreenBox box;
    CHECK(UiQuadBox(Quad(1068.5f, 162.5f, 1088.5f, 194.5f, 0.0f).data(), sizeof(Vertex), box)); // the game's cursor
    CHECK(Near(box.minX, 1068.5f) && Near(box.maxY, 194.5f));
    CHECK(!UiQuadBox(Quad(10, 10, 20, 20, 0.5f).data(), sizeof(Vertex), box)); // in the scene, not the UI
    CHECK(!UiQuadBox(nullptr, sizeof(Vertex), box));
    CHECK(!UiQuadBox(Quad(10, 10, 20, 20, 0.0f).data(), 12, box));
}

TEST(unusable_draws_are_rejected)
{
    const auto quad = Quad(10, 10, 20, 20, 0.5f);
    ScreenBox box;
    float depth = 0.0f;
    CHECK(!WorldTextBox(nullptr, sizeof(Vertex), 4, box, depth));
    CHECK(!WorldTextBox(quad.data(), 12, 4, box, depth));
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 0, box, depth));
    std::vector<Vertex> many(kMaxTextVertices + 1, quad[0]);
    CHECK(!WorldTextBox(many.data(), sizeof(Vertex), kMaxTextVertices + 1, box, depth));
}

TEST(render_target_box_scales_to_the_back_buffer)
{
    ScreenBox box;
    box.Add(3840.0f * 0.5f, 2160.0f * 0.25f);
    box.Add(3840.0f * 0.75f, 2160.0f * 0.5f);
    const ScreenBox scaled = box.Scaled(2560.0f / 3840.0f, 1440.0f / 2160.0f);
    CHECK(Near(scaled.minX, 1280.0f) && Near(scaled.maxX, 1920.0f));
    CHECK(Near(scaled.minY, 360.0f) && Near(scaled.maxY, 720.0f));
}

TEST(nameplate_box_is_the_whole_name)
{
    const ScreenBox box = NameplateFromGlyphs(Name(1000.0f));
    CHECK(box.valid);
    CHECK(Near(box.minX, 993.0f) && Near(box.maxX, 1081.0f));
    CHECK(Near(box.minY, 200.0f) && Near(box.maxY, 210.0f));
}

TEST(a_stray_glyph_from_another_nameplate_is_ignored)
{
    // One letter of a different name, attributed to this mob on some frames by a stale stack pointer.
    auto glyphs = Name(1000.0f);
    glyphs.push_back(Box(400.0f, 520.0f, 407.0f, 530.0f));
    const ScreenBox far = NameplateFromGlyphs(glyphs);
    CHECK(Near(far.minX, 993.0f) && Near(far.maxX, 1081.0f) && Near(far.maxY, 210.0f));

    // Same line but well past the end of the name.
    glyphs.back() = Box(1200.0f, 200.0f, 1207.0f, 210.0f);
    const ScreenBox sameLine = NameplateFromGlyphs(glyphs);
    CHECK(Near(sameLine.minX, 993.0f) && Near(sameLine.maxX, 1081.0f));
}

TEST(a_huge_in_scene_quad_is_not_a_nameplate)
{
    // A screen-sized in-scene quad that the game sometimes credits to whichever mob is on the stack.
    const ScreenBox huge = Box(667.0f, 401.0f, 2731.0f, 563.0f);
    CHECK(!NameplateFromGlyphs({huge}).valid);

    auto glyphs = Name(1000.0f);
    glyphs.push_back(huge);
    const ScreenBox box = NameplateFromGlyphs(glyphs);
    CHECK(Near(box.minX, 993.0f) && Near(box.maxX, 1081.0f) && Near(box.maxY, 210.0f));
}

TEST(a_tall_square_on_the_name_line_does_not_stretch_the_name)
{
    // From a fight capture: Brutal Sheep's letters, the last a 'p' with a descender, and a 20 px square from the same
    // font that the game credited to the sheep, drawn on its line while players fought it. The square made our text
    // jump a third larger for a frame.
    const std::vector<ScreenBox> glyphs{Box(1763.2f, 469.0f, 1781.2f, 481.7f), Box(1781.2f, 469.0f, 1794.7f, 481.7f),
        Box(1794.7f, 471.5f, 1812.7f, 481.7f), Box(1812.7f, 469.0f, 1823.9f, 481.7f), Box(1823.9f, 469.0f, 1839.7f, 481.7f),
        Box(1839.7f, 469.0f, 1850.9f, 481.7f), Box(1844.6f, 463.5f, 1864.8f, 483.8f), Box(1850.9f, 469.0f, 1857.7f, 477.9f),
        Box(1857.7f, 469.0f, 1875.7f, 481.7f), Box(1875.7f, 469.0f, 1893.7f, 481.7f), Box(1893.7f, 469.0f, 1911.7f, 481.7f),
        Box(1911.7f, 469.0f, 1929.7f, 481.7f), Box(1929.7f, 469.0f, 1947.7f, 484.2f)};
    const ScreenBox box = NameplateFromGlyphs(glyphs);
    CHECK(Near(box.minY, 469.0f) && Near(box.maxY, 484.2f)); // the descender still counts
    CHECK(Near(box.minX, 1763.2f) && Near(box.maxX, 1947.7f));
}

TEST(a_square_drawn_twice_counts_once_against_a_short_name)
{
    // Squares like those on Wudi's name in the same capture, each drawn twice (once glowing), three of them over a
    // three-letter name. Counted twice, they would be most of the glyphs and set the name's cap line.
    std::vector<ScreenBox> glyphs{Box(1784.2f, 468.6f, 1790.6f, 477.0f), Box(1790.6f, 468.6f, 1807.7f, 480.6f),
        Box(1807.7f, 471.0f, 1824.8f, 480.6f), Box(1824.8f, 468.6f, 1841.9f, 480.6f)};
    for (const float x : {1779.0f, 1798.2f, 1817.4f})
        for (int pass = 0; pass < 2; ++pass)
            glyphs.push_back(Box(x, 464.2f, x + 19.2f, 483.4f));
    const ScreenBox box = NameplateFromGlyphs(glyphs);
    CHECK(Near(box.minY, 468.6f) && Near(box.maxY, 480.6f));
    CHECK(Near(box.minX, 1784.2f) && Near(box.maxX, 1841.9f));
}

TEST(a_glyph_reaching_past_the_name_line_is_not_a_letter)
{
    // From a second fight capture: Damselfly's letters, the last a 'y' with a descender, and a wide bar from the same
    // font credited to it, reaching 0.6 of a letter below the baseline: no taller than a letter with a descender, but
    // not where letters go. Measured as GameNames hands them over, scaled from the 3840x2160 scene to the 2560x1440 screen.
    constexpr float kToScreen = 2560.0f / 3840.0f;
    std::vector<ScreenBox> glyphs{Box(1703.8f, 488.1f, 1721.8f, 500.8f), Box(1721.8f, 488.1f, 1737.6f, 500.8f),
        Box(1734.5f, 490.6f, 1808.7f, 508.3f), Box(1737.6f, 488.1f, 1755.6f, 500.8f), Box(1755.6f, 488.1f, 1773.6f, 500.8f),
        Box(1773.6f, 488.1f, 1791.6f, 500.8f), Box(1791.6f, 488.1f, 1802.8f, 500.8f), Box(1802.8f, 488.1f, 1816.3f, 500.8f),
        Box(1816.3f, 488.1f, 1827.6f, 500.8f), Box(1827.6f, 488.1f, 1845.6f, 503.3f)};
    for (ScreenBox& g : glyphs)
        g = g.Scaled(kToScreen, kToScreen);
    const ScreenBox box = NameplateFromGlyphs(glyphs);
    CHECK(Near(box.minY, 488.1f * kToScreen) && Near(box.maxY, 503.3f * kToScreen));
    CHECK(Near(box.minX, 1703.8f * kToScreen) && Near(box.maxX, 1845.6f * kToScreen));
}

TEST(fewer_than_three_glyphs_are_not_a_nameplate)
{
    CHECK(!NameplateFromGlyphs({Box(10, 10, 17, 20), Box(17, 10, 24, 20)}).valid);
    CHECK(NameplateFromGlyphs({Box(10, 10, 17, 20), Box(17, 10, 24, 20), Box(24, 10, 31, 20)}).valid);
}

TEST(only_glyphs_in_the_image_the_letters_go_to_are_part_of_a_name)
{
    // From captures: letters and the icon strip go to the scene image; the game also draws a quad over each character
    // into another image, at the character's depth, which is not part of any name.
    constexpr uintptr_t kScene = 0x0104C8A8, kOther = 0x0104C9E8;
    uintptr_t names = 0;
    CHECK(InNamesImage(true, kScene, names)); // a letter: its image is where names go
    CHECK_EQ(names, kScene);
    CHECK(InNamesImage(false, kScene, names));  // an icon strip beside it
    CHECK(!InNamesImage(false, kOther, names)); // the quad over a character
    CHECK_EQ(names, kScene);
    CHECK(InNamesImage(true, kOther, names)); // a letter drawn elsewhere moves it
    CHECK_EQ(names, kOther);
}

TEST(a_replaced_names_letters_are_hidden_wherever_they_are)
{
    // Names move between frames and the sub-target's is drawn larger, so neither where nor how big matters.
    CHECK(HideReplacedGlyph(Box(1010.0f, 201.0f, 1017.0f, 211.0f), true, 10.0f, {}));
    CHECK(HideReplacedGlyph(Box(400.0f, 520.0f, 407.0f, 530.0f), true, 10.0f, {}));
    CHECK(HideReplacedGlyph(Box(1010.0f, 201.0f, 1021.0f, 216.0f), true, 10.0f, {}));
    // Not the screen-sized quad the game sometimes credits to a mob.
    CHECK(!HideReplacedGlyph(Box(667.0f, 401.0f, 2731.0f, 563.0f), true, 10.0f, {}));
}

TEST(a_letter_in_a_kept_name_stays_whoever_it_is_credited_to)
{
    // A letter of a kept name, credited to a replaced mob by a stale stack pointer.
    const ScreenBox kept = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(!HideReplacedGlyph(Box(1010.0f, 201.0f, 1017.0f, 211.0f), true, 10.0f, {kept}));
    CHECK(HideReplacedGlyph(Box(400.0f, 520.0f, 407.0f, 530.0f), true, 10.0f, {kept}));
}

TEST(a_replaced_names_icons_are_hidden_wherever_they_are)
{
    // From a capture: a square icon 1.5 letters tall, in another texture.
    CHECK(HideReplacedGlyph(Box(985.0f, 198.0f, 1000.0f, 213.0f), false, 10.0f, {}));
    CHECK(HideReplacedGlyph(Box(800.0f, 640.0f, 815.0f, 655.0f), false, 10.0f, {}));
    // A close player: letters 34 px tall and the icons as one 90x51 strip, wider than any letter.
    CHECK(HideReplacedGlyph(Box(1091.0f, 634.0f, 1181.0f, 685.0f), false, 34.0f, {}));
    // The sub-target's, drawn larger than its letters were the frame before.
    CHECK(HideReplacedGlyph(Box(1091.0f, 634.0f, 1226.0f, 710.0f), false, 34.0f, {}));
    // Not a quad many times the letters' height, such as the one the game draws over the whole model.
    CHECK(!HideReplacedGlyph(Box(667.0f, 600.0f, 1400.0f, 900.0f), false, 34.0f, {}));
    // Not before any letters have been seen to size them by.
    CHECK(!HideReplacedGlyph(Box(985.0f, 198.0f, 1000.0f, 213.0f), false, 0.0f, {}));
}

TEST(a_kept_names_letters_inside_it_are_never_hidden)
{
    // A kept player's name overlapping a replaced mob's, as in melee, and one beside it on the same line.
    const ScreenBox mob    = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const ScreenBox player = Box(1040.0f, 196.0f, 1110.0f, 206.0f);
    CHECK(!HideStrayLetter(Box(1050.0f, 197.0f, 1057.0f, 207.0f), &player, {mob}));
    const ScreenBox beside = Box(1090.0f, 200.0f, 1150.0f, 210.0f);
    CHECK(!HideStrayLetter(Box(1090.0f, 200.0f, 1097.0f, 210.0f), &beside, {mob}));
}

TEST(a_letters_shown_color_applies_the_modulate_scale_and_is_opaque)
{
    CHECK_EQ(ShownColor(0x80604020u, 2), 0xFFC08040u);
    CHECK_EQ(ShownColor(0x80808040u, 2), 0xFFFFFF80u); // 0x80 is full intensity
    CHECK_EQ(ShownColor(0x80402010u, 4), 0xFFFF8040u);
    CHECK_EQ(ShownColor(0x80FFFF80u, 1), 0xFFFFFF80u);
}

TEST(name_color_is_the_most_common_letter_color)
{
    const ScreenBox plate = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    const std::vector<GlyphDraw> glyphs{{Box(1000, 200, 1007, 210), 0xFFFFFF80, 0, 0.0f}, {Box(1007, 200, 1014, 210), 0xFFFFFF80, 0, 0.0f},
        {Box(1014, 200, 1021, 210), 0xFF000000, 0, 0.0f}, {Box(400, 520, 407, 530), 0xFFFF0000, 0, 0.0f}, {Box(410, 520, 417, 530), 0xFFFF0000, 0, 0.0f},
        {Box(420, 520, 427, 530), 0xFFFF0000, 0, 0.0f}};
    CHECK_EQ(NameColor(glyphs, plate), 0xFFFFFF80u); // letters outside the nameplate do not count
    CHECK_EQ(NameColor({}, plate), 0xFFFFFFFFu);
}

TEST(a_stray_letter_is_hidden_only_in_a_replaced_name)
{
    const ScreenBox replaced = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    // Credited to no one: the first quad of a name is often credited to the entity drawn before it.
    CHECK(HideStrayLetter(Box(1010.0f, 201.0f, 1017.0f, 211.0f), nullptr, {replaced}));
    CHECK(HideStrayLetter(Box(990.0f, 214.0f, 997.0f, 224.0f), nullptr, {replaced})); // moved a little
    CHECK(!HideStrayLetter(Box(400.0f, 520.0f, 407.0f, 530.0f), nullptr, {replaced}));
    CHECK(!HideStrayLetter(Box(1010.0f, 201.0f, 1017.0f, 211.0f), nullptr, {}));
    // A 24 px quad just above the name, such as a target cursor, and a speck inside it: not its letters' size.
    CHECK(!HideStrayLetter(Box(1028.0f, 172.0f, 1052.0f, 196.0f), nullptr, {replaced}));
    CHECK(!HideStrayLetter(Box(1010.0f, 204.0f, 1012.0f, 206.0f), nullptr, {replaced}));
}

TEST(a_glyph_beside_a_name_is_one_of_its_icons)
{
    const ScreenBox name = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(BesideName(Box(985.0f, 198.0f, 1000.0f, 213.0f), name));
    CHECK(BesideName(Box(925.0f, 198.0f, 940.0f, 213.0f), name));   // a row of them
    CHECK(!BesideName(Box(800.0f, 198.0f, 815.0f, 213.0f), name));  // far to the left
    CHECK(!BesideName(Box(1100.0f, 198.0f, 1115.0f, 213.0f), name)); // past its right end
    CHECK(!BesideName(Box(985.0f, 240.0f, 1000.0f, 255.0f), name));  // below
    CHECK(!BesideName(Box(940.0f, 150.0f, 1000.0f, 260.0f), name));  // many letters tall
}

TEST(a_names_depth_comes_from_its_letters_inside_it)
{
    // From a capture looking steeply down: Jax's letters at 0.98687. A deeper letter of another name, credited to Jax
    // on some frames, would put our nameplate behind the top of Jax's head.
    const ScreenBox plate = Box(1830.6f, 478.7f, 1902.3f, 515.4f);
    const std::vector<GlyphDraw> letters{{Box(1830.6f, 478.7f, 1850.1f, 504.4f), kWhite, 1, 0.98687f},
        {Box(1850.1f, 478.7f, 1902.3f, 515.4f), kWhite, 1, 0.98687f}, {Box(400.0f, 520.0f, 407.0f, 530.0f), kWhite, 1, 0.99800f}};
    CHECK_EQ(NameDepth(letters, plate), 0.98687f);
    CHECK_EQ(NameDepth({}, plate), 0.0f);
}

namespace
{
    constexpr uintptr_t kFont = 0xF0, kIcons = 0x1C;

    // An entity's name as the game draws it: an icon strip, then the font's letters, at one depth and color.
    std::vector<GlyphDraw> Drawn(float x, float y, float depth, uint32_t argb, bool icons)
    {
        std::vector<GlyphDraw> drawn;
        if (icons) drawn.push_back({Box(x - 16.0f, y - 2.0f, x, y + 13.0f), kWhite, kIcons, depth});
        for (int i = 0; i < 4; ++i)
            drawn.push_back({Box(x + static_cast<float>(i) * 7.0f, y, x + static_cast<float>(i + 1) * 7.0f, y + 10.0f), argb, kFont, depth});
        return drawn;
    }
}

TEST(a_frames_names_come_from_the_font_the_most_glyphs_use)
{
    const std::unordered_map<uint16_t, std::vector<GlyphDraw>> glyphs{{0x220, Drawn(1000.0f, 200.0f, 0.98f, 0xFFFFFF80, true)},
        {0x430, Drawn(600.0f, 400.0f, 0.97f, 0xFF80C0FF, false)}};
    const FrameNames names = ReadFrameNames(glyphs, {0x430}, FrameNames{});
    CHECK_EQ(names.font, kFont);
    CHECK(Near(names.plates.at(0x220).minX, 1000.0f) && Near(names.plates.at(0x220).maxX, 1028.0f)); // letters only
    CHECK(Near(names.wholes.at(0x220).minX, 984.0f)); // with the icon strip
    CHECK_EQ(names.colors.at(0x220), 0xFFFFFF80u);
    CHECK_EQ(names.depths.at(0x430), 0.97f);
    CHECK_EQ(names.glyphCounts.at(0x220), 5u);
    CHECK_EQ(names.runs.at(0x220), 1u);
    CHECK_EQ(names.replacedPlates.size(), 1u);
    CHECK_EQ(names.keptPlates.size(), 1u);
    CHECK(Near(names.keptPlates[0].minX, 600.0f));
    CHECK(Near(names.letterHeight, 10.0f));
}

TEST(names_keep_their_run_and_the_last_frames_font_and_letters)
{
    const std::unordered_map<uint16_t, std::vector<GlyphDraw>> glyphs{{0x220, Drawn(1000.0f, 200.0f, 0.98f, kWhite, false)}};
    FrameNames last;
    last.runs[0x220] = 4;
    const FrameNames names = ReadFrameNames(glyphs, {}, last);
    CHECK_EQ(names.runs.at(0x220), 5u);
    last.font         = kFont;
    last.letterHeight = 12.0f;
    const FrameNames empty = ReadFrameNames({}, {}, last); // no glyphs: nothing to vote on or measure
    CHECK_EQ(empty.font, kFont);
    CHECK_EQ(empty.letterHeight, 12.0f);
    CHECK(empty.plates.empty() && empty.runs.empty());
}

TEST(a_frames_letter_height_is_the_median_names)
{
    // One huge name close to the camera does not size every new name's icons.
    std::unordered_map<uint16_t, std::vector<GlyphDraw>> glyphs{{1, Drawn(100.0f, 100.0f, 0.9f, kWhite, false)},
        {2, Drawn(300.0f, 100.0f, 0.9f, kWhite, false)}};
    std::vector<GlyphDraw> close;
    for (int i = 0; i < 4; ++i)
        close.push_back({Box(500.0f + static_cast<float>(i) * 30.0f, 100.0f, 530.0f + static_cast<float>(i) * 30.0f, 140.0f), kWhite, kFont, 0.9f});
    glyphs[3] = close;
    CHECK(Near(ReadFrameNames(glyphs, {}, FrameNames{}).letterHeight, 10.0f));
}

namespace
{
    // One frame of a four-letter name with letters this tall; returns the size its nameplate scales by.
    float NextSize(FrameNames& names, float letterHeight)
    {
        std::vector<GlyphDraw> letters;
        for (int i = 0; i < 4; ++i)
        {
            const float x = 1000.0f + static_cast<float>(i) * 7.0f;
            letters.push_back({Box(x, 200.0f, x + 7.0f, 200.0f + letterHeight), kWhite, kFont, 0.98f});
        }
        names = ReadFrameNames({{0x41, letters}}, {}, names);
        return names.sizes.at(0x41);
    }
}

TEST(a_names_size_ignores_a_glitch_of_a_frame_or_two)
{
    // Whatever slips past the letter rules for a frame or two (the game credits stray glyphs to whoever is on the
    // stack) does not resize the nameplate.
    FrameNames names;
    for (int i = 0; i < 3; ++i)
        CHECK(Near(NextSize(names, 10.0f), 10.0f));
    CHECK(Near(NextSize(names, 13.8f), 10.0f));
    CHECK(Near(NextSize(names, 13.8f), 10.0f));
    CHECK(Near(NextSize(names, 10.0f), 10.0f));
}

TEST(a_names_size_follows_a_real_change_on_its_third_frame)
{
    FrameNames names;
    for (int i = 0; i < 5; ++i)
        NextSize(names, 10.0f);
    CHECK(Near(NextSize(names, 14.0f), 10.0f));
    CHECK(Near(NextSize(names, 14.0f), 10.0f));
    CHECK(Near(NextSize(names, 14.0f), 14.0f)); // the camera zoomed in
    names = ReadFrameNames({}, {}, names);    // gone for a frame: a new name starts fresh
    CHECK(Near(NextSize(names, 8.0f), 8.0f));
}

TEST(a_glyph_is_blocked_by_its_owners_name_or_the_frames_letters)
{
    FrameNames last;
    last.letterHeight = 10.0f;
    const ScreenBox icon = Box(985.0f, 198.0f, 1000.0f, 213.0f);
    const ScreenBox name = Box(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(HideGameGlyph(icon, false, true, nullptr, last)); // a new name: sized by the frame's letters
    last.letterHeight = 2.0f;
    CHECK(!HideGameGlyph(icon, false, true, nullptr, last));
    CHECK(HideGameGlyph(icon, false, true, &name, last));   // its own name sizes it when it had one
    CHECK(!HideGameGlyph(icon, false, false, &name, last)); // a kept name keeps its icons
    last.replacedPlates = {name};
    CHECK(HideGameGlyph(Box(1010.0f, 201.0f, 1017.0f, 211.0f), true, false, nullptr, last)); // a stray letter in it
}
