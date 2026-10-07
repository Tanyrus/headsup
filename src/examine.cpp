#include "examine.h"

#include <cstring>

namespace headsup
{
    namespace
    {
        constexpr uint16_t kImpossibleToGauge = 0xF9;
        constexpr uint16_t kFirstCheckMessage = 0xAA; // 0xAA-0xB2: the con, then the defense/evasion remark
        constexpr uint16_t kLastCheckMessage  = 0xB2;
        constexpr uint32_t kCheckTypeBase     = 0x40; // the reply's check type field: 0x40 + Con

        // Byte offsets, counting Ashita's 4-byte packet header.
        constexpr size_t kReplyServerId      = 0x08;
        constexpr size_t kReplyLevel         = 0x0C;
        constexpr size_t kReplyCheckType     = 0x10;
        constexpr size_t kReplyTargetIndex   = 0x16;
        constexpr size_t kReplyMessage       = 0x18;
        constexpr size_t kReplyMinSize       = kReplyMessage + sizeof(uint16_t);

        template <typename T>
        T Read(const uint8_t* data, size_t offset)
        {
            T value;
            std::memcpy(&value, data + offset, sizeof(value));
            return value;
        }
    }

    std::optional<CheckReply> ParseCheckReply(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kReplyMinSize) return std::nullopt;
        const auto message = Read<uint16_t>(data, kReplyMessage);
        CheckReply reply{Read<uint32_t>(data, kReplyServerId), Read<uint16_t>(data, kReplyTargetIndex), false, {}};
        if (message == kImpossibleToGauge) return reply;
        if (message < kFirstCheckMessage || message > kLastCheckMessage) return std::nullopt;
        const auto level = Read<int32_t>(data, kReplyLevel);
        const auto type  = Read<uint32_t>(data, kReplyCheckType);
        if (level <= 0 || type < kCheckTypeBase || type >= kCheckTypeBase + kConCount) return std::nullopt;
        reply.gauged = true;
        reply.result = CheckResult{level, static_cast<Con>(type - kCheckTypeBase)};
        return reply;
    }

    double CheckLifetime(const MobRecord* mob)
    {
        return mob != nullptr && mob->respawn > 0 ? static_cast<double>(mob->respawn) : kDefaultCheckLifetime;
    }

    void CheckResults::Received(const CheckReply& reply, double lifetime, double now)
    {
        if (reply.gauged) m_Results[reply.serverId] = Stored{reply.result, now + lifetime};
    }

    const CheckResult* CheckResults::Result(uint32_t serverId, double now) const
    {
        const auto it = m_Results.find(serverId);
        return it != m_Results.end() && now < it->second.expires ? &it->second.result : nullptr;
    }

    void CheckResults::Forget(uint32_t serverId)
    {
        m_Results.erase(serverId);
    }
}
