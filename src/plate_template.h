#pragma once

#include "plate_settings.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    enum class Tag : uint8_t
    {
        Name, Level, Con, Id, ServerId, Ph, Detect, Weak, Resist, Status, Immune, Gm, Mentor, New, Sync, Away, Linkshell,
        Bazaar, Seeking, Job, Owner
    };
    constexpr int kTagCount = static_cast<int>(Tag::Owner) + 1;
    constexpr char kTagMark = '$';

    struct Atom
    {
        bool isTag = false;
        Tag tag    = Tag::Name;
        std::string text;
        bool operator==(const Atom&) const = default;
    };

    // Text and tags in order; a doubled mark is one mark, and a mark that starts no known tag stays text.
    std::vector<Atom> ParseTemplate(std::string_view text);
    const char* TagName(Tag tag); // "$name"
    const char* TagTip(Tag tag);  // what it renders, for the chip's tooltip
    // The chips a kind's page offers, in order.
    std::vector<Tag> TagsFor(PlateKind kind);
    // The text with the tag's name put in at byte caret, clamped to the text's end; unchanged when it would pass kMaxTemplate.
    std::string InsertTag(std::string text, Tag tag, size_t caret);
    // InsertTag at the caret, which then sits past the tag; false, with nothing changed, when the insert is refused.
    bool PutTag(std::string& text, size_t& caret, Tag tag);
    // What a card says while no line of the design holds any of its tags: "Put $weak or $resist on a line of the
    // design", then the ending, which says what that would show. Empty when a line holds one of them.
    std::string UnusedTagsNote(const KindSettings& kind, std::initializer_list<Tag> tags, const char* ending);
    constexpr const char* kShowsTheseEnding = " to show these on a nameplate."; // a card of settings the tags show
    constexpr const char* kShowsNameEnding  = " to show a name: the game's own is hidden where HeadsUp draws the "
                                              "nameplate.";
    // Whether any line of a design uses the tag.
    bool UsesTag(const KindSettings& kind, Tag tag);
}
