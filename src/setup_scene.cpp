#include "setup_scene.h"

#include "entity_state.h"
#include "game_font.h"
#include "plate_compose.h"

namespace headsup
{
    namespace
    {
        constexpr const char* kTonberryName  = "Tonberry Cutter";
        constexpr const char* kShantottoName = "Shantotto";
        constexpr const char* kAdventurerName = "Adventurer";
        constexpr uint32_t kAdventurerPearl   = 0xFF5FA8FF; // her linkshell's color
        constexpr float kBesideYalms         = 2.5f;  // between the actors standing side by side
        constexpr float kNameWidth = 60.0f, kNameHeight = 12.0f, kNameAboveHead = 4.0f; // pixels of the picture
        // The colors the game draws these names in, by SampleActor, as /hu debug reports read them: an unclaimed
        // mob's, yours, an NPC's and another player's.
        constexpr uint32_t kGameNameColors[]{0xFFFFFFC2, 0xFFC2C2FF, 0xFFC2FFC2, 0xFFFFFFFF};

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
        ActorInput you = Actor(SampleActor::You, EntityKind::Player, 0, yourName.c_str(), -kBesideYalms);
        you.self       = true;
        scene.actors.push_back(you);
        scene.actors.push_back(Actor(SampleActor::Shantotto, EntityKind::Npc, kSampleShantottoId, kShantottoName, kBesideYalms));
        ActorInput her = Actor(SampleActor::Adventurer, EntityKind::Player, 0, kAdventurerName, -2.0f * kBesideYalms);
        PlayerStatus status;
        status.linkshell     = true;
        status.linkshellArgb = kAdventurerPearl;
        status.mentor        = true;
        status.seekingParty  = true;
        her.status           = status;
        her.linkshell        = true;
        scene.actors.push_back(her);
        return scene;
    }

    DrawnName SampleName(SampleActor actor, float x, float y)
    {
        DrawnName name;
        name.box.Add(x - kNameWidth / 2.0f, y - kNameAboveHead - kNameHeight);
        name.box.Add(x + kNameWidth / 2.0f, y - kNameAboveHead);
        name.depth = DepthAtYalms(kSampleNameYalms);
        name.color = kGameNameColors[static_cast<size_t>(actor)];
        return name;
    }

    Settings WithGameNames(const Settings& settings, const Tracker& tracker, const PlayerState& player)
    {
        Settings shown = settings;
        for (const ActorPtr actor : tracker.Actors())
        {
            const ActorInfo* info = tracker.Find(actor);
            if (info == nullptr || HookActionFor(*info, settings, player, false, false) != HookAction::Keep) continue;
            shown.enabled = shown.nameplates = true;
            KindSettings& kind = shown.Kind(PlateKindOf(info->state));
            kind.rows[RowOf(info->state)].plate = PlateMode::Shown;
            for (LineDesign& line : kind.lines)
                line = LineDesign{};
            LineDesign& name  = kind.lines[kBottomLine];
            name.text         = kTagName;
            name.font         = kGameFontName;
            name.color.source = ColorSource::Game;
            name.glow         = 0;
            kind.ornament.place = OrnamentPlace::Disabled;
        }
        return shown;
    }
}
