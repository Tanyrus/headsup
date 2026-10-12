#include "ini_text.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <vector>

namespace headsup
{
    namespace
    {
        constexpr std::string_view kCrLf = "\r\n", kLf = "\n", kBlanks = " \t", kByteOrderMark = "\xEF\xBB\xBF";
        constexpr std::string_view kNotInKeys = "\r\n="; // a line break ends a key's line and an = ends the key
        constexpr char kAssign = '=', kSectionOpen = '[', kSectionClose = ']', kComment = ';', kSpace = ' ';

        std::string_view Trimmed(std::string_view text)
        {
            const size_t first = text.find_first_not_of(kBlanks);
            if (first == std::string_view::npos) return {};
            return text.substr(first, text.find_last_not_of(kBlanks) - first + 1);
        }

        std::string_view EndingOf(std::string_view file)
        {
            const size_t lf = file.find(kLf);
            return lf != std::string_view::npos && !file.substr(0, lf + 1).ends_with(kCrLf) ? kLf : kCrLf;
        }

        // A section's or key's name as the reader compares it: without case.
        std::string Name(std::string_view written)
        {
            std::string name(written);
            for (char& c : name)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return name;
        }

        bool IsKey(std::string_view key)
        {
            return !key.empty() && key.find_first_of(kNotInKeys) == std::string_view::npos && key.front() != kSectionOpen &&
                   key.front() != kComment;
        }
    }

    std::string WithIniValues(std::string_view file, std::string_view section, std::span<const KeyText> values)
    {
        std::vector<KeyText> wanted; // each key once, where it was first given, with its last text
        std::unordered_map<std::string, size_t> wantedAt;
        for (const KeyText& value : values)
        {
            if (!IsKey(value.key)) continue;
            const auto [at, added] = wantedAt.try_emplace(Name(value.key), wanted.size());
            if (added) wanted.push_back(value);
            wanted[at->second].text = value.text;
            // A line break in a text would start a line of its own.
            std::ranges::replace_if(
                wanted[at->second].text, [](char c) { return kCrLf.find(c) != std::string_view::npos; }, kSpace);
        }
        const std::string sectionName = Name(section);
        const std::string_view ending = EndingOf(file);
        std::vector<bool> placed(wanted.size(), false);
        std::string out;
        out.reserve(file.size());
        // The reader reads the first section of the name and no other.
        enum class Section { NotYet, Inside, Past } where = Section::NotYet;
        size_t sectionEnd = std::string::npos; // in out: past the section's last line that is not blank
        for (bool first = true; !file.empty(); first = false)
        {
            const size_t lf            = file.find(kLf);
            const std::string_view all = file.substr(0, lf == std::string_view::npos ? file.size() : lf + 1);
            file.remove_prefix(all.size());
            std::string_view line = all;
            for (const std::string_view end : {kCrLf, kLf})
                if (line.size() == all.size() && line.ends_with(end)) line.remove_suffix(end.size());
            const std::string_view trimmed =
                Trimmed(first && line.starts_with(kByteOrderMark) ? line.substr(kByteOrderMark.size()) : line);
            // A header runs to its line's last bracket, whatever follows; a line that only opens one ends a section too.
            const bool header = trimmed.starts_with(kSectionOpen);
            if (header)
            {
                const size_t close = trimmed.rfind(kSectionClose);
                const bool named   = close != std::string_view::npos && Name(Trimmed(trimmed.substr(1, close - 1))) == sectionName;
                where = where == Section::NotYet ? (named ? Section::Inside : Section::NotYet) : Section::Past;
            }
            const size_t assign = line.find(kAssign);
            const auto value    = where == Section::Inside && !header && assign != std::string_view::npos
                                      ? wantedAt.find(Name(Trimmed(line.substr(0, assign))))
                                      : wantedAt.end();
            if (value != wantedAt.end())
            {
                out += line.substr(0, assign + 1);
                out += wanted[value->second].text;
                placed[value->second] = true;
            }
            else
                out += line;
            out += line.size() == all.size() ? ending : all.substr(line.size());
            if (where == Section::Inside && !trimmed.empty()) sectionEnd = out.size();
        }
        std::string added;
        for (size_t i = 0; i < wanted.size(); ++i)
            if (!placed[i]) added += wanted[i].key + kAssign + wanted[i].text + std::string(ending);
        if (added.empty()) return out;
        if (sectionEnd == std::string::npos)
        {
            out += kSectionOpen;
            out += section;
            out += kSectionClose;
            out += ending;
            sectionEnd = out.size();
        }
        out.insert(sectionEnd, added);
        return out;
    }

    bool HoldsIniValues(std::string_view file, std::string_view section, std::span<const KeyText> values)
    {
        return WithIniValues(file, section, values) == file;
    }
}
