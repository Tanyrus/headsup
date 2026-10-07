#include "tracker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kSpawnFlagPlayer = 0x01;
        constexpr uint32_t kSpawnFlagMob    = 0x10;
    }

    EntityKind KindFromSpawnFlags(uint32_t flags)
    {
        if ((flags & kSpawnFlagMob) != 0) return EntityKind::Mob;
        return (flags & kSpawnFlagPlayer) != 0 ? EntityKind::Player : EntityKind::Npc;
    }

    bool ReplacesName(const Settings& settings, const ActorInfo& info)
    {
        const bool kind = info.kind == EntityKind::Mob      ? settings.replaceMobNames
                          : info.kind == EntityKind::Player ? settings.replacePlayerNames
                                                            : settings.replaceNpcNames;
        return kind && info.name[0] != '\0';
    }

    void Tracker::Update(const std::vector<ActorInput>& actors, const PlayerState& player, const Settings& settings)
    {
        m_Actors.clear();
        m_Min      = UINT32_MAX;
        m_Max      = 0;
        m_Outlined = 0;
        m_Order.clear();
        for (const ActorInput& a : actors)
        {
            if (a.actor == 0) continue;
            ActorInfo info;
            info.index       = a.index;
            info.kind        = a.kind;
            info.alive       = a.alive;
            info.pose        = a.pose;
            info.feet        = a.feet;
            std::snprintf(info.name, sizeof(info.name), "%s", a.name);
            if (a.kind == EntityKind::Mob)
            {
                const MobRecord* mob = FindMob(a.serverId, a.name);
                if (a.alive)
                {
                    info.label = MakeLabel(mob, a.checked, player.level);
                    info.icons = IconsFor(mob);
                }
                if (settings.enabled && a.alive && a.distance <= settings.maxDistance)
                {
                    const int category = CategoryIndex(Classify(mob, a.checked ? a.checked->level : 0, player));
                    if (settings.show[category])
                    {
                        info.outline    = true;
                        info.stencilRef = static_cast<uint8_t>(m_Outlined % kMaxStencilRef + 1);
                        info.argb       = ToArgb(settings.color[category]);
                        ++m_Outlined;
                    }
                }
            }
            if (a.kind == EntityKind::Player && a.status)
            {
                info.nameIcons     = PlayerIcons(*a.status, settings);
                info.linkshellArgb = a.status->linkshellArgb;
            }
            m_Order.push_back(a.actor);
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

    float DistanceFromSquared(float squared)
    {
        if (std::isnan(squared)) return squared;
        return squared > 0.0f ? std::sqrt(squared) : 0.0f;
    }
}
