#include "entity_state.h"

#include <algorithm>
#include <cctype>
#include <optional>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kSpawnFlagPlayer = 0x01;
        constexpr uint32_t kSpawnFlagMob    = 0x10;
    }

    EntityKind KindFromSpawnFlags(uint32_t flags)
    {
        if ((flags & kSpawnFlagMob) != 0) return EntityKind::Mob;
        return (flags & kSpawnFlagPlayer) != 0 ? EntityKind::Player : EntityKind::Npc;
    }

    Nature NatureOf(Category classified, bool placeholder)
    {
        if (placeholder) return Nature::Placeholder;
        switch (classified)
        {
        case Category::WillAttack: return Nature::Aggressive;
        case Category::WontAttack: return Nature::Passive;
        case Category::NmWillAttack: return Nature::AggressiveNm;
        case Category::NmWontAttack: return Nature::PassiveNm;
        case Category::Placeholder: return Nature::Placeholder;
        default: return Nature::NoData;
        }
    }

    Category MobRowOf(Nature nature, bool claimedByParty, bool claimed, bool alive)
    {
        if (!alive) return Category::Dead;
        if (claimedByParty) return Category::PartyClaim;
        if (claimed) return Category::OtherClaim;
        switch (nature)
        {
        case Nature::Passive: return Category::WontAttack;
        case Nature::Aggressive: return Category::WillAttack;
        case Nature::PassiveNm: return Category::NmWontAttack;
        case Nature::AggressiveNm: return Category::NmWillAttack;
        case Nature::Placeholder: return Category::Placeholder;
        case Nature::NoData: break;
        }
        return Category::Unknown;
    }

    PlayerRow PlayerRowOf(bool party, bool isFriend, bool linkshell)
    {
        if (party) return PlayerRow::PartyMember;
        if (isFriend) return PlayerRow::Friend;
        return linkshell ? PlayerRow::LinkshellMember : PlayerRow::Player;
    }

    PetRow PetRowOf(bool ownerIsYou, bool ownerInParty, bool ownerIsFriend)
    {
        if (ownerIsYou) return PetRow::Yours;
        if (ownerInParty) return PetRow::Party;
        return ownerIsFriend ? PetRow::Friend : PetRow::Others;
    }

    PlateKind PlateKindOf(const EntityState& s)
    {
        if (s.pet) return PlateKind::Pets;
        if (s.kind == EntityKind::Mob) return PlateKind::Mobs;
        if (s.kind == EntityKind::Player) return s.self ? PlateKind::Self : PlateKind::Players;
        return PlateKind::Npcs;
    }

    int RowOf(const EntityState& s)
    {
        switch (PlateKindOf(s))
        {
        case PlateKind::Mobs: return CategoryIndex(s.row);
        case PlateKind::Players: return static_cast<int>(s.playerRow);
        case PlateKind::Pets: return static_cast<int>(s.petRow);
        default: return 0;
        }
    }

    TargetType TypeOf(const EntityState& s)
    {
        switch (PlateKindOf(s))
        {
        case PlateKind::Mobs: return TargetType::Mob;
        case PlateKind::Pets: return TargetType::Pet;
        case PlateKind::Npcs: return TargetType::Npc;
        case PlateKind::Self: return TargetType::Player;
        case PlateKind::Players: break;
        }
        switch (s.playerRow)
        {
        case PlayerRow::PartyMember: return TargetType::PartyMember;
        case PlayerRow::Friend: return TargetType::Friend;
        case PlayerRow::LinkshellMember: return TargetType::LinkshellMember;
        case PlayerRow::Player: break;
        }
        return TargetType::Player;
    }

    bool Targeted(TargetState target) { return target != TargetState::None; }

    TargetState TargetUnderPick(uint16_t index, const CursorTargets& targets)
    {
        if (index == 0 || index != targets.subTarget || index != targets.target) return TargetState::None;
        return targets.locked ? TargetState::Locked : TargetState::Targeted;
    }

    TargetState TargetStateOf(uint16_t index, const CursorTargets& targets)
    {
        if (index == 0) return TargetState::None;
        if (index == targets.subTarget) return targets.outOfRange ? TargetState::OutOfRange : TargetState::SubTarget;
        if (index == targets.target) return targets.locked ? TargetState::Locked : TargetState::Targeted;
        return TargetState::None;
    }

    uint32_t ResolveColor(const ColorPick& pick, const EntityState& s, const Palette& p, uint32_t gameArgb)
    {
        switch (pick.source)
        {
        case ColorSource::Custom: return ToArgb(pick.custom);
        case ColorSource::Game: return gameArgb;
        case ColorSource::Aggression: return ToArgb(p.aggression[static_cast<int>(s.nature)]);
        case ColorSource::Toughness: return ToArgb(p.toughness[static_cast<int>(s.toughness)]);
        case ColorSource::Claim: return ToArgb(p.claim[static_cast<int>(s.claim)]);
        case ColorSource::Target: return ToArgb(p.target[static_cast<int>(s.target)]);
        case ColorSource::Type: return ToArgb(p.type[static_cast<int>(TypeOf(s))]);
        }
        return ToArgb(pick.custom);
    }

    bool InCombat(const EntityState& state)
    {
        return PlateKindOf(state) == PlateKind::Mobs ? state.claim == ClaimState::Yours : state.engaged;
    }

    uint32_t CursorColorOf(const ColorPick& pick, const EntityState& state, const Palette& palette)
    {
        const std::vector<ColorSource> readable = SourcesFor(PlateKindOf(state), false);
        const bool reads = std::ranges::find(readable, pick.source) != readable.end();
        return ResolveColor(reads ? pick : ColorPick{ColorSource::Target}, state, palette, kWhite);
    }

    namespace
    {
        struct Deciding
        {
            const RowSettings* row;
            Nature nature; // the aggression an Aggression pick reads
        };

        // Empty when the chain ends on a row that is itself ByNature.
        std::optional<Deciding> Decide(const EntityState& s, const Settings& settings)
        {
            const PlateKind plateKind = PlateKindOf(s);
            const KindSettings& kind  = settings.Kind(plateKind);
            if (plateKind != PlateKind::Mobs) return Deciding{&kind.rows[RowOf(s)], s.nature};
            Category row = s.row;
            Nature nature = s.nature;
            for (;;)
            {
                const RowSettings& r = kind.rows[CategoryIndex(row)];
                if (r.outline != OutlineMode::ByNature) return Deciding{&r, nature};
                if (row == s.classified) return std::nullopt;
                if (IsClaimRow(row) && s.nature == Nature::Placeholder)
                    row = Category::Placeholder;
                else
                {
                    row    = s.classified;
                    nature = NatureOf(s.classified, false);
                }
            }
        }
    }

    OutlineMode OutlineModeOf(const EntityState& s, const Settings& settings)
    {
        const std::optional<Deciding> d = Decide(s, settings);
        return d ? d->row->outline : OutlineMode::Disabled;
    }

    uint32_t RowColor(const EntityState& s, const Settings& settings)
    {
        const std::optional<Deciding> d = Decide(s, settings);
        if (!d) return kWhite;
        const ColorPick& pick = d->row->outlineColor;
        if (pick.source == ColorSource::Aggression) return ToArgb(settings.palette.aggression[static_cast<int>(d->nature)]);
        return ResolveColor(pick, s, settings.palette, kWhite); // Game is not offered for outlines
    }

    namespace
    {
        bool Holds(ShowCondition c, const EntityState& s)
        {
            const bool player = s.kind == EntityKind::Player && !s.pet;
            // By its aggression alone: a placeholder's nature does not say whether it would attack.
            const bool attacks = s.classified == Category::WillAttack || s.classified == Category::NmWillAttack;
            switch (c)
            {
            case ShowCondition::Targeted: return Targeted(s.target);
            case ShowCondition::Unclaimed: return s.kind == EntityKind::Mob && s.claim == ClaimState::Unclaimed;
            case ShowCondition::YourClaim: return s.claim == ClaimState::Yours;
            case ShowCondition::OthersClaim: return s.claim == ClaimState::Others;
            case ShowCondition::Engaged: return s.engaged;
            case ShowCondition::TooWeak: return s.tooWeak;
            case ShowCondition::Aggressive: return attacks;
            case ShowCondition::Passive: return s.kind == EntityKind::Mob && !attacks;
            case ShowCondition::PartyMember: return player && s.playerRow == PlayerRow::PartyMember;
            case ShowCondition::Friend: return player && s.playerRow == PlayerRow::Friend;
            case ShowCondition::LinkshellMember: return player && s.playerRow == PlayerRow::LinkshellMember;
            case ShowCondition::OwnerIsYou: return s.pet && s.petRow == PetRow::Yours;
            }
            return false;
        }
    }

    bool LineShows(const LineShow& show, const EntityState& s)
    {
        if (show.always) return true;
        for (int c = 0; c < kShowConditionCount; ++c)
            if ((show.when & ConditionBit(static_cast<ShowCondition>(c))) != 0 && Holds(static_cast<ShowCondition>(c), s)) return true;
        return false;
    }

    bool IsFriend(const std::vector<std::string>& friends, std::string_view name)
    {
        const auto lower = [](char ch) { return std::tolower(static_cast<unsigned char>(ch)); };
        const auto same  = [&](const std::string& entry)
        { return std::ranges::equal(entry, name, [&](char a, char b) { return lower(a) == lower(b); }); };
        return std::ranges::any_of(friends, same);
    }

    bool InYourParty(uint32_t serverId, bool self, std::span<const uint32_t> memberIds)
    {
        return !self && std::ranges::find(memberIds, serverId) != memberIds.end();
    }

    std::unordered_map<uint16_t, uint16_t> PetOwners(std::span<const PetLink> links)
    {
        std::unordered_map<uint16_t, uint16_t> owners;
        for (const PetLink& link : links)
            if (link.ownerKind == EntityKind::Player && link.pet != 0 && link.pet != link.owner)
                owners.try_emplace(link.pet, link.owner);
        return owners;
    }

    std::optional<PlayerJobs> KnownJobs(const PlayerJobs* own, const PlayerJobs* member, const InspectGeneral* checked)
    {
        for (const PlayerJobs* jobs : {own, member})
            if (jobs != nullptr && jobs->mainJob != 0) return *jobs;
        if (checked != nullptr && checked->mainJob != 0)
            return PlayerJobs{checked->mainJob, checked->mainLevel, checked->subJob, checked->subLevel};
        return std::nullopt;
    }
}
