#include "plate_template.h"

#include "utf16.h"

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
            {Tag::Title, "$title", "Title", "The player's title: yours, and a player's once you have checked them.", kPlayerKinds},
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

        constexpr char kNameOpen = '{', kNameClose = '}'; // around a tag's name, where a letter follows it

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

    namespace
    {
        // A piece of a line's text: a tag, or text with what it shows (a doubled mark is one) beside what was typed.
        struct Token
        {
            bool isTag = false;
            Tag tag    = Tag::Name;
            std::string typed, shown;
        };

        std::vector<Token> Tokens(std::string_view text)
        {
            std::vector<Token> tokens;
            const auto literal = [&](std::string_view typed, std::string_view shown) {
                if (tokens.empty() || tokens.back().isTag) tokens.push_back(Token{});
                tokens.back().typed += typed;
                tokens.back().shown += shown;
            };
            for (size_t i = 0; i < text.size();)
            {
                if (text[i] != kTagMark)
                {
                    literal(text.substr(i, 1), text.substr(i, 1));
                    ++i;
                    continue;
                }
                if (i + 1 < text.size() && text[i + 1] == kTagMark)
                {
                    literal(text.substr(i, 2), text.substr(i, 1));
                    i += 2;
                    continue;
                }
                // The name straight after the mark, or in braces after it.
                const bool braced = i + 1 < text.size() && text[i + 1] == kNameOpen;
                const size_t name = i + (braced ? 2 : 1);
                size_t end        = name;
                while (end < text.size() && IsLower(text[end])) ++end;
                const bool closed     = !braced || (end < text.size() && text[end] == kNameClose);
                const TagEntry* entry = closed ? FindTag(text.substr(name, end - name)) : nullptr;
                const size_t past     = entry != nullptr && braced ? end + 1 : end;
                if (entry != nullptr)
                    tokens.push_back(Token{true, entry->tag, {}, {}});
                else
                    literal(text.substr(i, past - i), text.substr(i, past - i));
                i = past;
            }
            return tokens;
        }

        bool OddMarksAtEnd(std::string_view text)
        {
            size_t marks = 0;
            while (marks < text.size() && text[text.size() - 1 - marks] == kTagMark) ++marks;
            return marks % 2 != 0;
        }

        // The stretch and the byte in it that a caret stands for in these runs.
        RunCaret Placed(const TemplateRuns& runs, RunCaret caret)
        {
            const int last = static_cast<int>(runs.texts.size()) - 1;
            if (caret.run < 0 || caret.run > last) return RunCaret{last, runs.texts.back().size()};
            return RunCaret{caret.run, std::min(caret.at, runs.texts[static_cast<size_t>(caret.run)].size())};
        }
    }

    std::vector<Atom> ParseTemplate(std::string_view text)
    {
        std::vector<Atom> atoms;
        for (Token& token : Tokens(text))
            atoms.push_back(Atom{token.isTag, token.tag, std::move(token.shown)});
        return atoms;
    }

    TemplateRuns RunsOf(std::string_view text)
    {
        TemplateRuns runs{{std::string()}, {}};
        for (Token& token : Tokens(text))
        {
            if (!token.isTag)
            {
                runs.texts.back() = std::move(token.typed);
                continue;
            }
            runs.tags.push_back(token.tag);
            runs.texts.emplace_back();
        }
        return runs;
    }

    std::string JoinRuns(const TemplateRuns& runs)
    {
        std::string text;
        for (size_t i = 0; i < runs.texts.size(); ++i)
        {
            text += runs.texts[i];
            if (i >= runs.tags.size()) break;
            if (OddMarksAtEnd(runs.texts[i])) text += kTagMark;
            const std::string_view name = TagName(runs.tags[i]) + 1;
            const std::string& next     = runs.texts[i + 1];
            // A small letter straight after the name would be read as more of it.
            if (!next.empty() && IsLower(next.front()))
                text.append(1, kTagMark).append(1, kNameOpen).append(name).append(1, kNameClose);
            else
                text.append(1, kTagMark).append(name);
        }
        return text;
    }

    std::vector<Tag> TagsFor(PlateKind kind)
    {
        std::vector<Tag> tags;
        for (const TagEntry& entry : kTags)
            if (entry.kinds & KindBit(kind)) tags.push_back(entry.tag);
        return tags;
    }

    bool PutTag(std::string& text, RunCaret& caret, Tag tag)
    {
        TemplateRuns runs  = RunsOf(text);
        const RunCaret at  = Placed(runs, caret);
        const auto run     = static_cast<size_t>(at.run);
        const std::string after = runs.texts[run].substr(at.at);
        runs.texts[run].resize(at.at);
        runs.texts.insert(runs.texts.begin() + at.run + 1, after);
        runs.tags.insert(runs.tags.begin() + at.run, tag);
        std::string put = JoinRuns(runs);
        if (put.size() > kMaxTemplate) return false;
        text  = std::move(put);
        caret = RunCaret{at.run + 1, 0};
        return true;
    }

    std::string WithoutTag(std::string_view text, int tag, RunCaret& caret)
    {
        TemplateRuns runs = RunsOf(text);
        if (tag < 0 || tag >= static_cast<int>(runs.tags.size())) return std::string(text);
        const auto at = static_cast<size_t>(tag);
        caret         = RunCaret{tag, runs.texts[at].size()};
        runs.texts[at] += runs.texts[at + 1];
        runs.texts.erase(runs.texts.begin() + tag + 1);
        runs.tags.erase(runs.tags.begin() + tag);
        return JoinRuns(runs);
    }

    std::string WithRun(std::string_view text, int run, std::string_view typed)
    {
        TemplateRuns runs = RunsOf(text);
        if (run < 0 || run >= static_cast<int>(runs.texts.size())) return std::string(text);
        std::string& stretch = runs.texts[static_cast<size_t>(run)];
        stretch              = typed;
        std::string joined   = JoinRuns(runs);
        // Too long, the stretch typed loses what is over, a whole character at a time.
        while (joined.size() > kMaxTemplate && !stretch.empty())
        {
            size_t end = stretch.size() - std::min(stretch.size(), joined.size() - kMaxTemplate);
            while (end > 0 && ContinuesCharacter(stretch[end])) --end;
            stretch.resize(end);
            joined = JoinRuns(runs);
        }
        return joined;
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
