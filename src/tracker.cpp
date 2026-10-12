#include "tracker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace headsup
{
    namespace
    {
        constexpr int kClaimedShift         = 16;
        constexpr uint32_t kClaimerIdBits   = 0xFFFF;
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

    void Tracker::Update(const std::vector<ActorInput>& actors, const PlayerState& player, const Settings& settings,
        const CursorTargets& targets)
    {
        BadgeRules badges{.damage = settings.showDamageIcons, .statusRanks = settings.showStatusRankIcons,
            .immunities = settings.showImmunityIcons, .elementWeakRank = settings.elementWeakRank,
            .elementResistRank = settings.elementResistRank, .maxElements = settings.maxElements};
        std::ranges::copy(settings.statusBadges, badges.status);
        m_Actors.clear();
        m_Actors.reserve(actors.size());
        m_Player   = player;
        m_Min      = UINT32_MAX;
        m_Max      = 0;
        m_Outlined = 0;
        m_Order.clear();
        for (const ActorInput& a : actors)
        {
            if (a.actor == 0) continue;
            // Filled where it is kept: an ActorInfo is half a kilobyte, and Update runs for every actor every frame.
            const auto [entry, added] = m_Actors.try_emplace(a.actor);
            ActorInfo& info           = entry->second;
            if (!added) info = ActorInfo{}; // listed twice: the later entry alone counts
            info.index       = a.index;
            info.serverId    = a.serverId;
            info.kind        = a.kind;
            info.alive       = a.alive;
            info.pose        = a.pose;
            info.feet        = a.feet;
            info.claimedByParty = a.claimedByParty;
            info.claimed        = a.claimed;
            std::snprintf(info.name, sizeof(info.name), "%s", a.name);
            std::snprintf(info.ownerName, sizeof(info.ownerName), "%s", a.ownerName);
            if (a.checked != nullptr) info.checked = *a.checked;
            info.status = a.status;
            info.jobs   = a.jobs;
            EntityState& st = info.state;
            st.kind      = a.kind;
            st.self      = a.self;
            st.pet       = a.pet;
            st.alive     = a.alive;
            st.engaged   = a.engaged;
            st.target    = TargetStateOf(a.index, targets);
            st.claim     = a.claimedByParty ? ClaimState::Yours : a.claimed ? ClaimState::Others : ClaimState::Unclaimed;
            st.playerRow = PlayerRowOf(a.party, a.isFriend, a.linkshell);
            if (a.kind == EntityKind::Mob)
            {
                const MobRecord* mob   = FindMob(a.serverId, a.name);
                const bool placeholder = mob != nullptr && mob->placeholderOf != 0;
                info.mob               = mob;
                info.placeholder       = placeholder;
                if (a.alive)
                {
                    const Label level = MakeLabel(mob, a.checked, player.level);
                    info.level        = level;
                    info.tooWeak      = level.shade == LabelShade::TooWeak;
                    info.label        = LevelLine(level, settings.showLabels,
                        MobIdText(a.serverId, placeholder, settings.mobId, settings.markPlaceholders));
                    info.mobIcons = IconsFor(mob);
                    info.infoIcons = InfoIconsFor(mob != nullptr ? &ResistancesOf(*mob) : nullptr, badges);
                }
                const Category classified = Classify(mob, a.checked ? a.checked->level : 0, player);
                st.nature    = NatureOf(classified, placeholder);
                st.classified = classified;
                st.row       = MobRowOf(st.nature, a.claimedByParty, a.claimed);
                st.toughness = info.level.shade;
                st.tooWeak   = info.tooWeak;
                const bool placeholderColor = placeholder && settings.show[CategoryIndex(Category::Placeholder)];
                info.category = placeholderColor ? Category::Placeholder : classified;
                // The bar and look.cpp color a mob by this category.
                if (a.claimedByParty && settings.show[CategoryIndex(Category::PartyClaim)])
                    info.category = Category::PartyClaim;
                else if (a.claimed && !a.claimedByParty && settings.show[CategoryIndex(Category::OtherClaim)])
                    info.category = Category::OtherClaim;
            }
            const OutlineMode mode = OutlineModeOf(st, settings);
            const bool wanted = mode == OutlineMode::Shown || (mode == OutlineMode::WhenTargeted && Targeted(st.target));
            if (settings.enabled && settings.outlines && wanted && a.alive && a.distance <= settings.maxDistance)
            {
                info.outline    = true;
                info.stencilRef = static_cast<uint8_t>(m_Outlined % kMaxStencilRef + 1);
                info.argb       = RowColor(st, settings);
                ++m_Outlined;
            }
            if (a.kind == EntityKind::Player && a.status)
            {
                info.playerIcons   = PlayerIcons(*a.status, settings);
                info.linkshellArgb = a.status->linkshellArgb;
            }
            m_Order.push_back(a.actor);
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

    const ActorInfo* Tracker::FindIndex(uint16_t index) const
    {
        if (index == 0) return nullptr;
        for (const auto& [actor, info] : m_Actors)
            if (info.index == index) return &info;
        return nullptr;
    }

    const ActorInfo* FindOwner(const uint32_t* begin, const uint32_t* end, const Tracker& tracker)
    {
        for (const uint32_t* p = begin; p < end; ++p)
            if (const ActorInfo* info = tracker.Find(*p)) return info;
        return nullptr;
    }

    const ActorInfo* FindOwnerAboveCaller(const uint32_t* begin, const uint32_t* end, const Tracker& tracker,
        uint32_t codeBegin, uint32_t codeEnd)
    {
        for (const uint32_t* p = begin; p < end; ++p)
            if (*p >= codeBegin && *p < codeEnd) return FindOwner(p + 1, end, tracker);
        return FindOwner(begin, end, tracker);
    }

    float DistanceFromSquared(float squared)
    {
        if (std::isnan(squared)) return squared;
        return squared > 0.0f ? std::sqrt(squared) : 0.0f;
    }
}
