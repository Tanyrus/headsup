#include "setup_scene.h"

namespace headsup
{
    namespace
    {
        constexpr const char* kTonberryName  = "Tonberry Cutter";
        constexpr const char* kShantottoName = "Shantotto";
        constexpr float kBeside              = 2.5f;  // yalms between the actors standing side by side
        constexpr float kNameWidth = 60.0f, kNameHeight = 12.0f, kNameAboveHead = 4.0f; // pixels of the picture
        constexpr uint32_t kGameNameWhite = 0xFFFFFFFF;

        ActorInput Actor(SampleActor which, EntityKind kind, uint32_t serverId, const char* name, float x)
        {
            ActorInput actor{};
            actor.actor    = static_cast<ActorPtr>(which) + 1;
            actor.index    = SampleIndex(which);
            actor.serverId = serverId;
            actor.kind     = kind;
            actor.alive    = true;
            actor.distance = kSampleYalms;
            actor.name     = name;
            actor.feet     = {x, 0.0f, kSampleYalms};
            return actor;
        }
    }

    SampleScene SetupScene(bool others, const std::string& yourName)
    {
        SampleScene scene;
        scene.player.level   = kSampleLevel;
        scene.targets.target = SampleIndex(SampleActor::Tonberry);
        scene.actors.push_back(Actor(SampleActor::Tonberry, EntityKind::Mob, kSampleTonberryId, kTonberryName, 0.0f));
        if (!others) return scene;
        ActorInput you = Actor(SampleActor::You, EntityKind::Player, 0, yourName.c_str(), -kBeside);
        you.self       = true;
        scene.actors.push_back(you);
        scene.actors.push_back(Actor(SampleActor::Shantotto, EntityKind::Npc, kSampleShantottoId, kShantottoName, kBeside));
        return scene;
    }

    DrawnName SampleName(float x, float y)
    {
        DrawnName name;
        name.box.Add(x - kNameWidth / 2.0f, y - kNameAboveHead - kNameHeight);
        name.box.Add(x + kNameWidth / 2.0f, y - kNameAboveHead);
        name.depth = DepthAtYalms(kSampleNameYalms);
        name.color = kGameNameWhite;
        return name;
    }
}
