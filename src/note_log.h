#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // The lines of HeadsUp's log: a note said before is counted, not written again, so one said every frame cannot
    // fill the file.
    class NoteLog
    {
    public:
        // The line to write for a note, stamped with the time; none for a note already written.
        std::optional<std::string> Line(const std::string& note, const std::string& stamp);
        // A line for each note said again after it was written, with how many more times, in the order written.
        std::vector<std::string> Repeats() const;

    private:
        std::vector<std::string> m_Written;
        std::unordered_map<std::string, int> m_Again;
    };
}
