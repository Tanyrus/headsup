#pragma once

#include "game_cursor.h"
#include "game_packets.h"
#include "labels.h"
#include "plate_settings.h"
#include "settings.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace headsup
{
    enum class EntityKind : uint8_t
    {
        Mob,
        Player, // you included
        Npc,
    };
    // From IEntity::GetSpawnFlags: the mob bit wins over the player bit, and neither means an NPC.
    EntityKind KindFromSpawnFlags(uint32_t flags);

    struct PlayerJobs
    {
        uint8_t mainJob = 0, mainLevel = 0, subJob = 0, subLevel = 0;
        bool operator==(const PlayerJobs&) const = default;
    };

    struct EntityState
    {
        EntityKind kind     = EntityKind::Npc;
        bool self = false, pet = false, alive = false, engaged = false, tooWeak = false;
        Nature nature       = Nature::NoData;
        Category row        = Category::Unknown; // mobs
        Category classified = Category::Unknown; // the row its aggression alone gives, under any claim or placeholder row
        PlayerRow playerRow = PlayerRow::Player; // players
        LabelShade toughness = LabelShade::Unknown;
        ClaimState claim    = ClaimState::Unclaimed;
        TargetState target  = TargetState::None;
        bool operator==(const EntityState&) const = default;
    };

    Nature NatureOf(Category classified, bool placeholder); // Classify's answer, with the placeholder flag
    Category MobRowOf(Nature nature, bool claimedByParty, bool claimed);
    PlayerRow PlayerRowOf(bool party, bool isFriend, bool linkshell);
    PlateKind PlateKindOf(const EntityState& state);
    int RowOf(const EntityState& state); // into KindSettings::rows
    TargetType TypeOf(const EntityState& state);
    bool Targeted(TargetState target);
    TargetState TargetStateOf(uint16_t index, const CursorTargets& targets);
    // A pick's color for this entity: a custom color, the game's name color, or the palette entry its state picks.
    uint32_t ResolveColor(const ColorPick& pick, const EntityState& state, const Palette& palette, uint32_t gameArgb);
    // The mode of the row that decides this entity's outline: its own, or for a mob the first row down the chain from a
    // claim to the Placeholder row to its aggression's row that is not ByNature (Disabled if the chain ends on one).
    OutlineMode OutlineModeOf(const EntityState& state, const Settings& settings);
    // The color that row's outline pick gives, whether or not the outline is drawn. Once the chain has fallen through to
    // the row the mob's aggression gives, an Aggression pick takes that row's nature.
    uint32_t RowColor(const EntityState& state, const Settings& settings);
    bool LineShows(const LineShow& show, const EntityState& state);
    // Whether the Friends list names this player, ignoring case.
    bool IsFriend(const std::vector<std::string>& friends, std::string_view name);
    // Whether a player is in your party or alliance: among the members' server IDs, and not you.
    bool InYourParty(uint32_t serverId, bool self, std::span<const uint32_t> memberIds);

    // An entity's target index and kind, and the index IEntity::GetPetTargetIndex gives for it: 0 with no pet out.
    struct PetLink
    {
        uint16_t owner;
        EntityKind ownerKind;
        uint16_t pet;
    };
    // Each pet's owner, by the pet's target index: only a player owns one, never itself, and of two naming one pet
    // the first listed.
    std::unordered_map<uint16_t, uint16_t> PetOwners(std::span<const PetLink> links);

    // A player's jobs from the first source that knows them: the game's for you, the party list's, then their last
    // check. Null for a source the player has none of; a main job of 0 (zoning, another zone, anonymous) knows nothing.
    std::optional<PlayerJobs> KnownJobs(const PlayerJobs* own, const PlayerJobs* member, const InspectGeneral* checked);
}
