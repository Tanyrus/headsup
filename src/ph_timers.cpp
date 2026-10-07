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

    std::optional<PhSighting> SightingOf(const MobRecord* mob, bool alive, bool bodyDrawn)
    {
        if (mob == nullptr || mob->placeholderOf == 0) return std::nullopt;
        const MobRecord* nm = MobById(mob->placeholderOf);
        return PhSighting{mob->id, mob->respawn, nm != nullptr ? nm->name : kUnknownNmName, alive, bodyDrawn};
    }

    void PhTimers::Update(double now, const std::vector<PhSighting>& placeholders)
    {
        std::unordered_map<uint32_t, bool> inRange;
        for (const PhSighting& ph : placeholders)
        {
            const auto before = m_WasAlive.find(ph.serverId);
            if (ph.alive)
                m_Timers.erase(ph.serverId);
            else if (ph.respawnSeconds > 0 && before != m_WasAlive.end() && before->second && ph.bodyDrawn)
                m_Timers[ph.serverId] = Timer{ph.serverId, ph.nmName, now + kDeathToDespawnSeconds + ph.respawnSeconds};
            inRange[ph.serverId] = ph.alive;
        }
        m_WasAlive = std::move(inRange);
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
            lines.push_back(TimerLine{timer->nmName + " " + MobIdText(timer->serverId, false, MobIdFormat::LastThree, false) + " " +
                                          (up ? kUpText : Countdown(timer->dueAt - now)),
                up});
        }
        return lines;
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
