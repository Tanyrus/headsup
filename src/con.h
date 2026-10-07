#pragma once

#include <cstdint>

namespace headsup
{
    // /check difficulty in Phoenix's EMobDifficulty order: the 0x029 check reply carries 0x40 + this value.
    enum class Con : uint8_t
    {
        TooWeak,
        IncrediblyEasyPrey,
        EasyPrey,
        DecentChallenge,
        EvenMatch,
        Tough,
        VeryTough,
        IncrediblyTough,
    };
    constexpr int kConCount = 8;

    // What a /check reported for one spawn.
    struct CheckResult
    {
        int level = 0;
        Con con   = Con::TooWeak;
    };

    // Phoenix's charutils::GetBaseExp with the era table it loads (modules/era toau_experience_points.lua).
    uint32_t BaseExp(int playerLevel, int mobLevel);

    // Phoenix's charutils::CheckMob with the era difficulty curve, which never returns Incredibly Easy Prey.
    Con Difficulty(int playerLevel, int mobLevel);

    bool IsTooWeak(int playerLevel, int mobLevel);

    // TW, IEP, EP, DC, EM, T, VT, IT; "??" for anything else.
    const char* Abbrev(Con con);
}
