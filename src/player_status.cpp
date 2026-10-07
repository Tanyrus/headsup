#include "player_status.h"

#include "bytes.h"
#include "settings.h"

#include <algorithm>

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

        // 0x067.
        constexpr size_t kSyncKind         = 0x04;
        constexpr uint8_t kSyncCharacter   = 0x02;
        constexpr size_t kSyncIndex        = 0x06;
        constexpr size_t kSyncFlags        = 0x10;
        constexpr size_t kSyncSize         = kSyncFlags + 1;
        constexpr int kSyncLevelSyncBit    = 2;

        constexpr uint32_t kGmLevelMask = 0x7;

        constexpr int kLevelSyncEffect   = 269; // the Level Sync status effect, and its icon
        constexpr int kIconHighBits      = 2;   // per slot in a party member's icon mask, each worth 256
        constexpr uint64_t kIconHighMask = 0x3;
        constexpr int kIconLowBits       = 8;

        // Each PlayerIcon's image.
        constexpr Icon kPlayerIconImages[kPlayerIconCount] = {Icon::Gm, Icon::Mentor, Icon::NewAdventurer, Icon::LevelSync,
            Icon::Away, Icon::Linkshell, Icon::Bazaar, Icon::Invite};

        // Render flags in the entity's memory.
        constexpr int kRenderSeekBit      = 20; // Flags1
        constexpr int kRenderLinkshellBit = 27;
        constexpr int kRenderBazaarBit    = 9;  // Flags2

        bool Bit(uint32_t word, int bit)
        {
            return (word >> bit & 1u) != 0;
        }
    }

    std::optional<PlayerUpdate> ParseOtherPlayer(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kOtherSize) return std::nullopt;
        const uint8_t send = data[kOtherSend];
        PlayerUpdate update{ReadAt<uint16_t>(data, kOtherIndex), (send & kSendDespawn) != 0, std::nullopt};
        if ((send & kSendGeneral) == 0) return update;
        const auto flags1 = ReadAt<uint32_t>(data, kOtherFlags1), flags2 = ReadAt<uint32_t>(data, kOtherFlags2),
                   flags3 = ReadAt<uint32_t>(data, kOtherFlags3);
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
        const auto flags0 = ReadAt<uint32_t>(data, kOwnFlags0), flags1 = ReadAt<uint32_t>(data, kOwnFlags1),
                   flags3 = ReadAt<uint32_t>(data, kOwnFlags3);
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

    std::optional<LevelSyncUpdate> ParseCharSync(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kSyncSize || data[kSyncKind] != kSyncCharacter) return std::nullopt;
        return LevelSyncUpdate{ReadAt<uint16_t>(data, kSyncIndex), Bit(data[kSyncFlags], kSyncLevelSyncBit)};
    }

    bool LevelSyncInBuffs(const int16_t* buffs)
    {
        return buffs != nullptr && std::find(buffs, buffs + kStatusIconSlots, kLevelSyncEffect) != buffs + kStatusIconSlots;
    }

    bool LevelSyncInPartyIcons(const uint8_t* icons, uint64_t bitMask)
    {
        if (icons == nullptr) return false;
        for (int slot = 0; slot < kStatusIconSlots; ++slot)
        {
            const auto high = static_cast<int>(bitMask >> (slot * kIconHighBits) & kIconHighMask);
            if ((high << kIconLowBits | icons[slot]) == kLevelSyncEffect) return true;
        }
        return false;
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

    PlayerIconRows PlayerIcons(const PlayerStatus& status, const Settings& settings)
    {
        const bool has[kPlayerIconCount] = {status.gm, status.mentor, status.newAdventurer, status.levelSync, status.away,
            status.linkshell, status.bazaar, status.seekingParty};
        PlayerIconRows rows;
        for (int i = 0; i < kPlayerIconCount; ++i)
        {
            const IconSide side = settings.playerIconSide[i];
            if (!has[i] || side == IconSide::Hidden) continue;
            IconSet& row           = side == IconSide::Right ? rows.right : rows.left;
            row.icons[row.count++] = kPlayerIconImages[i];
        }
        return rows;
    }
}
