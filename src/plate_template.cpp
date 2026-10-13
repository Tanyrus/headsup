#include "plate_template.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr uint8_t KindBit(PlateKind kind) { return static_cast<uint8_t>(1u << static_cast<int>(kind)); }
        constexpr uint8_t kMobKinds    = KindBit(PlateKind::Mobs);
        constexpr uint8_t kPlayerKinds = KindBit(PlateKind::Players) | KindBit(PlateKind::Self);
        constexpr uint8_t kNpcKinds    = KindBit(PlateKind::Npcs);
        constexpr uint8_t kPetKinds    = KindBit(PlateKind::Pets);
        constexpr uint8_t kAnyKinds    = kMobKinds | kPlayerKinds | kNpcKinds | kPetKinds;

        struct TagEntry
        {
            Tag tag;
            const char* name;  // with the mark
            const char* label; // on its chip
            const char* tip;
            uint8_t kinds;    // the kinds whose page offers it
        };
        // The order is the chips' order on each page that offers the tag, so it need not follow the enum.
        const TagEntry kTags[] = {
            {Tag::Name, "$name", "Name", "The entity's name.", kAnyKinds},
            {Tag::Gm, "$gm", "GM", "The GM icon.", kPlayerKinds},
            {Tag::Mentor, "$mentor", "Mentor", "The mentor icon.", kPlayerKinds},
            {Tag::New, "$new", "New Adventurer", "The new adventurer icon.", kPlayerKinds},
            {Tag::Sync, "$sync", "Level Sync", "The level sync icon.", kPlayerKinds},
            {Tag::Away, "$away", "Away", "The away icon.", kPlayerKinds},
            {Tag::Linkshell, "$linkshell", "Linkshell", "The linkshell icon, in the linkshell's color.", kPlayerKinds},
            {Tag::Bazaar, "$bazaar", "Bazaar", "The bazaar icon.", kPlayerKinds},
            {Tag::Seeking, "$seeking", "Seeking Party", "The seeking party icon.", kPlayerKinds},
            {Tag::Job, "$job", "Job", "Jobs as WAR/MNK: yours, your party's, and a player's once you have checked them.", kPlayerKinds},
            {Tag::Level, "$level", "Level",
                "A mob's level, or range, as 20-23; its exact level after a /check. A player's main job level when known.",
                kMobKinds | kPlayerKinds},
            {Tag::Con, "$con", "Difficulty", "The con at each end of the level range, as EP-DC; one con after a /check.", kMobKinds},
            {Tag::Id, "$id", "ID", "The last three hex digits of the mob's ID, as players name placeholders: [106].", kMobKinds},
            {Tag::ServerId, "$serverid", "Server ID", "The mob's whole server ID in decimal: [17190918].", kMobKinds},
            {Tag::Ph, "$ph", "Placeholder", "[PH] on a lottery placeholder, whose death can pop an NM; nothing on other mobs.", kMobKinds},
            {Tag::Detect, "$detect", "Detection", "Aggression, links, and how it detects you, as picked in Detection Icons.", kMobKinds},
            {Tag::Weak, "$weak", "Weaknesses", "Damage types and elements it is weak to, with their percents or ranks.", kMobKinds},
            {Tag::Resist, "$resist", "Resistances", "Damage types and elements it resists, with their percents or ranks.", kMobKinds},
            {Tag::Status, "$status", "Statuses", "Statuses it is weak to or resists, as picked in Statuses.", kMobKinds},
            {Tag::Immune, "$immune", "Immunities", "Statuses it cannot take, each with a times sign.", kMobKinds},
            {Tag::Owner, "$owner", "Owner", "The pet's owner.", kPetKinds},
        };
        static_assert(std::size(kTags) == kTagCount);

        const TagEntry& EntryOf(Tag tag)
        {
            return *std::ranges::find(kTags, tag, &TagEntry::tag);
        }

        bool IsLower(char c) { return c >= 'a' && c <= 'z'; }

        // The tag whose name past its mark is the letters, if any.
        const TagEntry* FindTag(std::string_view letters)
        {
            for (const TagEntry& entry : kTags)
                if (letters == entry.name + 1) return &entry;
            return nullptr;
        }
    }

    const char* TagName(Tag tag) { return EntryOf(tag).name; }
    const char* TagLabel(Tag tag) { return EntryOf(tag).label; }
    const char* TagTip(Tag tag) { return EntryOf(tag).tip; }

    std::optional<Icon> TagIcon(Tag tag)
    {
        switch (tag)
        {
        case Tag::Gm: return Icon::Gm;
        case Tag::Mentor: return Icon::Mentor;
        case Tag::New: return Icon::NewAdventurer;
        case Tag::Sync: return Icon::LevelSync;
        case Tag::Away: return Icon::Away;
        case Tag::Linkshell: return Icon::Linkshell;
        case Tag::Bazaar: return Icon::Bazaar;
        case Tag::Seeking: return Icon::Invite;
        default: return std::nullopt;
        }
    }

    std::vector<Atom> ParseTemplate(std::string_view text)
    {
        std::vector<Atom> atoms;
        std::string run;
        const auto closeRun = [&]
        {
            if (!run.empty()) atoms.push_back(Atom{false, Tag::Name, std::move(run)});
            run.clear();
        };
        for (size_t i = 0; i < text.size();)
        {
            if (text[i] != kTagMark)
            {
                run += text[i++];
                continue;
            }
            if (i + 1 < text.size() && text[i + 1] == kTagMark)
            {
                run += kTagMark;
                i += 2;
                continue;
            }
            size_t end = i + 1;
            while (end < text.size() && IsLower(text[end])) ++end;
            const TagEntry* entry = FindTag(text.substr(i + 1, end - i - 1));
            if (!entry)
            {
                run.append(text.substr(i, end - i));
            }
            else
            {
                closeRun();
                atoms.push_back(Atom{true, entry->tag, ""});
            }
            i = end;
        }
        closeRun();
        return atoms;
    }

    std::vector<Tag> TagsFor(PlateKind kind)
    {
        std::vector<Tag> tags;
        for (const TagEntry& entry : kTags)
            if (entry.kinds & KindBit(kind)) tags.push_back(entry.tag);
        return tags;
    }

    std::string InsertTag(std::string text, Tag tag, size_t caret)
    {
        const std::string_view name = TagName(tag);
        if (text.size() + name.size() > kMaxTemplate) return text;
        text.insert(std::min(caret, text.size()), name);
        return text;
    }

    bool PutTag(std::string& text, size_t& caret, Tag tag)
    {
        const size_t at  = std::min(caret, text.size());
        const size_t was = text.size();
        text             = InsertTag(std::move(text), tag, at);
        if (text.size() == was) return false;
        caret = at + (text.size() - was);
        return true;
    }

    std::string UnusedTagsNote(const KindSettings& kind, std::initializer_list<Tag> tags, const char* ending)
    {
        constexpr const char* kStart = "Put the ";
        constexpr const char* kComma = ", ";
        constexpr const char* kOr    = " or ";
        constexpr const char* kWhere = " tag on a line of the design";
        std::string note;
        size_t left = tags.size();
        for (const Tag tag : tags)
        {
            if (UsesTag(kind, tag)) return {};
            note += note.empty() ? kStart : left > 1 ? kComma : kOr;
            note += TagLabel(tag);
            --left;
        }
        return note.empty() ? note : note + kWhere + ending;
    }

    bool UsesTag(const KindSettings& kind, Tag tag)
    {
        for (const LineDesign& line : kind.lines)
            for (const Atom& atom : ParseTemplate(line.text))
                if (atom.isTag && atom.tag == tag) return true;
        return false;
    }
}
