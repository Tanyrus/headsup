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
    constexpr int kConCount = static_cast<int>(Con::IncrediblyTough) + 1;

    // What a /check reported for one spawn.
    struct CheckResult
    {
        int level = 0;
        Con con   = Con::TooWeak;
    };

    // Phoenix's charutils::GetBaseExp with the table its map server loads. mobLevel includes the mob's level mod.
    uint32_t BaseExp(int playerLevel, int mobLevel);

    // Phoenix's charutils::CheckMob with the difficulty curve its map server loads.
    Con Difficulty(int playerLevel, int mobLevel);

    bool IsTooWeak(int playerLevel, int mobLevel);

    // TW, IEP, EP, DC, EM, T, VT, IT; "??" for anything else.
    const char* Abbrev(Con con);
}
