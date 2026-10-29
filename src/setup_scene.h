#pragma once

#include "game_cursor.h"
#include "name_frame.h"
#include "player_status.h"
#include "tracker.h"

#include <cstdint>
#include <string>
#include <vector>

namespace headsup
{
    // What Setup's preview shows: a Tonberry Cutter of the Temple of Uggalepih (level 52 to 55, aggressive, links,
    // sight) as you would meet it at level 55, your target; and on the step for players and NPCs, you on its left and
    // Shantotto of Windurst Walls on its right. Each is an actor the tracker takes as it takes the world's.
    constexpr uint32_t kSampleTonberryId    = 17428524;
    constexpr uint16_t kSampleTonberryModel = 0x04B2;
    constexpr uint32_t kSampleShantottoId   = 17756186;
    constexpr PlayerLook kShantottoLook{6 | 6 << 8, 0x1000, 0x2013, 0x3013, 0x4013, 0x5013, 0x6000, 0x7000, 0};
    constexpr int kSampleLevel = 55;
    // How far the actors stand from you, near enough to be whole, and how far their names are drawn from, where the
    // distance scale is 100 percent.
    constexpr float kSampleYalms     = 8.0f;
    constexpr float kSampleNameYalms = 30.0f;

    enum class SampleActor : uint8_t { Tonberry, You, Shantotto };
    constexpr uint16_t kFirstSampleIndex = 0x400; // past the zone's own entities, none of which the scene holds
    constexpr uint16_t SampleIndex(SampleActor actor) { return static_cast<uint16_t>(kFirstSampleIndex + static_cast<int>(actor)); }

    struct SampleScene
    {
        std::vector<ActorInput> actors;
        PlayerState player;
        CursorTargets targets;
    };
    // others: you and Shantotto stand beside the Tonberry. yourName: as the game names you, kept by the caller while
    // the tracker reads it.
    SampleScene SetupScene(bool others, const std::string& yourName);

    // The name the game would draw for an actor whose head is at x, y of the picture: a small white one just above it,
    // at the depth of a name kSampleNameYalms away.
    DrawnName SampleName(float x, float y);
}
