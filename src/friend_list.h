#pragma once

#include "native_locate.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace headsup
{
    // The game's own friend list (Communication > Friend List), which Phoenix's profile server fills: an array of
    // slots in FFXiMain's data, 200 for friends and then the block list's. A slot, from its start:
    //   +0x00  the friend's account, doubled and odd; 0 for an empty slot
    //   +0x08  the name they were added by, 16 bytes
    //   +0x1C  the character they are playing, 16 bytes, and +0x30 that character's id, which is its entity's
    //          server id; both stay after they log off. A request not yet accepted never has them.
    //   +0x64  1 while they are playing
    //   +0x70  the slot's own number: four times it (from 1) as a word, then sixteen times it in the next word's
    //          low twelve bits. Every slot holds these, filled or not.
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
