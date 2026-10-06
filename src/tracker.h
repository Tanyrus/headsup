#pragma once

#include "classifier.h"
#include "labels.h"
#include "settings.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace aggroglow
{
    using ActorPtr = uint32_t; // FFXI is a 32-bit process

    struct ActorInput
    {
        ActorPtr actor;
        uint16_t index;    // entity target index
        uint32_t serverId; // the mob data key
        bool isMob;        // spawn flag 0x10
        bool alive;       // HP above 0 (defeated mobs keep their model until they despawn)
        float distance;   // yalms from the player
        const char* name; // only read during Update
        const CheckResult* examined; // this spawn's latest /check, or nullptr; only read during Update
    };

    struct ActorInfo
    {
        bool outline       = false; // a mob that gets an outline this frame
        uint8_t stencilRef = 0;     // 1-255 when outlined
        uint32_t argb      = 0;     // outline colour (D3DCOLOR)
        uint16_t index     = 0;     // entity target index
        Label label{};              // level and con text, set when outlined
    };

    // Per-frame table of every entity's actor pointer and the outline decision for each mob. Players and NPCs are
    // included: a draw's owner is the first actor pointer of any kind on the stack.
    class Tracker
    {
    public:
        void Update(const std::vector<ActorInput>& actors, const PlayerState& player, const Settings& settings);
        const ActorInfo* Find(ActorPtr actor) const;
        uint32_t OutlinedCount() const { return static_cast<uint32_t>(m_Outlined.size()); }
        // Actor pointers of the mobs outlined this frame, in entity order.
        const std::vector<ActorPtr>& OutlinedActors() const { return m_Outlined; }

    private:
        std::unordered_map<ActorPtr, ActorInfo> m_Actors;
        std::vector<ActorPtr> m_Outlined;
        ActorPtr m_Min = 0;
        ActorPtr m_Max = 0;
    };
}
