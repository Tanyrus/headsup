#pragma once

#include "icons.h"
#include "plate_settings.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    enum class Tag : uint8_t
    {
        Name, Level, Con, Id, ServerId, Ph, Detect, Weak, Resist, Status, Immune, Gm, Mentor, New, Sync, Away, Linkshell,
        Bazaar, Seeking, Job, Owner, Title
    };
    constexpr int kTagCount = static_cast<int>(Tag::Title) + 1;
    constexpr char kTagMark = '$';

    // The tags that share one place in a line, of which the first that has something to show is drawn: one tag,
    // or several written in one pair of braces with a bar between them, as "${sync|bazaar}".
    using TagSlot = std::vector<Tag>;
    constexpr char kSlotBar = '|';

    struct Atom
    {
        TagSlot slot; // empty for text
        std::string text;
        bool operator==(const Atom&) const = default;
    };

    // Text and slots in order; a doubled mark is one mark, and a mark that starts no known tag stays text. A tag is
    // its mark and its name, or its name in braces after the mark where a small letter follows it: "${name}s".
    std::vector<Atom> ParseTemplate(std::string_view text);

    // A line's text as its box shows it: what was typed, as typed, in the stretches before, between and after its
    // slots. One stretch more than slots, empty where two slots touch or one stands at an end.
    struct TemplateRuns
    {
        std::vector<std::string> texts;
        std::vector<TagSlot> slots;
        bool operator==(const TemplateRuns&) const = default;
    };
    TemplateRuns RunsOf(std::string_view text);
    // The text those make. A stretch that ends in a lone mark before a tag has the mark doubled, so that the tag
    // stays one.
    std::string JoinRuns(const TemplateRuns& runs);

    // A place in a line's box: a stretch and a byte in it. Past either end is the end.
    constexpr int kLastRun = -1;
    struct RunCaret
    {
        int run   = kLastRun;
        size_t at = std::string::npos;
        bool operator==(const RunCaret&) const = default;
    };
    const char* TagName(Tag tag);  // "$name", as a line's text holds it
    const char* TagLabel(Tag tag); // "Name", as its chip reads
    const char* TagTip(Tag tag);   // what it renders, for the chip's tooltip
    // The icon a player's status tag draws, which its chip shows too; nothing for a tag that is text or whose icons
    // depend on the mob.
    std::optional<Icon> TagIcon(Tag tag);
    // The chips a kind's page offers, in order.
    std::vector<Tag> TagsFor(PlateKind kind);
    // Puts the slot in at the caret, which then stands after it; false, with nothing changed, when the text would
    // pass kMaxTemplate.
    bool PutTag(std::string& text, RunCaret& caret, const TagSlot& slot);
    // Adds to the slot of this number the tags it does not hold yet; false, with nothing changed, when it holds
    // them all, the text would pass kMaxTemplate or no slot has the number.
    bool JoinSlot(std::string& text, int slot, const TagSlot& more);
    // The text without one tag of a slot, or its last. A slot left with none goes whole, the stretches either side
    // joined and caret where they met. The text as it is for a slot or a tag that is not there.
    constexpr int kLastTag = -1;
    std::string WithoutTag(std::string_view text, int slot, int tag, RunCaret& caret);
    // Puts one tag of a slot in the place of another of it, which moves along with those between: the first of a
    // slot's tags is the first tried. False, with nothing changed, for places that are the same or not there.
    bool MoveTag(std::string& text, int slot, int from, int to);
    // The text with one stretch typed anew, cut to fit kMaxTemplate. What was typed may hold tags of its own.
    std::string WithRun(std::string_view text, int run, std::string_view typed);
    // What a card says while no line of the design holds any of its tags: "Put the Weaknesses or Resistances tag on a
    // line of the design", then the ending, which says what that would show. Empty when a line holds one of them.
    std::string UnusedTagsNote(const KindSettings& kind, std::initializer_list<Tag> tags, const char* ending);
    constexpr const char* kShowsTheseEnding = " to show these on a nameplate."; // a card of settings the tags show
    constexpr const char* kShowsNameEnding  = " to show a name: the game's own is hidden where HeadsUp draws the "
                                              "nameplate.";
    // Whether any line of a design uses the tag.
    bool UsesTag(const KindSettings& kind, Tag tag);
}
