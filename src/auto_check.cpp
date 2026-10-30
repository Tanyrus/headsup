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
        // What an addon's report of a check's answer holds, both parts: the checker addon's name in the line's header
        // and the level it reports, or simplelog's words for a con and for an answer the server would not gauge.
        struct ReportMarks
        {
            std::string_view first, second;
        };
        constexpr ReportMarks kReports[] = {{"checker", "Lv. "}, {" seems ", "to be level "}, {"'s strength is impossible to gauge!", ""}};

        bool IsReport(std::string_view line)
        {
            for (const ReportMarks& marks : kReports)
                if (line.find(marks.first) != std::string_view::npos && line.find(marks.second) != std::string_view::npos) return true;
            return false;
        }

        // A mob by more than the kind the client's flags gave it: the server tells a player who examined them.
        bool IsZoneMob(const CheckCandidate& entity)
        {
            return entity.kind == EntityKind::Mob && !entity.pet && entity.serverId >= kFirstZoneEntityId &&
                   entity.index < kFirstPlayerIndex;
        }
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

    std::optional<uint32_t> PlayersCheck(uint16_t id, const uint8_t* data, uint32_t size, bool injected, bool blocked)
    {
        if (id != kCheckRequestPacket || injected || blocked) return std::nullopt;
        return CheckRequestTarget(data, size);
    }

    std::optional<CheckCandidate> AutoCheck::Due(std::span<const CheckCandidate> mobs, double now)
    {
        if (m_LastAsk && now - *m_LastAsk < kAutoCheckGap) return std::nullopt;
        const CheckCandidate* nearest = nullptr;
        for (const CheckCandidate& mob : mobs)
        {
            if (!IsZoneMob(mob) || !mob.alive || mob.known || !mob.gaugeable || mob.distance > kAutoCheckRange) continue;
            if (m_NeverGauged.contains(mob.serverId)) continue;
            // The answer to the player's own check is on its way: a second would be read twice.
            if (const auto theirs = m_PlayerAsked.find(mob.serverId); theirs != m_PlayerAsked.end() && now - theirs->second < kOwnAnswerWait)
                continue;
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
        if (!IsReport(line)) return false;
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
        m_NeverGauged.erase(serverId);
        // An answer still on its way is HeadsUp's to keep out; with none owed, the next spawn is asked about at once.
        if (!m_Waiting.contains(serverId)) m_Asked.erase(serverId);
    }

    void AutoCheck::Clear() { *this = AutoCheck{}; }
}
