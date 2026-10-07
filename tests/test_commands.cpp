#include "commands.h"
#include "test.h"

using namespace headsup;

TEST(a_command_splits_into_lower_case_words)
{
    CHECK((SplitLower("/HU  DrawDump\t2") == std::vector<std::string>{"/hu", "drawdump", "2"}));
    CHECK((SplitLower("  /hu  ") == std::vector<std::string>{"/hu"}));
    CHECK(SplitLower("").empty());
    CHECK(SplitLower(" \t ").empty());
}
