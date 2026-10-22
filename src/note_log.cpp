#include "note_log.h"

namespace headsup
{
    std::optional<std::string> NoteLog::Line(const std::string& note, const std::string& stamp)
    {
        if (const auto said = m_Again.find(note); said != m_Again.end())
        {
            ++said->second;
            return std::nullopt;
        }
        m_Again.emplace(note, 0);
        m_Written.push_back(note);
        return "[" + stamp + "] " + note;
    }

    std::vector<std::string> NoteLog::Repeats() const
    {
        std::vector<std::string> lines;
        for (const std::string& note : m_Written)
            if (const int again = m_Again.at(note); again > 0)
                lines.push_back(note + " (" + std::to_string(again) + (again == 1 ? " more time)" : " more times)"));
        return lines;
    }
}
