#include "memo.h"
#include "test.h"

#include <string>

using namespace headsup;

namespace
{
    struct Key
    {
        std::string text;
        int size = 0;
        bool operator==(const Key&) const = default;
    };
}

TEST(a_memo_makes_its_value_again_only_when_its_key_changes)
{
    Memo<Key, std::string> memo;
    int made  = 0;
    auto make = [&](const Key& key) {
        ++made;
        return key.text + std::to_string(key.size);
    };
    CHECK_EQ(memo.Get(Key{"a", 1}, make), std::string("a1"));
    CHECK_EQ(memo.Get(Key{"a", 1}, make), std::string("a1"));
    CHECK_EQ(made, 1);
    CHECK_EQ(memo.Get(Key{"a", 2}, make), std::string("a2")); // any field of the key
    CHECK_EQ(memo.Get(Key{"b", 2}, make), std::string("b2"));
    CHECK_EQ(made, 3);
}

TEST(a_memos_first_value_is_made_even_for_a_default_key)
{
    Memo<Key, int> memo;
    CHECK_EQ(memo.Get(Key{}, [](const Key&) { return 7; }), 7);
}
