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
