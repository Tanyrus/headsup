#pragma once

#include "classifier.h"
#include "icons.h"
#include "labels.h"
#include "player_status.h"
#include "pose.h"
#include "settings.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
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
        const CheckResult* checked; // this spawn's latest /check, or nullptr; only read during Update
        std::optional<PlayerStatus> status; // a player's
        Pose pose;                   // a player's; others stand
        WorldPoint feet;
    };

    struct ActorInfo
    {
        bool outline       = false; // a mob that gets an outline this frame
        uint8_t stencilRef = 0;     // 1 to kMaxStencilRef when outlined
        uint32_t argb      = 0;     // outline colour (D3DCOLOR)
        uint16_t index     = 0;     // entity target index
        EntityKind kind    = EntityKind::Npc;
        bool alive         = false;
        char name[32]      = {};    // for the replacement nameplate
        Label label{};              // level and con text: every living mob
        IconSet icons{};            // the MobDB icon row: every living mob with data
        PlayerIconRows nameIcons{}; // a player's icons, either side of the name
        uint32_t linkshellArgb = 0; // a player's linkshell color, for its icon
        Pose pose              = Pose::Standing;
        WorldPoint feet{};
    };

    // Whether HeadsUp draws this entity's name in place of the game's: its kind's is replaced and it has one.
    bool ReplacesName(const Settings& settings, const ActorInfo& info);

    // Per-frame table of every entity's actor pointer and name, and each mob's level, icons and outline decision. A
    // draw's owner is the first actor pointer of any kind on the stack.
    class Tracker
    {
    public:
        void Update(const std::vector<ActorInput>& actors, const PlayerState& player, const Settings& settings);
        const ActorInfo* Find(ActorPtr actor) const;
        uint32_t OutlinedCount() const { return m_Outlined; }
        // Every actor pointer, in entity order.
        const std::vector<ActorPtr>& Actors() const { return m_Order; }

    private:
        std::unordered_map<ActorPtr, ActorInfo> m_Actors;
        std::vector<ActorPtr> m_Order;
        uint32_t m_Outlined = 0;
        ActorPtr m_Min = 0;
        ActorPtr m_Max = 0;
    };

    // Yalms from Ashita's squared entity distance. A NaN stays NaN, so no distance limit passes it; a negative value
    // becomes 0.
    float DistanceFromSquared(float squared);
}
