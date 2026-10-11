#pragma once

#include <optional>
#include <utility>

namespace headsup
{
    // The value made for the last key, kept while the key stays equal: work only a change of its inputs can change.
    // Keys compare with ==, so every input the value depends on belongs in the key.
    template <typename Key, typename Value>
    class Memo
    {
    public:
        // The value for key, made by make(key) unless the last call's key was equal.
        template <typename Make>
        const Value& Get(Key key, const Make& make)
        {
            if (!m_Value || !(m_Key == key))
            {
                m_Value.emplace(make(std::as_const(key)));
                m_Key = std::move(key);
            }
            return *m_Value;
        }

    private:
        Key m_Key{};
        std::optional<Value> m_Value;
    };
}
