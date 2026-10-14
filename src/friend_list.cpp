#include "friend_list.h"

#include "bytes.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        constexpr size_t kSlotAccount = 0x00, kSlotCharacter = 0x30, kSlotNumber = 0x70;
        constexpr uint16_t kNumberStep = 4, kWideStep = 16, kWideMask = 0x0FFF;
        constexpr int kSlotsChecked    = 8; // as many slots' numbers in a row are no accident
        constexpr size_t kScanStep     = 4; // the slots are aligned

        bool NumberedAs(const uint8_t* slotNumber, int slot)
        {
            return ReadAt<uint16_t>(slotNumber, 0) == kNumberStep * slot &&
                   (ReadAt<uint16_t>(slotNumber, sizeof(uint16_t)) & kWideMask) == kWideStep * slot;
        }
    }

    std::optional<uint32_t> LocateFriendList(const ImageSection& data)
    {
        const size_t listBytes = kFriendSlotBytes * kFriendSlots;
        if (data.bytes == nullptr || data.size < listBytes) return std::nullopt;
        std::optional<uint32_t> found;
        for (size_t first = 0; first + listBytes <= data.size; first += kScanStep)
        {
            const uint8_t* number = data.bytes + first + kSlotNumber;
            if (!NumberedAs(number, 1)) continue;
            bool run = true;
            for (int slot = 1; slot < kSlotsChecked && run; ++slot)
                run = NumberedAs(number + kFriendSlotBytes * static_cast<size_t>(slot), slot + 1);
            if (!run) continue;
            if (found) return std::nullopt;
            found = static_cast<uint32_t>(first);
        }
        return found;
    }

    std::vector<uint32_t> FriendCharacters(const uint8_t* firstSlot)
    {
        std::vector<uint32_t> characters;
        for (int slot = 0; slot < kFriendSlots; ++slot)
        {
            const uint8_t* at        = firstSlot + kFriendSlotBytes * static_cast<size_t>(slot);
            const uint32_t character = ReadAt<uint32_t>(at, kSlotCharacter);
            if (ReadAt<uint64_t>(at, kSlotAccount) != 0 && character != 0) characters.push_back(character);
        }
        std::ranges::sort(characters);
        return characters;
    }

    bool IsFriend(std::span<const uint32_t> characters, uint32_t serverId)
    {
        return std::ranges::binary_search(characters, serverId);
    }
}
