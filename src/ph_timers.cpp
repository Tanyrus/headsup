#include "ph_timers.h"

#include "labels.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kZoneBits         = 0xFFF;
        constexpr int kSecondsPerMinute      = 60;
        constexpr int kSecondsPerHour        = 3600;
        constexpr const char* kUpText        = "up";
        constexpr const char* kUnknownNmName = "NM";
    }

    std::optional<PhSighting> SightingOf(const MobRecord* mob)
    {
        if (mob == nullptr) return std::nullopt;
        if (mob->placeholders > 0) return PhSighting{mob->id, mob->placeholderRespawn, mob->name};
        if (mob->placeholderOf == 0) return std::nullopt;
        const MobRecord* nm = MobById(mob->placeholderOf);
        return PhSighting{mob->id, mob->respawn, nm != nullptr ? nm->name : kUnknownNmName};
    }

    void PhTimers::Died(double now, const PhSighting& mob)
    {
        if (mob.respawnSeconds == 0) return;
        m_Timers[mob.serverId] = Timer{mob.serverId, mob.nmName,
            now - kDefeatMessageSeconds + kDeathToDespawnSeconds + static_cast<double>(mob.respawnSeconds)};
    }

    void PhTimers::Update(double now, const std::vector<uint32_t>& aliveMobs)
    {
        for (const uint32_t serverId : aliveMobs)
            m_Timers.erase(serverId);
        std::erase_if(m_Timers, [&](const auto& entry) { return now > entry.second.dueAt + kUpSeconds; });
    }

    std::vector<TimerLine> PhTimers::Lines(uint16_t zone, double now) const
    {
        std::vector<const Timer*> here;
        for (const auto& [serverId, timer] : m_Timers)
            if (((serverId >> kTargetIndexWidth) & kZoneBits) == zone) here.push_back(&timer);
        std::sort(here.begin(), here.end(), [](const Timer* a, const Timer* b) { return a->dueAt < b->dueAt; });
        std::vector<TimerLine> lines;
        for (const Timer* timer : here)
        {
            const bool up = now >= timer->dueAt;
            lines.push_back(TimerLine{timer->nmName + " " + IdText(timer->serverId) + " " +
                                          (up ? kUpText : Countdown(timer->dueAt - now)),
                up});
        }
        return lines;
    }

    std::optional<double> PhTimers::SecondsLeft(uint32_t serverId, double now) const
    {
        const auto it = m_Timers.find(serverId);
        if (it == m_Timers.end()) return std::nullopt;
        return std::max(0.0, it->second.dueAt - now);
    }

    std::string Countdown(double seconds)
    {
        const int total = static_cast<int>(std::ceil(seconds));
        const int hours = total / kSecondsPerHour, minutes = total % kSecondsPerHour / kSecondsPerMinute, rest = total % kSecondsPerMinute;
        char text[32];
        if (hours > 0)
            std::snprintf(text, sizeof(text), "%d:%02d:%02d", hours, minutes, rest);
        else
            std::snprintf(text, sizeof(text), "%d:%02d", minutes, rest);
        return text;
    }
}
