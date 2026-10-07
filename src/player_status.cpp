#include "player_status.h"

#include <cstring>

namespace headsup
{
    namespace
    {
        // 0x00D, counting Ashita's 4-byte header.
        constexpr size_t kOtherIndex    = 0x08;
        constexpr size_t kOtherSend     = 0x0A;
        constexpr size_t kOtherFlags1   = 0x20;
        constexpr size_t kOtherFlags2   = 0x24; // the linkshell's red, green and blue in its low three bytes
        constexpr size_t kOtherFlags3   = 0x28;
        constexpr size_t kOtherSize     = kOtherFlags3 + sizeof(uint32_t);
        constexpr uint8_t kSendGeneral  = 0x04; // the update carries the flags
        constexpr uint8_t kSendDespawn  = 0x20;
        constexpr int kOtherSeekBit     = 11;
        constexpr int kOtherAwayBit     = 14;
        constexpr int kOtherLinkshellBit = 17;
        constexpr int kOtherGmShift     = 24;   // 3-bit GM level
        constexpr int kOtherBazaarBit   = 31;
        constexpr int kOtherNewBit      = 23;   // in the third word
        constexpr int kOtherMentorBit   = 24;

        // 0x037.
        constexpr size_t kOwnFlags0     = 0x28;
        constexpr size_t kOwnFlags1     = 0x2C;
        constexpr size_t kOwnRed        = 0x31; // then green and blue
        constexpr size_t kOwnFlags3     = 0x38;
        constexpr size_t kOwnSize       = kOwnFlags3 + sizeof(uint32_t);
        constexpr int kOwnSeekBit       = 4;
        constexpr int kOwnAwayBit       = 7;
        constexpr int kOwnLinkshellBit  = 25;
        constexpr int kOwnGmShift       = 29;
        constexpr int kOwnBazaarBit     = 29;   // in the second word
        constexpr int kOwnNewBit        = 3;    // in the fourth word
        constexpr int kOwnMentorBit     = 4;

        constexpr uint32_t kGmLevelMask = 0x7;

        // Render flags in the entity's memory.
        constexpr int kRenderSeekBit      = 20; // Flags1
        constexpr int kRenderLinkshellBit = 27;
        constexpr int kRenderBazaarBit    = 9;  // Flags2

        uint32_t Word(const uint8_t* data, size_t offset)
        {
            uint32_t value;
            std::memcpy(&value, data + offset, sizeof(value));
            return value;
        }

        bool Bit(uint32_t word, int bit)
        {
            return (word >> bit & 1u) != 0;
        }

        uint32_t Opaque(uint8_t r, uint8_t g, uint8_t b)
        {
            return 0xFF000000u | uint32_t{r} << 16 | uint32_t{g} << 8 | b;
        }
    }

    std::optional<PlayerUpdate> ParseOtherPlayer(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kOtherSize) return std::nullopt;
        uint16_t index;
        std::memcpy(&index, data + kOtherIndex, sizeof(index));
        const uint8_t send = data[kOtherSend];
        PlayerUpdate update{index, (send & kSendDespawn) != 0, std::nullopt};
        if ((send & kSendGeneral) == 0) return update;
        const uint32_t flags1 = Word(data, kOtherFlags1), flags2 = Word(data, kOtherFlags2), flags3 = Word(data, kOtherFlags3);
        PlayerStatus s;
        s.seekingParty  = Bit(flags1, kOtherSeekBit);
        s.away          = Bit(flags1, kOtherAwayBit);
        s.linkshell     = Bit(flags1, kOtherLinkshellBit);
        s.gm            = (flags1 >> kOtherGmShift & kGmLevelMask) != 0;
        s.bazaar        = Bit(flags1, kOtherBazaarBit);
        s.newAdventurer = Bit(flags3, kOtherNewBit);
        s.mentor        = Bit(flags3, kOtherMentorBit);
        s.linkshellArgb = Opaque(static_cast<uint8_t>(flags2), static_cast<uint8_t>(flags2 >> 8), static_cast<uint8_t>(flags2 >> 16));
        update.status   = s;
        return update;
    }

    std::optional<PlayerStatus> ParseOwnStatus(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kOwnSize) return std::nullopt;
        const uint32_t flags0 = Word(data, kOwnFlags0), flags1 = Word(data, kOwnFlags1), flags3 = Word(data, kOwnFlags3);
        PlayerStatus s;
        s.seekingParty  = Bit(flags0, kOwnSeekBit);
        s.away          = Bit(flags0, kOwnAwayBit);
        s.linkshell     = Bit(flags0, kOwnLinkshellBit);
        s.gm            = (flags0 >> kOwnGmShift & kGmLevelMask) != 0;
        s.bazaar        = Bit(flags1, kOwnBazaarBit);
        s.newAdventurer = Bit(flags3, kOwnNewBit);
        s.mentor        = Bit(flags3, kOwnMentorBit);
        s.linkshellArgb = Opaque(data[kOwnRed], data[kOwnRed + 1], data[kOwnRed + 2]);
        return s;
    }

    PlayerStatus StatusFromRender(uint32_t flags1, uint32_t flags2, uint32_t linkshellBgr)
    {
        PlayerStatus s;
        s.seekingParty  = Bit(flags1, kRenderSeekBit);
        s.linkshell     = Bit(flags1, kRenderLinkshellBit);
        s.bazaar        = Bit(flags2, kRenderBazaarBit);
        s.linkshellArgb = Opaque(static_cast<uint8_t>(linkshellBgr), static_cast<uint8_t>(linkshellBgr >> 8),
            static_cast<uint8_t>(linkshellBgr >> 16));
        return s;
    }

    PlayerStatus WithPacketStatus(const PlayerStatus& memory, const PlayerStatus* packet)
    {
        PlayerStatus s = memory;
        if (packet == nullptr) return s;
        s.away          = packet->away;
        s.mentor        = packet->mentor;
        s.newAdventurer = packet->newAdventurer;
        s.gm            = packet->gm;
        return s;
    }

    IconSet PlayerIcons(const PlayerStatus& status)
    {
        IconSet set;
        auto add = [&](bool shown, Icon icon) {
            if (shown) set.icons[set.count++] = icon;
        };
        add(status.gm, Icon::Gm);
        add(status.mentor, Icon::Mentor);
        add(status.newAdventurer, Icon::NewAdventurer);
        add(status.away, Icon::Away);
        add(status.linkshell, Icon::Linkshell);
        add(status.bazaar, Icon::Bazaar);
        add(status.seekingParty, Icon::Invite);
        return set;
    }
}
