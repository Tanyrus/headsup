#include "tracker.h"

#include <algorithm>
#include <cstdio>

namespace aggroglow
{
    void Tracker::Update(const std::vector<ActorInput>& actors, const PlayerState& player, const Settings& settings)
    {
        m_Actors.clear();
        m_Min      = UINT32_MAX;
        m_Max      = 0;
        m_Outlined = 0;
        m_Mobs.clear();
        for (const ActorInput& a : actors)
        {
            if (a.actor == 0) continue;
            ActorInfo info;
            info.index = a.index;
            info.isMob = a.isMob;
            info.alive = a.alive;
            if (a.isMob)
            {
                const char* name     = a.name != nullptr ? a.name : "";
                const MobRecord* mob = FindMob(a.serverId, name);
                std::snprintf(info.name, sizeof(info.name), "%s", name);
                if (a.alive)
                {
                    info.label = MakeLabel(mob, a.examined, player.level);
                    info.icons = IconsFor(mob);
                }
                if (settings.enabled && a.alive && a.distance <= settings.maxDistance)
                {
                    const int category = static_cast<int>(Classify(mob, a.examined ? a.examined->level : 0, player));
                    if (settings.show[category])
                    {
                        info.outline    = true;
                        info.stencilRef = static_cast<uint8_t>(m_Outlined % 255 + 1);
                        info.argb       = ToArgb(settings.color[category]);
                        ++m_Outlined;
                    }
                }
                m_Mobs.push_back(a.actor);
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
