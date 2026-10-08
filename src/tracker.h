#pragma once

#include "icons.h"
#include "labels.h"
#include "player_status.h"
#include "pose.h"
#include "settings.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace headsup
{
    using ActorPtr = uint32_t; // FFXI is a 32-bit process

    enum class EntityKind : uint8_t
    {
        Mob,
        Player, // you included
        Npc,
    };
    // From IEntity::GetSpawnFlags: the mob bit wins over the player bit, and neither means an NPC.
    EntityKind KindFromSpawnFlags(uint32_t flags);

    struct PlayerState
    {
        int level    = 0;     // main job level; 0 while unknown (zoning, logging in)
        bool sitting = false; // resting, /sit or a chair: Phoenix lets Too Weak aggressive mobs aggro then
        bool engaged = false; // your status is engaged
    };

    // Phoenix's CZoneEntities::tapMobAggro with its mob data. checkedLevel is the level a /check reported for this
    // spawn, which includes its level mod, or 0 without one.
    Category Classify(const MobRecord* mob, int checkedLevel, const PlayerState& player);

    constexpr int kMaxStencilRef    = 255;  // outlined mobs take stencil references 1 to this, then wrap
    constexpr uint32_t kMaxEntities = 4096; // more than the client's entity map holds

    struct ActorInput
    {
        ActorPtr actor;
        uint16_t index;    // entity target index
        uint32_t serverId; // the mob data key
        EntityKind kind;
        bool alive;       // HP above 0 (defeated mobs keep their model until they despawn)
        float distance;   // yalms from the player
        const char* name; // only read during Update
        const CheckResult* checked         = nullptr;        // this spawn's latest /check; only read during Update
        std::optional<PlayerStatus> status = std::nullopt;   // a player's
        Pose pose                          = Pose::Standing; // a player's
        WorldPoint feet{};
        bool claimedByParty = false;
        bool claimed        = false;
    };

    struct ActorInfo
    {
        bool outline       = false;
        uint8_t stencilRef = 0;     // 1 to kMaxStencilRef when outlined
        uint32_t argb      = 0;     // the outline's D3DCOLOR
        uint16_t index     = 0;     // entity target index
        EntityKind kind    = EntityKind::Npc;
        bool alive         = false;
        char name[32]      = {};
        Label label{};              // level and con text: every living mob
        IconSet mobIcons{};
        PlayerIconRows playerIcons{};
        uint32_t linkshellArgb = 0; // a player's linkshell color, for its icon
        Pose pose              = Pose::Standing;
        WorldPoint feet{};
        bool claimedByParty    = false;
        bool claimed           = false;
        bool tooWeak           = false;
    };

    bool ReplacesName(const Settings& settings, const ActorInfo& info);

    // Whether a mob's claim (IEntity's ClaimStatus: the claimer's server ID in the low 16 bits, and 1 in the high 16
    // while it is claimed) is held by anyone.
    bool IsClaimed(uint32_t claimStatus);
    // Held by you or a member of your party or alliance.
    bool ClaimedByParty(uint32_t claimStatus, const std::vector<uint32_t>& partyServerIds);

    // Every entity's actor pointer and name, and each mob's level, icons and outline, as of the last Update.
    class Tracker
    {
    public:
        void Update(const std::vector<ActorInput>& actors, const PlayerState& player, const Settings& settings);
        const ActorInfo* Find(ActorPtr actor) const;
        uint32_t OutlinedCount() const { return m_Outlined; }
        const PlayerState& Player() const { return m_Player; }
        // In entity order.
        const std::vector<ActorPtr>& Actors() const { return m_Order; }

    private:
        std::unordered_map<ActorPtr, ActorInfo> m_Actors;
        std::vector<ActorPtr> m_Order;
        uint32_t m_Outlined = 0;
        PlayerState m_Player;
        ActorPtr m_Min = 0;
        ActorPtr m_Max = 0;
    };

    // The first tracked actor pointer in [begin, end), a stack: the live actor being drawn, as pointers further up can
    // be stale leftovers. nullptr when there is none.
    const ActorInfo* FindOwner(const uint32_t* begin, const uint32_t* end, const Tracker& tracker);

    // Yalms from Ashita's squared entity distance. A NaN stays NaN, so no distance limit passes it; a negative value
    // becomes 0.
    float DistanceFromSquared(float squared);
}
