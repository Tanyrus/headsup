#include "tracker.h"

#include <algorithm>

namespace aggroglow
{
    void Tracker::Update(const std::vector<ActorInput>& actors, uint16_t zone, PlayerState player, const Settings& settings)
    {
        player.table = settings.modernConTable ? ConTable::Modern : ConTable::Era;
        m_Actors.clear();
        m_Min      = UINT32_MAX;
        m_Max      = 0;
        m_Outlined = 0;
        for (const ActorInput& a : actors)
        {
            if (a.actor == 0) continue;
            ActorInfo info;
            if (settings.enabled && a.isMob && a.alive && a.distance <= settings.maxDistance)
            {
                const int category = static_cast<int>(Classify(FindMob(zone, a.index, a.name ? a.name : ""), player));
                if (settings.show[category])
                {
                    info.outline    = true;
                    info.stencilRef = static_cast<uint8_t>(m_Outlined % 255 + 1);
                    info.argb       = ToArgb(settings.color[category]);
                    ++m_Outlined;
                }
            }
            m_Actors[a.actor] = info;
            m_Min = std::min(m_Min, a.actor);
            m_Max = std::max(m_Max, a.actor);
        }
    }

    const ActorInfo* Tracker::Find(ActorPtr actor) const
    {
        if (actor < m_Min || actor > m_Max) return nullptr; // cheap reject for the stack scan
        const auto it = m_Actors.find(actor);
        return it == m_Actors.end() ? nullptr : &it->second;
    }
}
