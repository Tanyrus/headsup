#include "examine.h"

#include <cmath>
#include <cstring>

namespace aggroglow
{
    namespace
    {
        constexpr uint16_t kImpossibleToGauge = 0xF9;
        constexpr uint16_t kFirstCheckMessage = 0xAA; // 0xAA-0xB2: the con, then the defense/evasion remark
        constexpr uint16_t kLastCheckMessage  = 0xB2;
        constexpr uint32_t kCheckTypeBase     = 0x40; // the reply's check type field: 0x40 + Con

        // Byte offsets, counting Ashita's 4-byte packet header.
        constexpr size_t kRequestServerId    = 0x04;
        constexpr size_t kRequestTargetIndex = 0x08;
        constexpr size_t kRequestKind        = 0x0C;
        constexpr uint8_t kKindCheck         = 0;
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

        template <typename T>
        void Write(uint8_t* data, size_t offset, T value)
        {
            std::memcpy(data + offset, &value, sizeof(value));
        }
    }

    std::array<uint8_t, 16> BuildCheckRequest(uint32_t serverId, uint16_t targetIndex)
    {
        std::array<uint8_t, 16> packet{};
        Write<uint32_t>(packet.data(), kRequestServerId, serverId);
        Write<uint32_t>(packet.data(), kRequestTargetIndex, targetIndex);
        packet[kRequestKind] = kKindCheck;
        return packet;
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

    double ExamineCooldown(const MobRecord* mob)
    {
        return mob != nullptr && mob->respawn > 0 ? static_cast<double>(mob->respawn) : kDefaultCooldown;
    }

    float DistanceFromSquared(float squared)
    {
        if (std::isnan(squared)) return squared;
        return squared > 0.0f ? std::sqrt(squared) : 0.0f;
    }

    bool IsExamineEligible(const ExamineTarget& t)
    {
        if (!t.alive || t.playerInEvent || t.playerLevel <= 0) return false;
        if (!(t.distance <= kExamineMaxDistance)) return false; // also rejects NaN
        if (t.mob == nullptr) return true;                      // no data: only a check can tell
        if (t.mob->flags & (kMobNotorious | kMobBattlefield)) return false; // always "impossible to gauge"
        return t.mob->maxLevel == 0 || !IsTooWeak(t.playerLevel, t.mob->maxLevel);
    }

    bool Examiner::CanSend(uint32_t serverId, double now) const
    {
        if (Result(serverId, now) != nullptr) return false;
        if (m_SentAny && now < m_LastSent + kExamineInterval) return false;
        const auto it = m_NextAllowed.find(serverId);
        return it == m_NextAllowed.end() || now >= it->second;
    }

    void Examiner::Sent(uint32_t serverId, double cooldown, double now)
    {
        m_LastSent              = now;
        m_SentAny               = true;
        m_NextAllowed[serverId] = now + cooldown;
        m_Pending               = Pending{serverId, now, true};
    }

    bool Examiner::Received(const CheckReply& reply, double lifetime, double now)
    {
        if (reply.gauged) m_Results[reply.serverId] = Stored{reply.result, now + lifetime};
        const bool answers = m_Pending.active && m_Pending.serverId == reply.serverId && now - m_Pending.time <= kPendingTimeout;
        if (answers) m_Pending.active = false;
        return answers;
    }

    const CheckResult* Examiner::Result(uint32_t serverId, double now) const
    {
        const auto it = m_Results.find(serverId);
        return it != m_Results.end() && now < it->second.expires ? &it->second.result : nullptr;
    }

    void Examiner::Forget(uint32_t serverId)
    {
        m_Results.erase(serverId);
    }
}
