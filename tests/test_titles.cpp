#include "test.h"
#include "titles.h"

#include <string>

using namespace headsup;

TEST(titles_are_named_from_phoenixs_title_list)
{
    CHECK_EQ(std::string(TitleName(1)), std::string("Fodderchief Flayer"));                // FODDERCHIEF_FLAYER
    CHECK_EQ(std::string(TitleName(36)), std::string("Sealer of the Portal of the Gods")); // small words stay small
    CHECK_EQ(std::string(TitleName(206)), std::string("New Adventurer"));
    CHECK(std::string(TitleName(0)).empty() && std::string(TitleName(99999)).empty());
}
