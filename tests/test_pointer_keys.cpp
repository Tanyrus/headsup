#include "pointer_keys.h"
#include "test.h"

using namespace headsup;

TEST(left_and_right_ctrl_shift_and_alt_are_one_key_each)
{
    CHECK_EQ(NormalizeKey(0xA2), 0x11); // left Ctrl
    CHECK_EQ(NormalizeKey(0xA3), 0x11);
    CHECK_EQ(NormalizeKey(0xA0), 0x10); // left Shift
    CHECK_EQ(NormalizeKey(0xA5), 0x12); // right Alt
    CHECK_EQ(NormalizeKey(0x51), 0x51); // Q
}

TEST(the_pointer_stays_only_while_every_key_held_is_one_to_keep)
{
    const std::vector<uint8_t> keep = {0x11, 0x1B}; // Ctrl, Esc
    CHECK(KeepsPointer({0x11}, keep));
    CHECK(KeepsPointer({0x11, 0x1B}, keep));
    CHECK(!KeepsPointer({0x11, 0x51}, keep)); // Ctrl+Q: Q hides
    CHECK(!KeepsPointer({}, keep));           // a key already let go: as the game does
    CHECK(!KeepsPointer({0x11}, {}));
}

TEST(mouse_buttons_are_not_keys)
{
    for (const uint8_t button : {0x01, 0x02, 0x04, 0x05, 0x06})
        CHECK(MouseButton(button));
    CHECK(!MouseButton(0x1B));
}

TEST(keys_have_names_a_player_knows)
{
    CHECK(KeyName(0x11) == "Ctrl");
    CHECK(KeyName(0x12) == "Alt");
    CHECK(KeyName(0x1B) == "Esc");
    CHECK(KeyName(0x51) == "Q");
    CHECK(KeyName(0x35) == "5");
    CHECK(KeyName(0x70) == "F1");
    CHECK(KeyName(0x7B) == "F12");
    CHECK(KeyName(0x60) == "Num 0");
    CHECK(KeyName(0x26) == "Up");
    CHECK(KeyName(0xDB) == "Key DB"); // one with no name here
}

TEST(the_keys_held_are_each_key_down_once_without_mouse_buttons)
{
    // Windows reports left Ctrl down as both Ctrl and left Ctrl.
    const auto down = [](uint8_t vk) { return vk == 0x11 || vk == 0xA2 || vk == 0x51 || vk == 0x01; };
    CHECK(HeldKeys(down) == (std::vector<uint8_t>{0x11, 0x51}));
    CHECK(HeldKeys([](uint8_t) { return false; }).empty());
}

namespace
{
    constexpr uint8_t kF10 = 0x79, kF11 = 0x7A;
}

TEST(a_key_counts_once_per_press)
{
    KeyPress press;
    CHECK(!press.Pressed(kF10, false)); // the key the settings held at load, up
    CHECK(press.Pressed(kF10, true));
    CHECK(!press.Pressed(kF10, true)); // held
    CHECK(!press.Pressed(kF10, false));
    CHECK(press.Pressed(kF10, true));
}

// The settings window takes the next key pressed, so the key is down in the frame it becomes the one to watch.
TEST(the_press_that_chose_a_key_is_not_a_press_of_it)
{
    KeyPress press;
    CHECK(!press.Pressed(0, false));
    CHECK(!press.Pressed(kF10, true)); // chosen
    CHECK(!press.Pressed(kF10, true));
    CHECK(!press.Pressed(kF10, false));
    CHECK(press.Pressed(kF10, true));
    CHECK(!press.Pressed(kF11, true)); // another chosen while the first is still down
    CHECK(!press.Pressed(kF11, false));
    CHECK(press.Pressed(kF11, true));
}

TEST(no_key_is_never_pressed)
{
    KeyPress press;
    CHECK(!press.Pressed(0, true));
    CHECK(!press.Pressed(0, false));
    CHECK(!press.Pressed(0, true));
}

TEST(a_key_to_press_is_a_real_key_with_its_sides_as_one_or_none)
{
    CHECK_EQ(BindableKey(kF10), kF10);
    CHECK_EQ(BindableKey(0xA3), 0x11); // right Ctrl
    CHECK_EQ(BindableKey(0x02), 0);    // the right mouse button
    CHECK_EQ(BindableKey(0), 0);
    CHECK_EQ(BindableKey(0xFF), 0);    // past the last key
    CHECK_EQ(BindableKey(999), 0);
    CHECK_EQ(BindableKey(-1), 0);
}
