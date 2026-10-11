#include "function_ref.h"
#include "test.h"

#include <string>

using namespace headsup;

namespace
{
    std::string Levels(int level) { return "+" + std::to_string(level); }
    std::string Twice(FunctionRef<std::string(int)> label, int level) { return label(level) + label(level + 1); }
}

TEST(a_function_ref_calls_the_callable_it_was_given)
{
    const std::string prefix = "Lv ";
    int calls                = 0;
    auto label               = [&](int level) {
        ++calls;
        return prefix + std::to_string(level);
    };
    CHECK_EQ(Twice(label, 20), std::string("Lv 20Lv 21"));
    CHECK_EQ(calls, 2);
    CHECK_EQ(Twice([](int level) { return std::to_string(level * 2); }, 3), std::string("68"));
    CHECK_EQ(Twice(Levels, 1), std::string("+1+2"));
}
