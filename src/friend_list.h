#pragma once

#include "native_locate.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace headsup
{
    // The game's own friend list, which Phoenix's profile server fills: slots in FFXiMain's data, 200 for friends
    // and then the block list's. Each holds its own number, filled or not, which is how the list is found.
    constexpr size_t kFriendSlotBytes = 0x100;
    constexpr int kFriendSlots        = 200;

    // Where the list's first slot starts in the image's data, found by the numbers its slots hold of their own
    // places; nothing when the data has no such run, or more than one.
    std::optional<uint32_t> LocateFriendList(const ImageSection& data);

    // The characters the list's friends are playing or last played, by server id, in ascending order: what tells
    // a player in view for a friend.
    std::vector<uint32_t> FriendCharacters(const uint8_t* firstSlot);
    bool IsFriend(std::span<const uint32_t> characters, uint32_t serverId);
}
