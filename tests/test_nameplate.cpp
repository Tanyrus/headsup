#include "nameplate.h"
#include "test.h"

#include <cmath>
#include <vector>

using namespace aggroglow;

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
    const auto quad = Quad(389.016f, 601.077f, 396.38f, 610.281f, 0.997f);
    ScreenBox box;
    CHECK(WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
    CHECK(box.valid);
    CHECK(Near(box.minX, 389.016f) && Near(box.maxX, 396.38f));
    CHECK(Near(box.minY, 601.077f) && Near(box.maxY, 610.281f));
}

TEST(hud_text_at_depth_zero_is_rejected)
{
    // The target bar's copy of a mob's name: HUD text at depth 0.
    const auto quad = Quad(45.5f, 881.5f, 50.5f, 891.5f, 0.0f);
    ScreenBox box;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
    CHECK(!box.valid);
}

TEST(any_vertex_outside_the_scene_depth_rejects_the_draw)
{
    auto quad = Quad(10, 10, 20, 20, 0.5f);
    quad[3].z = 1.0f;
    ScreenBox box;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
    quad[3].z = std::nanf("");
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
    quad[3].z = 0.5f;
    quad[3].x = INFINITY;
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 4, box));
}

TEST(unusable_draws_are_rejected)
{
    const auto quad = Quad(10, 10, 20, 20, 0.5f);
    ScreenBox box;
    CHECK(!WorldTextBox(nullptr, sizeof(Vertex), 4, box));
    CHECK(!WorldTextBox(quad.data(), 12, 4, box));
    CHECK(!WorldTextBox(quad.data(), sizeof(Vertex), 0, box));
    std::vector<Vertex> many(kMaxTextVertices + 1, quad[0]);
    CHECK(!WorldTextBox(many.data(), sizeof(Vertex), kMaxTextVertices + 1, box));
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

TEST(a_replaced_names_letters_are_hidden)
{
    const ScreenBox replaced = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    const std::vector<ScreenBox> plates{replaced};
    CHECK(HideGlyph(Glyph(1010.0f, 201.0f, 1017.0f, 211.0f), GlyphOwner::Mob, &replaced, plates));
    // Whoever the game says drew it: the first quad of a name is often credited to the entity drawn before it.
    CHECK(HideGlyph(Glyph(1010.0f, 201.0f, 1017.0f, 211.0f), GlyphOwner::None, nullptr, plates));
    CHECK(HideGlyph(Glyph(990.0f, 214.0f, 997.0f, 224.0f), GlyphOwner::None, nullptr, plates)); // moved a little
    // Credited to the mob and near its name: the name moved further during a fast turn.
    CHECK(HideGlyph(Glyph(1100.0f, 230.0f, 1107.0f, 240.0f), GlyphOwner::Mob, &replaced, plates));
}

TEST(a_name_that_was_not_replaced_keeps_its_letters)
{
    // Our name was not drawn last frame (at the screen edge, no body, past the plate limit, or just appeared), so the
    // game's must stay.
    CHECK(!HideGlyph(Glyph(1010.0f, 201.0f, 1017.0f, 211.0f), GlyphOwner::Mob, nullptr, {}));
}

TEST(player_and_npc_letters_inside_their_own_name_are_never_hidden)
{
    // The player's name overlapping a replaced mob name, as in melee.
    const ScreenBox mob    = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    const ScreenBox player = Glyph(1040.0f, 196.0f, 1110.0f, 206.0f);
    CHECK(!HideGlyph(Glyph(1050.0f, 197.0f, 1057.0f, 207.0f), GlyphOwner::Other, &player, {mob}));
    // Next to the mob's name on the same line.
    const ScreenBox beside = Glyph(1090.0f, 200.0f, 1150.0f, 210.0f);
    CHECK(!HideGlyph(Glyph(1090.0f, 200.0f, 1097.0f, 210.0f), GlyphOwner::Other, &beside, {mob}));
}

TEST(only_letters_of_the_names_size_are_hidden)
{
    const ScreenBox replaced = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    // A 24 px quad just above the name, such as a target cursor.
    CHECK(!HideGlyph(Glyph(1028.0f, 172.0f, 1052.0f, 196.0f), GlyphOwner::None, nullptr, {replaced}));
    // The screen-sized quad the game sometimes credits to a mob.
    CHECK(!HideGlyph(Glyph(667.0f, 401.0f, 2731.0f, 563.0f), GlyphOwner::Mob, &replaced, {replaced}));
}

TEST(stray_letters_far_from_the_name_are_kept)
{
    const ScreenBox replaced = Glyph(1000.0f, 200.0f, 1080.0f, 210.0f);
    // A letter of another name, credited to the mob by a stale stack pointer.
    CHECK(!HideGlyph(Glyph(400.0f, 520.0f, 407.0f, 530.0f), GlyphOwner::Mob, &replaced, {replaced}));
    CHECK(!HideGlyph(Glyph(400.0f, 520.0f, 407.0f, 530.0f), GlyphOwner::Other, nullptr, {replaced}));
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
    const std::vector<GlyphDraw> glyphs{{Glyph(1000, 200, 1007, 210), 0xFFFFFF80}, {Glyph(1007, 200, 1014, 210), 0xFFFFFF80},
        {Glyph(1014, 200, 1021, 210), 0xFF000000}, {Glyph(400, 520, 407, 530), 0xFFFF0000}, {Glyph(410, 520, 417, 530), 0xFFFF0000},
        {Glyph(420, 520, 427, 530), 0xFFFF0000}};
    CHECK_EQ(NameColor(glyphs, plate), 0xFFFFFF80u); // letters outside the nameplate do not count
    CHECK_EQ(NameColor({}, plate), 0xFFFFFFFFu);
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

TEST(sizes_follow_the_game_name_with_limits)
{
    // 1440p: a typical game letter is 8 px (1440 / 180).
    CHECK_EQ(ScaledSize(15, 8.0f, 1440.0f), 15);
    CHECK_EQ(ScaledSize(15, 16.0f, 1440.0f), 30);
    CHECK_EQ(ScaledSize(15, 100.0f, 1440.0f), 38); // at most 2.5x
    CHECK_EQ(ScaledSize(15, 1.0f, 1440.0f), 8);    // at least 0.5x
}

TEST(sizes_change_only_in_steps_of_two)
{
    CHECK_EQ(SteppedSize(0, 15), 15); // first size
    CHECK_EQ(SteppedSize(15, 16), 15);
    CHECK_EQ(SteppedSize(15, 14), 15);
    CHECK_EQ(SteppedSize(15, 17), 17);
    CHECK_EQ(SteppedSize(15, 13), 13);
}
