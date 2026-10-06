#include "tracker.h"

#include <algorithm>

namespace aggroglow
{
    void Tracker::Update(const std::vector<ActorInput>& actors, const PlayerState& player, const Settings& settings)
    {
        m_Actors.clear();
        m_Min      = UINT32_MAX;
        m_Max      = 0;
        m_Outlined.clear();
        for (const ActorInput& a : actors)
        {
            if (a.actor == 0) continue;
            ActorInfo info;
            if (settings.enabled && a.isMob && a.alive && a.distance <= settings.maxDistance)
            {
                const MobRecord* mob = FindMob(a.serverId, a.name ? a.name : "");
                const int category   = static_cast<int>(Classify(mob, a.examined ? a.examined->level : 0, player));
                if (settings.show[category])
                {
                    info.outline    = true;
                    info.stencilRef = static_cast<uint8_t>(m_Outlined.size() % 255 + 1);
                    info.argb       = ToArgb(settings.color[category]);
                    info.label      = MakeLabel(mob, a.examined, player.level);
                    m_Outlined.push_back(a.actor);
                }
            }
            info.index = a.index;
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
