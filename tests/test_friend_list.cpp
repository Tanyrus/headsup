#include "friend_list.h"
#include "test.h"

#include <cstdint>
#include <cstring>
#include <vector>

using namespace headsup;

namespace
{
    constexpr size_t kBefore = 0x4A8; // what lies before the list in the data, any length that keeps it aligned

    void Put(std::vector<uint8_t>& data, size_t at, const char* hex)
    {
        for (; hex[0] != '\0' && hex[1] != '\0'; hex += 2)
        {
            if (hex[0] == ' ')
            {
                --hex;
                continue;
            }
            const auto digit = [](char c) { return static_cast<uint8_t>(c <= '9' ? c - '0' : c - 'a' + 10); };
            data[at++] = static_cast<uint8_t>(digit(hex[0]) << 4 | digit(hex[1]));
        }
    }

    // The game's data around its friend list as read from the test client's memory on 2026-10-13: a friend who is
    // playing, a request still pending, the rest empty, and after the 200 friend slots a block list of one account.
    std::vector<uint8_t> Data()
    {
        std::vector<uint8_t> data(kBefore + kFriendSlotBytes * 210, 0);
        std::memset(data.data(), 0xFF, kBefore - 0x80); // the bytes before the list's own header were all FF
        const size_t first = kBefore;
        for (int slot = 0; slot < 210; ++slot)
        {
            // Every slot's own number, whether it is filled or not; the block list counts from 1 again, with no
            // second number.
            const size_t at = first + kFriendSlotBytes * static_cast<size_t>(slot) + 0x70;
            const auto number = static_cast<uint16_t>(4 * ((slot < kFriendSlots ? slot : slot - kFriendSlots) + 1));
            const auto wide   = static_cast<uint16_t>(slot < kFriendSlots ? 16 * (slot + 1) : 0);
            std::memcpy(&data[at], &number, 2);
            std::memcpy(&data[at + 2], &wide, 2);
        }
        // Slot 0: Headsuptwo (account 1001), playing the character of that name, id 65002, WAR30/MNK15 in zone 100.
        Put(data, first + 0x00, "d3070000 00000000 48656164737570 74776f");
        Put(data, first + 0x18, "7f41c100 48656164737570 74776f");
        Put(data, first + 0x30, "eafd0000 00020100 01021e0f 00000000 64000000 02000000");
        Put(data, first + 0x64, "01000000 ac2d481b 00000000 040010d0 01000000");
        // Slot 1: Headsupthree (account 1002), whose request is pending: a name and nothing more.
        Put(data, first + 0x100, "d5070000 00000000 48656164737570 7468726565");
        // Slot 200, the block list's first: Headsupfour (account 1003).
        Put(data, first + kFriendSlotBytes * kFriendSlots, "d7070000 00000000 48656164737570 666f7572");
        return data;
    }

    ImageSection Section(const std::vector<uint8_t>& data)
    {
        return ImageSection{0x34F000, data.data(), static_cast<uint32_t>(data.size())};
    }
}

TEST(the_friend_list_is_found_by_the_numbers_its_slots_hold)
{
    const std::vector<uint8_t> data = Data();
    CHECK(LocateFriendList(Section(data)) == static_cast<uint32_t>(kBefore));
    // A client whose data holds no such list, or too little to hold one, has none.
    const std::vector<uint8_t> blank(data.size(), 0);
    CHECK(!LocateFriendList(Section(blank)));
    CHECK(!LocateFriendList(ImageSection{0, data.data(), 0x400}));
    CHECK(!LocateFriendList(ImageSection{}));
    // One slot numbered otherwise breaks the run: it is no list of this build's.
    std::vector<uint8_t> odd = data;
    odd[kBefore + kFriendSlotBytes * 3 + 0x70] = 0x11;
    CHECK(!LocateFriendList(Section(odd)));
    // Two such runs, and neither is taken: which is the list cannot be told.
    std::vector<uint8_t> twice = data;
    twice.insert(twice.end(), data.begin(), data.end());
    CHECK(!LocateFriendList(Section(twice)));
    // A run with less than a whole list of data after its start is none: its last slots would be read past the end.
    std::vector<uint8_t> cut = data;
    cut.resize(kBefore + kFriendSlotBytes * (kFriendSlots - 1));
    cut.shrink_to_fit();
    CHECK(!LocateFriendList(Section(cut)));
    // The block list numbers its slots from 1 again, and in the client more data follows it. Only the second
    // number, which it lacks, tells it from a second friend list.
    std::vector<uint8_t> blocks = data;
    blocks.resize(kBefore + kFriendSlotBytes * (2 * kFriendSlots + 10), 0);
    for (int slot = 0; slot < kFriendSlots; ++slot)
    {
        const auto number = static_cast<uint16_t>(4 * (slot + 1));
        std::memcpy(&blocks[kBefore + kFriendSlotBytes * static_cast<size_t>(kFriendSlots + slot) + 0x70], &number, 2);
    }
    CHECK(LocateFriendList(Section(blocks)) == static_cast<uint32_t>(kBefore));
}

TEST(a_friends_character_is_known_once_the_server_has_said_who_they_play)
{
    std::vector<uint8_t> data = Data();
    const uint8_t* first      = data.data() + kBefore;
    // The friend playing; not the one whose request is pending, nor anyone on the block list.
    CHECK((FriendCharacters(first) == std::vector<uint32_t>{65002}));
    // Logged off, the slot keeps the character: seen again before the server says so, they are still a friend.
    data[kBefore + 0x64] = 0;
    CHECK((FriendCharacters(first) == std::vector<uint32_t>{65002}));
    // A friend removed leaves an empty slot, whatever else it still holds.
    std::memset(&data[kBefore], 0, 8);
    CHECK(FriendCharacters(first).empty());
    // The block list's first slot is no friend's, whatever character it comes to hold.
    Put(data, kBefore + kFriendSlotBytes * kFriendSlots + 0x30, "eefd0000");
    CHECK(FriendCharacters(first).empty());
    // Several come in ascending order, which the lookup counts on.
    std::vector<uint8_t> more = Data();
    Put(more, kBefore + 0x100 + 0x30, "e9030000");
    Put(more, kBefore + 0x500, "0b000000 00000000");
    Put(more, kBefore + 0x500 + 0x30, "40420f00");
    const std::vector<uint32_t> characters = FriendCharacters(more.data() + kBefore);
    CHECK((characters == std::vector<uint32_t>{1001, 65002, 1000000}));
    CHECK(IsFriend(characters, 65002) && IsFriend(characters, 1001) && IsFriend(characters, 1000000));
    CHECK(!IsFriend(characters, 65001) && !IsFriend(characters, 0) && !IsFriend({}, 65002));
}
