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
        constexpr int kClaimedShift         = 16;
        constexpr uint32_t kClaimerIdBits   = 0xFFFF;
    }

    EntityKind KindFromSpawnFlags(uint32_t flags)
    {
        if ((flags & kSpawnFlagMob) != 0) return EntityKind::Mob;
        return (flags & kSpawnFlagPlayer) != 0 ? EntityKind::Player : EntityKind::Npc;
    }

    Category Classify(const MobRecord* mob, int checkedLevel, const PlayerState& player)
    {
        if (mob == nullptr) return Category::Unknown;
        const bool notorious  = (mob->flags & kMobNotorious) != 0;
        const Category attack = notorious ? Category::NmWillAttack : Category::WillAttack;
        const Category ignore = notorious ? Category::NmWontAttack : Category::WontAttack;

        if (!IsAggressive(*mob)) return ignore;
        if (mob->flags & kMobAlwaysAggro) return attack;
        // Unknown player level, or a level only a spawn script knows: warn rather than paint an aggressive mob safe.
        if (player.level <= 0 || (checkedLevel <= 0 && mob->maxLevel == 0)) return attack;
        // This spawn's checked level, else the top of the range: if any spawn of this mob can aggro, warn.
        const int level = checkedLevel > 0 ? checkedLevel : mob->maxLevel + mob->expLevelMod;
        if (Difficulty(player.level, level) != Con::TooWeak) return attack;
        return player.sitting ? attack : ignore;
    }

    bool IsClaimed(uint32_t claimStatus) { return (claimStatus >> kClaimedShift) != 0; }

    bool ClaimedByParty(uint32_t claimStatus, const std::vector<uint32_t>& partyServerIds)
    {
        if (!IsClaimed(claimStatus)) return false;
        return std::any_of(partyServerIds.begin(), partyServerIds.end(),
            [&](uint32_t id) { return (id & kClaimerIdBits) == (claimStatus & kClaimerIdBits); });
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
        m_Player   = player;
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
            info.claimedByParty = a.claimedByParty;
            info.claimed        = a.claimed;
            std::snprintf(info.name, sizeof(info.name), "%s", a.name);
            if (a.kind == EntityKind::Mob)
            {
                const MobRecord* mob   = FindMob(a.serverId, a.name);
                const bool placeholder = mob != nullptr && mob->placeholderOf != 0;
                if (a.alive)
                {
                    const Label level = MakeLabel(mob, a.checked, player.level);
                    info.tooWeak      = level.shade == LabelShade::TooWeak;
                    info.label        = LevelLine(level, settings.showLabels,
                        MobIdText(a.serverId, placeholder, settings.mobId, settings.markPlaceholders));
                    info.mobIcons = IconsFor(mob);
                }
                if (settings.enabled && a.alive && a.distance <= settings.maxDistance)
                {
                    const bool placeholderColor = placeholder && settings.show[CategoryIndex(Category::Placeholder)];
                    const int category = CategoryIndex(placeholderColor ? Category::Placeholder
                                                                        : Classify(mob, a.checked ? a.checked->level : 0, player));
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
                info.playerIcons   = PlayerIcons(*a.status, settings);
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

    const ActorInfo* FindOwner(const uint32_t* begin, const uint32_t* end, const Tracker& tracker)
    {
        for (const uint32_t* p = begin; p < end; ++p)
            if (const ActorInfo* info = tracker.Find(*p)) return info;
        return nullptr;
    }

    float DistanceFromSquared(float squared)
    {
        if (std::isnan(squared)) return squared;
        return squared > 0.0f ? std::sqrt(squared) : 0.0f;
    }
}
