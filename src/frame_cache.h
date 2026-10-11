#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace headsup
{
    // How long a cached texture outlives its last use: ten seconds at 60 frames a second, twenty at 30.
    constexpr uint32_t kIdleFrames = 600;

    // Values shared by key and kept while some frame uses them: a badge's texture serves every plate that shows the same
    // text, and survives a plate leaving the screen for a moment.
    template <typename Key, typename Value>
    class FrameCache
    {
    public:
        // The entry for key, value-initialized when there was none, marked used in this frame.
        Value& Use(const Key& key, uint32_t frame) { return Use<Key>(key, frame); }
        // The same for anything a Key compares with, such as a view of the text a Key owns: it is made into a Key only
        // for a new entry.
        template <typename Probe>
        Value& Use(const Probe& key, uint32_t frame)
        {
            auto found = m_Entries.find(key);
            if (found == m_Entries.end()) found = m_Entries.emplace(Key(key), Entry{}).first;
            found->second.used = frame;
            return found->second.value;
        }

        // Takes out every entry last used more than idleFrames frames ago, for the caller to release.
        std::vector<Value> TakeIdle(uint32_t frame, uint32_t idleFrames)
        {
            std::vector<Value> idle;
            for (auto it = m_Entries.begin(); it != m_Entries.end();)
            {
                if (frame - it->second.used <= idleFrames)
                {
                    ++it;
                    continue;
                }
                idle.push_back(std::move(it->second.value));
                it = m_Entries.erase(it);
            }
            return idle;
        }

        std::vector<Value> TakeAll()
        {
            std::vector<Value> all;
            for (auto& [key, entry] : m_Entries)
                all.push_back(std::move(entry.value));
            m_Entries.clear();
            return all;
        }

        size_t Size() const { return m_Entries.size(); }

    private:
        struct Entry
        {
            Value value{};
            uint32_t used = 0;
        };
        std::map<Key, Entry, std::less<>> m_Entries;
    };
}
