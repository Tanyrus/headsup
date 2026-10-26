#include "note_log.h"
#include "test.h"

using namespace headsup;

TEST(a_note_is_written_once_with_its_time)
{
    NoteLog log;
    CHECK(log.Line("RmlUi: could not animate", "13:17:50") == "[13:17:50] RmlUi: could not animate");
    CHECK(!log.Line("RmlUi: could not animate", "13:17:59"));
    CHECK(log.Line("the weather widget guesses", "13:18:00") == "[13:18:00] the weather widget guesses");
}

TEST(notes_said_again_are_counted_in_the_order_they_were_first_written)
{
    NoteLog log;
    for (const char* note : {"top", "right", "top", "top", "right", "once"}) log.Line(note, "13:17:50");
    const std::vector<std::string> repeats = log.Repeats();
    CHECK(repeats.size() == 2);
    CHECK(repeats[0] == "top (2 more times)");
    CHECK(repeats[1] == "right (1 more time)");
}
