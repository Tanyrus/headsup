#include "nameplate.h"
#include "test.h"

#include <cmath>
#include <vector>

using namespace headsup;

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

    bool Near(float a, float b)
    {
        return std::fabs(a - b) < 1e-3f;
    }
}

TEST(vertex_counts_by_primitive_type)
{
    CHECK_EQ(VertexCount(5, 2), 4u); // triangle strip: one glyph quad
    CHECK_EQ(VertexCount(4, 2), 6u);
    CHECK_EQ(VertexCount(6, 3), 5u);
    CHECK_EQ(VertexCount(2, 3), 6u);
    CHECK_EQ(VertexCount(3, 3), 4u);
    CHECK_EQ(VertexCount(1, 7), 7u);
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

namespace
{
    ScreenBox Glyph(float x0, float y0, float x1, float y1)
    {
        ScreenBox b;
        b.Add(x0, y0);
        b.Add(x1, y1);
        return b;
    }

    // "Brutal Sheep"-like nameplate: a lead quad and 11 letters, 7 px wide, 10 px tall, at y 200.
    std::vector<ScreenBox> Name(float x)
    {
        std::vector<ScreenBox> glyphs{Glyph(x - 7.0f, 200.0f, x, 210.0f)};
        for (int i = 0; i < 11; ++i)
        {
            const float left = x + i * 7.0f + (i >= 6 ? 4.0f : 0.0f); // a space after the sixth letter
            glyphs.push_back(Glyph(left, 200.0f, left + 7.0f, 210.0f));
        }
        return glyphs;
    }
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
    glyphs.push_back(Glyph(400.0f, 520.0f, 407.0f, 530.0f));
    const ScreenBox far = NameplateFromGlyphs(glyphs);
    CHECK(Near(far.minX, 993.0f) && Near(far.maxX, 1081.0f) && Near(far.maxY, 210.0f));

    // Same line but well past the end of the name.
    glyphs.back() = Glyph(1200.0f, 200.0f, 1207.0f, 210.0f);
    const ScreenBox sameLine = NameplateFromGlyphs(glyphs);
    CHECK(Near(sameLine.minX, 993.0f) && Near(sameLine.maxX, 1081.0f));
}

TEST(no_glyphs_no_box)
{
    CHECK(!NameplateFromGlyphs({}).valid);
}

TEST(label_shows_for_a_steady_on_screen_nameplate_of_a_drawn_mob)
{
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(LabelVisible(&plate, 18, kStableFrames, 2560.0f, 1440.0f));
}

TEST(label_hides_when_the_camera_cannot_see_the_mob)
{
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(!LabelVisible(nullptr, 18, kStableFrames, 2560.0f, 1440.0f));
    CHECK(!LabelVisible(&plate, 0, kStableFrames, 2560.0f, 1440.0f));                // body not drawn
    CHECK(!LabelVisible(&plate, 18, kStableFrames - 1, 2560.0f, 1440.0f));           // nameplate just appeared
    const ScreenBox invalid;
    CHECK(!LabelVisible(&invalid, 18, kStableFrames, 2560.0f, 1440.0f));
}

TEST(label_stays_while_the_nameplate_crosses_the_screen_edge)
{
    // In replace mode the game's name comes back wherever ours is not drawn.
    for (const ScreenBox& edge : {Glyph(-5.0f, 200.0f, 60.0f, 210.0f), Glyph(2500.0f, 200.0f, 2570.0f, 210.0f),
             Glyph(1000.0f, -3.0f, 1080.0f, 7.0f), Glyph(1000.0f, 1435.0f, 1080.0f, 1445.0f)})
        CHECK(LabelVisible(&edge, 18, kStableFrames, 2560.0f, 1440.0f));
}

TEST(label_hides_when_the_nameplate_is_off_screen)
{
    for (const ScreenBox& off : {Glyph(-90.0f, 200.0f, -10.0f, 210.0f), Glyph(2570.0f, 200.0f, 2650.0f, 210.0f),
             Glyph(1000.0f, -20.0f, 1080.0f, -10.0f), Glyph(1000.0f, 1450.0f, 1080.0f, 1460.0f)})
        CHECK(!LabelVisible(&off, 18, kStableFrames, 2560.0f, 1440.0f));
}

TEST(a_huge_in_scene_quad_is_not_a_nameplate)
{
    // A screen-sized in-scene quad that the game sometimes credits to whichever mob is on the stack.
    const ScreenBox huge = Glyph(667.0f, 401.0f, 2731.0f, 563.0f);
    CHECK(!NameplateFromGlyphs({huge}).valid);

    auto glyphs = Name(1000.0f);
    glyphs.push_back(huge);
    const ScreenBox box = NameplateFromGlyphs(glyphs);
    CHECK(Near(box.minX, 993.0f) && Near(box.maxX, 1081.0f) && Near(box.maxY, 210.0f));
}

TEST(fewer_than_three_glyphs_are_not_a_nameplate)
{
    CHECK(!NameplateFromGlyphs({Glyph(10, 10, 17, 20), Glyph(17, 10, 24, 20)}).valid);
    CHECK(NameplateFromGlyphs({Glyph(10, 10, 17, 20), Glyph(17, 10, 24, 20), Glyph(24, 10, 31, 20)}).valid);
}

TEST(a_replaced_names_letters_are_hidden_wherever_they_are)
{
    // Names move between frames and the sub-target's is drawn larger, so neither where nor how big matters.
    CHECK(HideReplacedGlyph(Glyph(1010.0f, 201.0f, 1017.0f, 211.0f), true, 10.0f, {}));
    CHECK(HideReplacedGlyph(Glyph(400.0f, 520.0f, 407.0f, 530.0f), true, 10.0f, {}));
    CHECK(HideReplacedGlyph(Glyph(1010.0f, 201.0f, 1021.0f, 216.0f), true, 10.0f, {}));
    // Not the screen-sized quad the game sometimes credits to a mob.
    CHECK(!HideReplacedGlyph(Glyph(667.0f, 401.0f, 2731.0f, 563.0f), true, 10.0f, {}));
}

TEST(a_letter_in_a_kept_name_stays_whoever_it_is_credited_to)
{
    // A letter of a kept name, credited to a replaced mob by a stale stack pointer.
    const ScreenBox kept = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(!HideReplacedGlyph(Glyph(1010.0f, 201.0f, 1017.0f, 211.0f), true, 10.0f, {kept}));
    CHECK(HideReplacedGlyph(Glyph(400.0f, 520.0f, 407.0f, 530.0f), true, 10.0f, {kept}));
}

TEST(a_replaced_names_icons_are_hidden_wherever_they_are)
{
    // From a capture: a square icon 1.5 letters tall, in another texture.
    CHECK(HideReplacedGlyph(Glyph(985.0f, 198.0f, 1000.0f, 213.0f), false, 10.0f, {}));
    CHECK(HideReplacedGlyph(Glyph(800.0f, 640.0f, 815.0f, 655.0f), false, 10.0f, {}));
    // A close player: letters 34 px tall and the icons as one 90x51 strip, wider than any letter.
    CHECK(HideReplacedGlyph(Glyph(1091.0f, 634.0f, 1181.0f, 685.0f), false, 34.0f, {}));
    // The sub-target's, drawn larger than its letters were the frame before.
    CHECK(HideReplacedGlyph(Glyph(1091.0f, 634.0f, 1226.0f, 710.0f), false, 34.0f, {}));
    // Not a quad many times the letters' height, such as the one the game draws over the whole model.
    CHECK(!HideReplacedGlyph(Glyph(667.0f, 600.0f, 1400.0f, 900.0f), false, 34.0f, {}));
    // Not before any letters have been seen to size them by.
    CHECK(!HideReplacedGlyph(Glyph(985.0f, 198.0f, 1000.0f, 213.0f), false, 0.0f, {}));
}

TEST(a_stray_letter_is_hidden_only_in_a_replaced_name)
{
    const ScreenBox replaced = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    // Credited to no one: the first quad of a name is often credited to the entity drawn before it.
    CHECK(HideStrayLetter(Glyph(1010.0f, 201.0f, 1017.0f, 211.0f), nullptr, {replaced}));
    CHECK(HideStrayLetter(Glyph(990.0f, 214.0f, 997.0f, 224.0f), nullptr, {replaced})); // moved a little
    CHECK(!HideStrayLetter(Glyph(400.0f, 520.0f, 407.0f, 530.0f), nullptr, {replaced}));
    CHECK(!HideStrayLetter(Glyph(1010.0f, 201.0f, 1017.0f, 211.0f), nullptr, {}));
    // A 24 px quad just above the name, such as a target cursor.
    CHECK(!HideStrayLetter(Glyph(1028.0f, 172.0f, 1052.0f, 196.0f), nullptr, {replaced}));
}

TEST(a_kept_names_letters_inside_it_are_never_hidden)
{
    // A kept player's name overlapping a replaced mob's, as in melee, and one beside it on the same line.
    const ScreenBox mob    = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    const ScreenBox player = Glyph(1040.0f, 196.0f, 1110.0f, 206.0f);
    CHECK(!HideStrayLetter(Glyph(1050.0f, 197.0f, 1057.0f, 207.0f), &player, {mob}));
    const ScreenBox beside = Glyph(1090.0f, 200.0f, 1150.0f, 210.0f);
    CHECK(!HideStrayLetter(Glyph(1090.0f, 200.0f, 1097.0f, 210.0f), &beside, {mob}));
}

TEST(a_glyph_beside_a_name_is_one_of_its_icons)
{
    const ScreenBox name = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(BesideName(Glyph(985.0f, 198.0f, 1000.0f, 213.0f), name));
    CHECK(BesideName(Glyph(925.0f, 198.0f, 940.0f, 213.0f), name)); // a row of them
    CHECK(!BesideName(Glyph(800.0f, 198.0f, 815.0f, 213.0f), name)); // far away
    CHECK(!BesideName(Glyph(985.0f, 240.0f, 1000.0f, 255.0f), name)); // below
}

TEST(the_games_cursor_sits_on_its_anchor)
{
    // From captures: the game's cursor quads in the UI image and the anchors its target window held.
    CHECK(AtCursorAnchor(Glyph(1185.5f, 216.5f, 1205.5f, 248.5f), 1196.0f, 249.0f));
    CHECK(AtCursorAnchor(Glyph(713.5f, 300.5f, 733.5f, 332.5f), 724.0f, 333.0f));
    CHECK(AtCursorAnchor(Glyph(713.5f, 290.5f, 733.5f, 322.5f), 724.0f, 333.0f)); // bobbing
    CHECK(!AtCursorAnchor(Glyph(743.5f, 300.5f, 763.5f, 332.5f), 724.0f, 333.0f));
    CHECK(!AtCursorAnchor(Glyph(713.5f, 380.5f, 733.5f, 412.5f), 724.0f, 333.0f));
    // The target window's text, credited to the target too.
    CHECK(!AtCursorAnchor(Glyph(1796.5f, 995.5f, 1805.5f, 1006.5f), 1196.0f, 249.0f));
}

TEST(the_target_arrow_is_taller_than_wide)
{
    // From a capture: the target arrow, and the menu's pointer drawn from the same texture.
    CHECK(LooksLikeTargetArrow(Glyph(1185.5f, 216.5f, 1205.5f, 248.5f)));
    CHECK(!LooksLikeTargetArrow(Glyph(4.5f, 810.5f, 36.5f, 830.5f)));
}

TEST(a_name_shows_whenever_some_of_it_is_on_screen)
{
    // Unlike labels, from its first frame and without its body: the game's own name is hidden either way.
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    CHECK(NameOnScreen(&plate, 2560.0f, 1440.0f));
    CHECK(!LabelVisible(&plate, 0, 1, 2560.0f, 1440.0f));
    const ScreenBox above = Glyph(1000.0f, -40.0f, 1080.0f, -30.0f);
    CHECK(!NameOnScreen(&above, 2560.0f, 1440.0f));
    CHECK(!NameOnScreen(nullptr, 2560.0f, 1440.0f));
}

TEST(name_icons_line_up_against_the_name)
{
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes{100.0f, 18.0f, 0.0f, 0.0f, 0, 16.0f};
    sizes.nameIconCount = 2;
    sizes.nameIconSize  = 16.0f;
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.nameX, 990.0f));                // the name stays centered on the game's
    CHECK(Near(l.nameIconsX, 954.0f));           // two 16 px icons and their gap, 2 px left of it
    CHECK(Near(l.nameIconsY, 197.0f));           // centered on the name's 18 px
    CHECK(Near(l.nameIconStep, 18.0f));
}

TEST(a_name_and_its_icons_can_be_centered_together)
{
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes{100.0f, 18.0f, 0.0f, 0.0f, 0, 16.0f, 20.0f, 16.0f};
    sizes.nameIconCount     = 2;
    sizes.nameIconSize      = 16.0f;
    sizes.centerNameAndIcons = true;
    const NameplateLayout l = LayoutNameplate(plate, sizes, true);
    CHECK(Near(l.nameIconsX, 972.0f)); // 34 px of icons, a 2 px gap and the 100 px name, centered on 1040
    CHECK(Near(l.nameX, 1008.0f));
    CHECK(Near(l.cursorX, 1030.0f)); // the cursor stays over the center
    sizes.nameIconCount = 0;
    CHECK(Near(LayoutNameplate(plate, sizes, true).nameX, 990.0f));
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
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    const std::vector<GlyphDraw> glyphs{{Glyph(1000, 200, 1007, 210), 0xFFFFFF80, 0, 0.0f}, {Glyph(1007, 200, 1014, 210), 0xFFFFFF80, 0, 0.0f},
        {Glyph(1014, 200, 1021, 210), 0xFF000000, 0, 0.0f}, {Glyph(400, 520, 407, 530), 0xFFFF0000, 0, 0.0f}, {Glyph(410, 520, 417, 530), 0xFFFF0000, 0, 0.0f},
        {Glyph(420, 520, 427, 530), 0xFFFF0000, 0, 0.0f}};
    CHECK_EQ(NameColor(glyphs, plate), 0xFFFFFF80u); // letters outside the nameplate do not count
    CHECK_EQ(NameColor({}, plate), 0xFFFFFFFFu);
}

TEST(a_names_depth_comes_from_its_letters)
{
    // From a capture looking steeply down: Jax's letters at 0.98687, the game's icon strip beside them, and a large quad
    // the game drew into another image at 0.98736, credited to Jax that frame. Behind the letters, it would put our
    // nameplate behind the top of Jax's head.
    constexpr uintptr_t kFont = 1, kIcons = 2, kOther = 3;
    const std::vector<GlyphDraw> glyphs{{Glyph(1732.7f, 471.4f, 1830.6f, 526.4f), kWhite, kIcons, 0.98687f},
        {Glyph(1000.0f, 474.0f, 1936.0f, 687.0f), kWhite, kOther, 0.98736f},
        {Glyph(1830.6f, 478.7f, 1850.1f, 504.4f), kWhite, kFont, 0.98687f},
        {Glyph(1850.1f, 478.7f, 1902.3f, 515.4f), kWhite, kFont, 0.98687f}};
    CHECK_EQ(NameDepth(glyphs, kFont), 0.98687f);
    CHECK_EQ(NameDepth({}, kFont), 0.0f);
}

TEST(replace_mode_stacks_icons_label_and_name)
{
    // Game name centered at (1040, 205). Name 100x18, label 80x16, three 16 px icons.
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout l = LayoutNameplate(plate, LineSizes{100.0f, 18.0f, 80.0f, 16.0f, 3, 16.0f}, true);
    CHECK(Near(l.nameX, 990.0f) && Near(l.nameY, 196.0f));   // centered on the game's name
    CHECK(Near(l.labelX, 1000.0f) && Near(l.labelY, 183.0f)); // overlapping the name's text box by 3 px
    CHECK(Near(l.iconsX, 1014.0f) && Near(l.iconsY, 165.0f)); // 52 px wide row, 2 px above the label
    CHECK(Near(l.iconStep, 18.0f));
}

TEST(label_mode_stacks_above_the_game_name)
{
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout l = LayoutNameplate(plate, LineSizes{0.0f, 0.0f, 80.0f, 16.0f, 1, 16.0f}, false);
    CHECK(Near(l.labelX, 1000.0f) && Near(l.labelY, 182.0f)); // 2 px above the game's name
    CHECK(Near(l.iconsX, 1032.0f) && Near(l.iconsY, 164.0f));
    const NameplateLayout iconsOnly = LayoutNameplate(plate, LineSizes{0.0f, 0.0f, 0.0f, 0.0f, 2, 16.0f}, false);
    CHECK(Near(iconsOnly.iconsY, 182.0f));
}

TEST(the_cursor_sits_above_the_top_line)
{
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    const NameplateLayout withIcons =
        LayoutNameplate(plate, LineSizes{100.0f, 18.0f, 80.0f, 16.0f, 3, 16.0f, 20.0f, 16.0f}, true);
    CHECK(Near(withIcons.cursorX, 1030.0f) && Near(withIcons.cursorY, 147.0f)); // 2 px above the icons at 165
    const NameplateLayout labelOnly =
        LayoutNameplate(plate, LineSizes{0.0f, 0.0f, 80.0f, 16.0f, 0, 16.0f, 20.0f, 16.0f}, false);
    CHECK(Near(labelOnly.cursorY, 164.0f)); // 2 px above the label at 182
}

TEST(the_cursors_point_sits_over_the_center)
{
    const ScreenBox plate = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    LineSizes sizes{0.0f, 0.0f, 80.0f, 16.0f, 0, 16.0f, 20.0f, 16.0f};
    sizes.cursorTip = 0.25f; // a shape that points from a quarter of its width
    CHECK(Near(LayoutNameplate(plate, sizes, false).cursorX, 1035.0f));
}

TEST(the_games_cursor_is_the_quad_right_above_the_name)
{
    // From a capture: Goblin Tinkerer's name at (1407,261)-(1472,267) and the game's 20x32 cursor at
    // (1068.5,162.5)-(1088.5,194.5) in the 1920x1080 UI, which is 4/3 smaller than the 2560x1440 screen.
    const ScreenBox name   = Glyph(1407.0f, 261.0f, 1472.0f, 267.0f);
    const ScreenBox cursor = Glyph(1068.5f, 162.5f, 1088.5f, 194.5f).Scaled(4.0f / 3.0f, 4.0f / 3.0f);
    CHECK(IsGameCursor(cursor, name));
    CHECK(!IsGameCursor(Glyph(1484.7f, 216.7f, 1511.3f, 259.3f), name));        // beside the name
    CHECK(!IsGameCursor(Glyph(1424.7f, 126.7f, 1451.3f, 169.3f), name));        // well above it
    CHECK(!IsGameCursor(Glyph(1424.7f, 256.7f, 1451.3f, 299.3f), name));        // reaching below its top
    CHECK(!IsGameCursor(Glyph(0.0f, 0.0f, 2560.0f, 1440.0f), name));          // the screen-sized copy
    CHECK(!IsGameCursor(Glyph(48.1f, 855.8f, 98.8f, 878.8f), name));           // the target window
}

TEST(the_games_cursor_is_centered_on_a_players_name_and_icons)
{
    // From a capture: Shio's letters at (933.9,444.8)-(1054.8,466.7) with the game's icon strip against their left, and
    // the game's cursor at (713.5,300.5)-(733.5,332.5) in the 1920x1080 UI.
    const ScreenBox letters = Glyph(933.9f, 444.8f, 1054.8f, 466.7f);
    const ScreenBox icons   = Glyph(875.4f, 444.8f, 933.9f, 477.7f);
    const ScreenBox cursor  = Glyph(713.5f, 300.5f, 733.5f, 332.5f).Scaled(4.0f / 3.0f, 4.0f / 3.0f);
    CHECK(BesideName(icons, letters));
    ScreenBox whole = letters;
    whole.Add(icons);
    CHECK(IsGameCursor(cursor, whole));
    CHECK(!IsGameCursor(cursor, letters)); // centered on the letters alone, it would be missed
}

TEST(a_name_is_centered_over_its_entity_not_its_letters)
{
    // Shio: the game centers the letters and its icons together over the player, at x 965.1.
    const ScreenBox letters  = Glyph(933.9f, 444.8f, 1054.8f, 466.7f);
    const ScreenBox whole    = Glyph(875.4f, 444.8f, 1054.8f, 477.7f);
    const ScreenBox centered = CenteredOver(letters, whole);
    CHECK(Near(centered.CenterX(), 965.1f));
    CHECK(Near(centered.Width(), letters.Width()) && Near(centered.minY, letters.minY) && Near(centered.maxY, letters.maxY));
    CHECK(Near(CenteredOver(letters, letters).minX, letters.minX)); // no icons: nothing moves
}

TEST(the_cursor_bobs_a_little_and_repeats)
{
    for (double t = 0.0; t < 3.0; t += 0.05)
    {
        const float bob = CursorBob(t, 20.0f);
        CHECK(bob <= 0.0f && bob >= -3.0f); // at most 15% of its height, upward
        CHECK(Near(bob, CursorBob(t + 1.2, 20.0f)));
    }
}

TEST(scale_follows_the_game_name_with_limits)
{
    // 1440p: a typical game letter is 8 px (1440 / 180).
    CHECK(Near(DistanceScale(8.0f, 1440.0f), 1.0f));
    CHECK(Near(DistanceScale(10.0f, 1440.0f), 1.25f)); // smoothly, not in whole pixels
    CHECK(Near(DistanceScale(100.0f, 1440.0f), 2.5f)); // at most 2.5x
    CHECK(Near(DistanceScale(1.0f, 1440.0f), 0.5f));   // at least 0.5x
    CHECK(Near(DistanceScale(std::nanf(""), 1440.0f), 1.0f));
    CHECK(Near(DistanceScale(8.0f, 0.0f), 1.0f));
}

TEST(text_is_redrawn_only_when_its_size_moves_a_step)
{
    CHECK_EQ(RasterHeight(15.0f, 0), 18);  // the first size at or above it: 6 px times 1.25 steps
    CHECK_EQ(RasterHeight(15.0f, 18), 18); // shrunk by less than a step: drawn smaller
    CHECK_EQ(RasterHeight(14.5f, 18), 18);
    CHECK_EQ(RasterHeight(18.8f, 18), 18); // grown by less than 5%: drawn a little larger
    CHECK_EQ(RasterHeight(19.0f, 18), 23);
    CHECK_EQ(RasterHeight(14.0f, 18), 15);
    CHECK_EQ(RasterHeight(15.2f, 15), 15); // and back up is not a step yet: no flicker at the edge
    CHECK_EQ(RasterHeight(2.0f, 0), 6);    // the smallest
}
