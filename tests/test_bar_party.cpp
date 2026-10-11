#include "bar_party.h"
#include "test.h"

using namespace headsup;

TEST(only_the_jobs_phoenix_grades_with_mp_have_it)
{
    for (const uint8_t job : {3, 4, 5, 7, 8, 15, 16, 20, 21, 22}) // WHM BLM RDM PLD DRK SMN BLU SCH GEO RUN
        CHECK(JobHasMp(job));
    for (const uint8_t job : {0, 1, 2, 6, 9, 10, 11, 12, 13, 14, 17, 18, 19, 23}) // BRD is 10
        CHECK(!JobHasMp(job));
}

TEST(a_member_uses_mp_from_the_main_job_or_a_leveled_subjob)
{
    PartyMember war;
    war.mainJob = 1, war.subJob = 3, war.subLevel = 37; // WAR/WHM
    CHECK(UsesMp(war));
    war.subLevel = 0; // the subjob not unlocked
    CHECK(!UsesMp(war));
    PartyMember blm;
    blm.mainJob = 4;
    CHECK(UsesMp(blm));
}
