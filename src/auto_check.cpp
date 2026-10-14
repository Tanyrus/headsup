#include "auto_check.h"

#include "bytes.h"

#include <cstring>

namespace headsup
{
    namespace
    {
        // Phoenix's c2s 0x0DD, counting Ashita's 4-byte header: the id in the first word's low 9 bits and the size
        // in 4-byte words in its high 7.
        constexpr uint8_t kRequestHeader[] = {0xDD, 0x08};
        constexpr size_t kRequestServerId = 0x04, kRequestIndex = 0x08;
        // What every line of the checker addon's holds: its name in the line's header, and the level it reports.
        constexpr std::string_view kCheckerAddon = "checker", kCheckerLevel = "Lv. ";
    }

    std::array<uint8_t, kCheckRequestSize> CheckRequest(uint32_t serverId, uint16_t index)
    {
        std::array<uint8_t, kCheckRequestSize> request{};
        std::memcpy(request.data(), kRequestHeader, sizeof(kRequestHeader));
        const uint32_t wideIndex = index;
        std::memcpy(request.data() + kRequestServerId, &serverId, sizeof(serverId));
        std::memcpy(request.data() + kRequestIndex, &wideIndex, sizeof(wideIndex));
        return request;
    }

    std::optional<uint32_t> CheckRequestTarget(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr || size < kRequestServerId + sizeof(uint32_t)) return std::nullopt;
        return ReadAt<uint32_t>(data, kRequestServerId);
    }

    std::optional<CheckCandidate> AutoCheck::Due(std::span<const CheckCandidate> mobs, double now)
    {
        if (m_LastAsk && now - *m_LastAsk < kAutoCheckGap) return std::nullopt;
        const CheckCandidate* nearest = nullptr;
        for (const CheckCandidate& mob : mobs)
        {
            if (!mob.alive || mob.known || !mob.gaugeable || mob.distance > kAutoCheckRange) continue;
            if (m_NeverGauged.contains(mob.serverId)) continue;
            if (const auto asked = m_Asked.find(mob.serverId); asked != m_Asked.end() && now - asked->second < kAskAgain) continue;
            if (nearest == nullptr || mob.distance < nearest->distance) nearest = &mob;
        }
        if (nearest == nullptr) return std::nullopt;
        m_Asked[nearest->serverId] = now;
        m_Names[nearest->serverId] = nearest->name;
        m_Waiting.insert(nearest->serverId);
        m_LastAsk = now;
        return *nearest;
    }

    void AutoCheck::PlayerAsked(uint32_t serverId, double now) { m_PlayerAsked[serverId] = now; }

    bool AutoCheck::TakeAnswer(const CheckAnswer& answer, double now)
    {
        if (!answer.gauged) m_NeverGauged.insert(answer.serverId);
        const auto within = [&](const std::unordered_map<uint32_t, double>& asked) {
            const auto at = asked.find(answer.serverId);
            return at != asked.end() && now - at->second <= kOwnAnswerWait;
        };
        const bool ours = m_Waiting.erase(answer.serverId) != 0 && within(m_Asked);
        const bool theirs = within(m_PlayerAsked);
        m_PlayerAsked.erase(answer.serverId);
        if (ours && !theirs) m_OwnAnswered[answer.serverId] = now;
        return ours && !theirs;
    }

    bool AutoCheck::OwnReport(std::string_view line, double now) const
    {
        if (line.find(kCheckerAddon) == std::string_view::npos || line.find(kCheckerLevel) == std::string_view::npos) return false;
        // The player's own check of anything is theirs to read, whichever mob the line names.
        for (const auto& [id, at] : m_PlayerAsked)
            if (now - at <= kOwnAnswerWait) return false;
        const auto names = [&](uint32_t id) {
            const auto name = m_Names.find(id);
            return name != m_Names.end() && !name->second.empty() && line.find(name->second) != std::string_view::npos;
        };
        for (const uint32_t id : m_Waiting)
            if (const auto asked = m_Asked.find(id); asked != m_Asked.end() && now - asked->second <= kOwnAnswerWait && names(id)) return true;
        for (const auto& [id, at] : m_OwnAnswered)
            if (now - at <= kReportWait && names(id)) return true;
        return false;
    }

    void AutoCheck::Forget(uint32_t serverId)
    {
        m_Asked.erase(serverId);
        m_PlayerAsked.erase(serverId);
        m_Waiting.erase(serverId);
        m_NeverGauged.erase(serverId);
        m_OwnAnswered.erase(serverId);
        m_Names.erase(serverId);
    }

    void AutoCheck::Clear() { *this = AutoCheck{}; }
}
