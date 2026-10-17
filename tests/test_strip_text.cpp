#include "fake_memory.h"
#include "native_locate.h"
#include "strip_text.h"
#include "test.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

using namespace headsup;

namespace
{
    // A text object showing `level`, each level's buffer as given.
    std::vector<uint8_t> Object(uint32_t level, std::vector<std::string> buffers, uint32_t shown = 1)
    {
        std::vector<uint8_t> o(kStripObjectBytes, 0);
        std::memcpy(o.data() + kStripShown, &shown, sizeof(shown));
        std::memcpy(o.data() + kStripLevel, &level, sizeof(level));
        for (size_t l = 0; l < buffers.size(); ++l)
            std::memcpy(o.data() + kStripBuffers + l * kStripStride, buffers[l].data(),
                std::min<size_t>(buffers[l].size(), kStripStride));
        return o;
    }

    std::string Shown(const std::vector<uint8_t>& o) { return ShownCp932(o.data(), o.size()); }
}

TEST(the_strip_shows_the_level_the_game_picked)
{
    CHECK_EQ(Shown(Object(1, {"Low", "Commands"})), std::string("Commands"));
    CHECK_EQ(Shown(Object(0, {"Low", "", "High"})), std::string("Low")); // the object's level, not the highest
    CHECK(Shown(Object(1, {"Low", "Commands"}, 0)).empty());              // nothing shown
    CHECK(Shown(Object(4, {"Low", "Commands"})).empty());                 // a level the game never draws
    CHECK(ShownCp932(nullptr, kStripObjectBytes).empty() && Shown(std::vector<uint8_t>(kStripObjectBytes - 1)).empty());
}

TEST(a_cleared_level_is_empty_though_the_game_leaves_its_old_text_after_the_nul)
{
    // The game clears a level by writing a NUL to the first byte only.
    CHECK(Shown(Object(0, {std::string("\0elect a target to auto-attack.", 31)})).empty());
    CHECK(Shown(Object(2, {"", "", std::string("\0Commands", 9)})).empty());
}

TEST(text_still_in_the_buffer_is_not_shown_while_the_shown_flag_is_zero)
{
    CHECK(Shown(Object(0, {"Select a target to auto-attack."}, 0)).empty());
    CHECK_EQ(Shown(Object(0, {"Select a target to auto-attack."}, 1)), std::string("Select a target to auto-attack."));
}

TEST(a_strip_line_drops_the_no_width_byte_and_the_scroll_pad)
{
    CHECK_EQ(Shown(Object(2, {"", "", "Sel\x04" "ect a target."})), std::string("Select a target."));
    CHECK_EQ(Shown(Object(2, {"", "", "A line wider than its box  "})), std::string("A line wider than its box"));
    CHECK(Shown(Object(2, {"", "", "   "})).empty());
}

TEST(a_full_buffer_stops_at_its_size_without_a_nul)
{
    std::vector<uint8_t> o = Object(0, {});
    std::memset(o.data() + kStripBuffers, 'x', kStripStride); // no NUL in level 0's 0xCB bytes
    o[kStripBuffers + kStripStride] = 'y';                     // level 1's first byte
    CHECK_EQ(Shown(o), std::string(kStripBufferBytes, 'x'));
}

TEST(ascii_needs_no_conversion_and_anything_else_goes_through_it_whole)
{
    int calls = 0;
    auto convert = [&](const std::string& bytes) {
        ++calls;
        return bytes == "\x81\x48\x81\x40\x81\x48\x81\x40\x81\x48" ? std::string("\xEF\xBC\x9F\xE3\x80\x80\xEF\xBC\x9F"
                                                                             "\xE3\x80\x80\xEF\xBC\x9F")
                                                                       : std::string("wrong");
    };
    CHECK_EQ(Cp932ToUtf8("Commands", convert), std::string("Commands"));
    CHECK_EQ(calls, 0);
    CHECK_EQ(Cp932ToUtf8("\x81\x48\x81\x40\x81\x48\x81\x40\x81\x48", convert),
             std::string("\xEF\xBC\x9F\xE3\x80\x80\xEF\xBC\x9F\xE3\x80\x80\xEF\xBC\x9F")); // ？　？　？ in UTF-8
    CHECK_EQ(calls, 1);
}

TEST(a_lead_byte_cut_off_at_the_end_of_a_full_buffer_is_dropped)
{
    std::vector<uint8_t> o = Object(0, {});
    std::memset(o.data() + kStripBuffers, 'x', kStripBufferBytes - 1);
    o[kStripBuffers + kStripBufferBytes - 1] = 0xE0; // a two-byte character whose second byte did not fit
    CHECK_EQ(Shown(o), std::string(kStripBufferBytes - 1, 'x'));
    CHECK_EQ(Shown(Object(0, {"A\x81\x48\x81"})), std::string("A\x81\x48"));      // the first pair is whole
    CHECK_EQ(Shown(Object(0, {"A\x81\x48"})), std::string("A\x81\x48"));          // a whole pair stays
    CHECK_EQ(Shown(Object(0, {"A\x81\x81"})), std::string("A\x81\x81"));          // 0x81 as a trail byte is no lead
    CHECK_EQ(Shown(Object(0, {"A\xB1"})), std::string("A\xB1"));                  // half-width katakana is one byte
}

namespace
{
    constexpr uint32_t kSlot = 0x00401000, kTextAt = 0x100;
    constexpr uint32_t kOwnerMcdAt = 0x08, kMcdOwnerAt = 0x0C, kDescriptorAt = 0x04, kNameAt = 0x46;

    // A help window: its owner at owner, the owner's record at mcd, the record's descriptor naming it, the slot
    // pointing at the owner, and its text object showing text.
    void Window(FakeMemory& m, uint32_t owner, uint32_t mcd, uint32_t descriptor, const char* name,
                const std::string& text)
    {
        m.Map(owner, kTextAt + kStripObjectBytes);
        m.Put32(owner + kOwnerMcdAt, mcd);
        const std::vector<uint8_t> object = Object(0, {text});
        m.Put(owner + kTextAt, object.data(), object.size());
        m.Map(mcd, 0x10);
        m.Put32(mcd + kMcdOwnerAt, owner);
        m.Put32(mcd + kDescriptorAt, descriptor);
        m.Map(descriptor, kNameAt + kWindowNameLength);
        m.Put(descriptor + kNameAt, name, kWindowNameLength);
        m.Put32(kSlot, owner);
    }

    std::string Read(StripWindowReader& reader, FakeMemory& m, const char*& why)
    {
        StripObject object{};
        why = reader.Read(kSlot, kTextAt, StripWindow::Help, m.Access(), object);
        return why == nullptr ? ShownCp932(object.data(), object.size()) : "";
    }
}

TEST(a_strip_window_is_checked_once_then_read_each_frame_without_asking_windows)
{
    FakeMemory m;
    m.Map(kSlot, sizeof(uint32_t));
    Window(m, 0x20000, 0x30000, 0x40000, "menu    helpwind", "Commands");
    StripWindowReader reader;
    const char* why = nullptr;
    CHECK_EQ(Read(reader, m, why), std::string("Commands"));
    const int checked = m.queries;
    CHECK_EQ(checked, 4); // the owner, its text object, its record and the record's name
    for (int frame = 0; frame < 100; ++frame)
        CHECK_EQ(Read(reader, m, why), std::string("Commands"));
    CHECK_EQ(m.queries, checked);
    // The text changing in place is read without a check.
    const std::vector<uint8_t> object = Object(0, {"Items"});
    m.Put(0x20000 + kTextAt, object.data(), object.size());
    CHECK_EQ(Read(reader, m, why), std::string("Items"));
    CHECK_EQ(m.queries, checked);
}

TEST(a_new_owner_is_checked_before_anything_is_read_from_it)
{
    FakeMemory m;
    m.Map(kSlot, sizeof(uint32_t));
    Window(m, 0x20000, 0x30000, 0x40000, "menu    helpwind", "Commands");
    StripWindowReader reader;
    const char* why = nullptr;
    CHECK_EQ(Read(reader, m, why), std::string("Commands"));
    const int checked = m.queries;
    // The game frees the window and the slot points at memory that is not there: hidden, never read.
    m.Free(0x20000);
    m.Put32(kSlot, 0x50000);
    CHECK(Read(reader, m, why).empty() && why != nullptr);
    CHECK(m.queries > checked);
    // A new window elsewhere is checked, then read.
    Window(m, 0x60000, 0x70000, 0x40000, "menu    helpwind", "Magic");
    CHECK_EQ(Read(reader, m, why), std::string("Magic"));
    // No window (the title screen) asks nothing; one coming back is checked again.
    m.Put32(kSlot, 0);
    const int before = m.queries;
    CHECK(Read(reader, m, why).empty() && why != nullptr && m.queries == before);
    m.Put32(kSlot, 0x60000);
    CHECK_EQ(Read(reader, m, why), std::string("Magic"));
    CHECK(m.queries > before);
}

TEST(a_strip_window_whose_record_changes_or_does_not_match_is_checked_again_and_hidden)
{
    FakeMemory m;
    m.Map(kSlot, sizeof(uint32_t));
    Window(m, 0x20000, 0x30000, 0x40000, "menu    helpwind", "Commands");
    StripWindowReader reader;
    const char* why = nullptr;
    CHECK_EQ(Read(reader, m, why), std::string("Commands"));
    // The owner's record moves to one that is not there.
    m.Put32(0x20000 + kOwnerMcdAt, 0x90000);
    int before = m.queries;
    CHECK(Read(reader, m, why).empty() && why != nullptr && m.queries > before);
    // A record that does not point back, then one naming another window.
    Window(m, 0x20000, 0xA0000, 0xB0000, "menu    titlewin", "Commands");
    m.Put32(0xA0000 + kMcdOwnerAt, 0x12345);
    CHECK(Read(reader, m, why).empty() && why != nullptr);
    m.Put32(0xA0000 + kMcdOwnerAt, 0x20000);
    before = m.queries;
    CHECK(Read(reader, m, why).empty() && why != nullptr && m.queries > before); // a failed check is tried again
    m.Put(0xB0000 + kNameAt, "menu    helpwind", kWindowNameLength);
    CHECK_EQ(Read(reader, m, why), std::string("Commands"));
}
