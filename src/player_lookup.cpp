#include "player_lookup.h"

#include <algorithm>

namespace headsup
{
    void PlayerLookups::Frame(std::span<const InView> inView, double now)
    {
        for (auto& [id, p] : m_Players)
            if (p.inView)
            {
                p.inView = false;
                p.left   = now;
            }
        for (const InView& v : inView)
        {
            Player& p = m_Players[v.id];
            if (p.left && now - *p.left > kAwaySeconds) p.stale = true;
            p.left.reset();
            p.inView   = true;
            p.distance = v.distance;
        }
    }

    void PlayerLookups::LookChanged(uint32_t id)
    {
        if (const auto it = m_Players.find(id); it != m_Players.end()) it->second.stale = true;
    }

    void PlayerLookups::Zoned()
    {
        for (auto& [id, p] : m_Players)
            p.asked.reset();
    }

    bool PlayerLookups::Due(const Player& p, double now) const
    {
        if (!p.inView) return false;
        if (!p.asked) return true;
        const double since = now - *p.asked;
        return since >= kKeepSeconds || (p.stale && since >= kRecheckSeconds);
    }

    std::optional<std::vector<uint32_t>> PlayerLookups::NextRequest(double now)
    {
        if (!m_Out.empty() || (m_RetryAt && now < *m_RetryAt)) return std::nullopt;
        std::vector<std::pair<float, uint32_t>> due;
        for (const auto& [id, p] : m_Players)
            if (Due(p, now)) due.emplace_back(p.distance, id);
        if (due.empty()) return std::nullopt;
        std::sort(due.begin(), due.end());
        if (due.size() > kMaxIdsPerRequest) due.resize(kMaxIdsPerRequest);
        for (const auto& [distance, id] : due)
        {
            Player& p = m_Players[id];
            p.asked   = now;
            p.stale   = false;
            m_Out.push_back(id);
        }
        m_RetryAt.reset();
        return m_Out;
    }

    std::vector<LookupAnswer> PlayerLookups::Answered(std::span<const SearchedPlayer> players)
    {
        std::vector<LookupAnswer> matched;
        for (const SearchedPlayer& player : players)
            for (const uint32_t id : m_Out)
                if ((id & kSearchIdMask) == player.id)
                {
                    matched.push_back(LookupAnswer{id, player});
                    break;
                }
        m_Out.clear();
        return matched;
    }

    void PlayerLookups::Failed(double now)
    {
        for (const uint32_t id : m_Out)
            m_Players[id].asked.reset();
        m_Out.clear();
        m_RetryAt = now + kRetrySeconds;
    }
}
