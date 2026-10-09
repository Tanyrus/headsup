#include "native_locate.h"
#include "test.h"

#include <cstring>
#include <initializer_list>

using namespace headsup;

namespace
{
    // The client as /hu codedump found it on 2026-10-07: FFXiMain loaded at 0x034C0000, the name routine at RVA 0x086210
    // reading the icon tables at 0x32D434 and 0x32D43C through operands 200 bytes short of each, and called from two
    // places, which the hook confirmed in game (/hu namedump: it returns to RVAs 0x0D08A8 and 0x175711). Every byte
    // below is copied from that dump.
    constexpr uint32_t kClientBase = 0x034C0000;
    constexpr uint32_t kTextRva    = 0x1000;
    constexpr uint32_t kRdataRva   = 0x329000;
    constexpr uint32_t kRoutine    = 0x086210;
    constexpr uint32_t kBaseTable  = 0x32D434;
    constexpr uint32_t kCountTable = 0x32D43C;

    struct Image
    {
        std::vector<uint8_t> text  = std::vector<uint8_t>(0x180000, 0xCC);
        std::vector<uint8_t> rdata = std::vector<uint8_t>(0x26000, 0x00);

        uint8_t* At(uint32_t rva)
        {
            if (rva >= kRdataRva) return rdata.data() + (rva - kRdataRva);
            return text.data() + (rva - kTextRva);
        }
        void Put(uint32_t rva, std::initializer_list<uint8_t> bytes) { std::memcpy(At(rva), bytes.begin(), bytes.size()); }
        void Put32(uint32_t rva, uint32_t value) { std::memcpy(At(rva), &value, sizeof(value)); }
        ImageSection Text() const { return {kTextRva, text.data(), static_cast<uint32_t>(text.size())}; }
        ImageSection Rdata() const { return {kRdataRva, rdata.data(), static_cast<uint32_t>(rdata.size())}; }
    };

    void PutTableReads(Image& image, uint32_t routine, uint32_t baseOperand, uint32_t countOperand)
    {
        image.Put(routine + 0x1F7, {0x8A, 0x90}); // mov dl,[eax+base-200]
        image.Put32(routine + 0x1F9, baseOperand);
        image.Put(routine + 0x1FD, {0x8A, 0x80}); // mov al,[eax+count-200]
        image.Put32(routine + 0x1FF, countOperand);
    }

    Image ClientImage()
    {
        Image image;
        image.Put(kBaseTable, {0xA9, 0xA9, 0xA9, 0xA9, 0xA9, 0xA9});
        image.Put(kCountTable, {0x00, 0x01, 0x02, 0x00, 0x01, 0x02});
        image.Put(kRoutine, {0x81, 0xEC, 0xC4, 0x06, 0x00, 0x00, 0x53, 0x55, 0x56, 0x57});
        image.Put(kRoutine + 0x1D1, {0x33, 0xD2, 0x33, 0xED, 0x33, 0xF6});
        PutTableReads(image, kRoutine, 0x037ED36C, 0x037ED374);
        image.Put(kRoutine + 0x6D7, {0x8D, 0x8C, 0x24, 0x84, 0x00, 0x00, 0x00});
        image.Put(0x0D08A3, {0xE8, 0x68, 0x59, 0xFB, 0xFF, 0x83, 0xC4, 0x14, 0xC2, 0x14, 0x00}); // a wrapper: returns at once
        image.Put(0x17570C, {0xE8, 0xFF, 0x0A, 0xF1, 0xFF, 0x83, 0xC4, 0x14, 0x8B, 0x47, 0x10}); // a loop over world labels
        return image;
    }

    NameRoutine Locate(const Image& image, uint32_t base = kClientBase)
    {
        return LocateNameRoutine(base, image.Text(), image.Rdata());
    }
}

TEST(the_name_routine_is_found_where_the_client_had_it)
{
    const NameRoutine found = Locate(ClientImage());
    CHECK(found.problem == LocateProblem::None);
    CHECK_EQ(found.entry, 0x086210u);
    CHECK_EQ(found.hook, 0x0863E1u);
    CHECK_EQ(found.resume, 0x0863E7u);
    CHECK_EQ(found.exit, 0x0868E7u);
}

TEST(calls_into_the_routine_are_known_by_where_they_return)
{
    Image image = ClientImage();
    image.Put(0x100000, {0xE8, 0x00, 0x00, 0x00, 0x00}); // a call to the next instruction, not to the routine
    const NameRoutine found = Locate(image);
    CHECK(found.problem == LocateProblem::None);
    CHECK((found.callerReturns == std::vector<uint32_t>{0x0D08A8, 0x175711}));
}

TEST(entity_names_come_through_the_wrapper_that_returns_straight_after)
{
    // The other caller walks lists of world labels, whose frames hold no actor.
    const NameRoutine found = Locate(ClientImage());
    CHECK_EQ(found.entityCaller, 0x0D08A8u);
}

TEST(without_that_wrapper_the_routine_is_refused)
{
    Image image = ClientImage();
    image.Put(0x0D08AB, {0xC3}); // ret: no longer the wrapper the entity names come through
    CHECK(Locate(image).problem == LocateProblem::NoEntityCaller);
}

TEST(two_such_wrappers_are_ambiguous)
{
    Image image = ClientImage();
    image.Put(0x17570C, {0xE8, 0xFF, 0x0A, 0xF1, 0xFF, 0x83, 0xC4, 0x14, 0xC2, 0x14, 0x00});
    CHECK(Locate(image).problem == LocateProblem::NoEntityCaller);
}

TEST(an_image_loaded_elsewhere_reads_the_tables_through_other_operands)
{
    Image image = ClientImage();
    PutTableReads(image, kRoutine, 0x1032D36C, 0x1032D374); // as loaded at the DLL's preferred base, 0x10000000
    CHECK_EQ(Locate(image, 0x10000000).entry, 0x086210u);
    CHECK(Locate(image, kClientBase).problem == LocateProblem::NoTableReads);
}

TEST(without_the_icon_tables_there_is_nothing_to_find_the_routine_by)
{
    Image image = ClientImage();
    image.Put(kCountTable, {0x00, 0x01, 0x02, 0x00, 0x01, 0x03});
    CHECK(Locate(image).problem == LocateProblem::NoIconTables);
}

TEST(icon_tables_not_eight_bytes_apart_are_not_the_pair)
{
    Image image = ClientImage();
    image.Put(kCountTable, {0, 0, 0, 0, 0, 0});
    image.Put(kCountTable + 2, {0x00, 0x01, 0x02, 0x00, 0x01, 0x02});
    PutTableReads(image, kRoutine, 0x037ED36C, 0x037ED376);
    CHECK(Locate(image).problem == LocateProblem::NoIconTables);
}

TEST(a_second_pair_of_table_reads_is_ambiguous)
{
    Image image = ClientImage();
    PutTableReads(image, 0x120000, 0x037ED36C, 0x037ED374);
    CHECK(Locate(image).problem == LocateProblem::AmbiguousTableReads);
}

TEST(table_reads_with_no_room_for_the_routine_around_them_are_not_counted)
{
    Image image = ClientImage();
    image.Put32(kTextRva + 0x10, 0x037ED36C); // the entry would be before .text
    image.Put32(kTextRva + 0x16, 0x037ED374);
    const uint32_t end = kTextRva + static_cast<uint32_t>(image.text.size());
    image.Put32(end - 0x20, 0x037ED36C); // the exit would be past it
    image.Put32(end - 0x1A, 0x037ED374);
    CHECK_EQ(Locate(image).entry, 0x086210u);
}

TEST(a_routine_that_does_not_start_as_the_clients_did_is_refused)
{
    Image image = ClientImage();
    image.Put(kRoutine + 2, {0xC8}); // sub esp,0x6C8: a different frame, so different offsets
    CHECK(Locate(image).problem == LocateProblem::UnexpectedPrologue);
}

TEST(a_patch_site_another_plugin_changed_is_refused)
{
    Image image = ClientImage();
    image.Put(kRoutine + 0x1D1, {0xE9, 0x10, 0x20, 0x30, 0x40, 0x90}); // jmp rel32; nop
    CHECK(Locate(image).problem == LocateProblem::PatchSiteChanged);
}

TEST(a_routine_without_the_clients_exit_is_refused)
{
    Image image = ClientImage();
    image.Put(kRoutine + 0x6D7, {0x8D, 0x8C, 0x24, 0x88});
    CHECK(Locate(image).problem == LocateProblem::UnexpectedExit);
}

TEST(a_routine_nobody_calls_is_refused)
{
    Image image = ClientImage();
    image.Put(0x0D08A3, {0x90});
    image.Put(0x17570C, {0x90});
    CHECK(Locate(image).problem == LocateProblem::NoCallers);
}

namespace
{
    // The end of the target window's draw (RVA 0x1511A0) in the same dump: the arrow over the target at (m_AnkX, m_AnkY)
    // in gray E0808080, then while a sub-target is picked the arrow over the candidate at (m_SubAnkX, m_SubAnkY) in the
    // range color, each through the shape draw at 0x120C80 (it ends in ret 20h).
    constexpr uint32_t kTargetArrow = 0x15151C;
    constexpr uint32_t kPickedArrow = 0x151564;
    constexpr uint32_t kShapeDraw   = 0x120C80;

    Image ArrowClient()
    {
        Image image;
        image.Put(kTargetArrow, {0x66, 0x8B, 0x86, 0xBC, 0x00, 0x00, 0x00, 0x66, 0x85, 0xC0, 0x74, 0x32, 0x0F, 0xBF, 0x96, 0xBE,
                                    0x00, 0x00, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0x33, 0xC9, 0x8A, 0x8E, 0xBA, 0x00, 0x00,
                                    0x00, 0x68, 0x80, 0x80, 0x80, 0xE0, 0x0F, 0xBF, 0xC0, 0x8B, 0x4C, 0x8E, 0x78, 0x68, 0x00, 0x00,
                                    0x80, 0x3F, 0x68, 0x00, 0x00, 0x80, 0x3F, 0x52, 0x50, 0xE8, 0x26, 0xF7, 0xFC, 0xFF});
        image.Put(kPickedArrow, {0x0F, 0xBF, 0x96, 0xC2, 0x00, 0x00, 0x00, 0x0F, 0xBF, 0x86, 0xC0, 0x00, 0x00, 0x00, 0x6A, 0x00,
                                    0x6A, 0x00, 0x6A, 0x00, 0x33, 0xC9, 0x8A, 0x8E, 0xBA, 0x00, 0x00, 0x00, 0x57, 0x68, 0x00, 0x00,
                                    0x80, 0x3F, 0x68, 0x00, 0x00, 0x80, 0x3F, 0x8B, 0x4C, 0x8E, 0x78, 0x52, 0x50, 0xE8, 0xEA, 0xF6,
                                    0xFC, 0xFF});
        return image;
    }
}

TEST(the_target_arrows_are_two_calls_into_one_shape_draw)
{
    const ArrowCalls calls = LocateArrowCalls(ArrowClient().Text());
    CHECK(calls.problem == LocateProblem::None);
    CHECK_EQ(calls.target, kTargetArrow + 0x39); // the E8 after its pushes
    CHECK_EQ(calls.picked, kPickedArrow + 0x2D);
    CHECK_EQ(calls.draw, kShapeDraw);
}

TEST(a_client_without_the_arrow_draws_is_left_alone)
{
    Image image = ArrowClient();
    image.Put(kPickedArrow + 0x1C, {0x56}); // push esi in place of push edi: another color
    CHECK(LocateArrowCalls(image.Text()).problem == LocateProblem::NoArrowDraws);
    CHECK(LocateArrowCalls(Image{}.Text()).problem == LocateProblem::NoArrowDraws);
}

TEST(a_second_copy_of_an_arrow_draw_is_ambiguous)
{
    Image image = ArrowClient();
    const uint8_t* from = image.At(kTargetArrow);
    std::memcpy(image.At(0x100000), from, 0x3E);
    CHECK(LocateArrowCalls(image.Text()).problem == LocateProblem::AmbiguousArrowDraws);
}

TEST(arrow_draws_into_different_functions_are_left_alone)
{
    Image image = ArrowClient();
    image.Put32(kPickedArrow + 0x2E, 0xFFFCF6F0); // six bytes past the shape draw
    CHECK(LocateArrowCalls(image.Text()).problem == LocateProblem::ArrowDrawsDiffer);
}

namespace
{
    // Where the same dump guesses the client area from GetWindowRect and the system's border sizes: reading a mouse
    // message's position, moving Windows' pointer to the game's, and panning at the pointer area's edge. Each loads
    // GetSystemMetrics into a different register.
    constexpr uint32_t kMessageGuess = 0x002917, kWarpGuess = 0x1257E1, kEdgeGuess = 0x125C15;

    Image PointerClient()
    {
        Image image;
        image.Put(kMessageGuess, {0x8B, 0x48, 0x18, 0x51, 0xFF, 0x15, 0xE4, 0x93, 0x7E, 0x03, 0x8B, 0x2D, 0xA0, 0x93, 0x7E, 0x03,
                                     0x6A, 0x20, 0xFF, 0xD5, 0x6A, 0x21});
        image.Put(kWarpGuess, {0xD9, 0x5C, 0x24, 0x20, 0xFF, 0x15, 0xE4, 0x93, 0x7E, 0x03, 0x8B, 0x35, 0xA0, 0x93, 0x7E, 0x03, 0x6A,
                                  0x20, 0xFF, 0xD6, 0x8B, 0xF8, 0x6A, 0x21});
        image.Put(kEdgeGuess, {0x8B, 0x51, 0x18, 0x52, 0xFF, 0x15, 0xE4, 0x93, 0x7E, 0x03, 0x8B, 0x1D, 0xA0, 0x93, 0x7E, 0x03, 0x6A,
                                  0x20, 0xFF, 0xD3, 0x6A, 0x21});
        return image;
    }
}

TEST(the_games_three_guesses_at_its_client_area_are_found)
{
    const PointerMapping mapping = LocatePointerMapping(PointerClient().Text());
    CHECK(mapping.problem == LocateProblem::None);
    CHECK_EQ(mapping.guesses.size(), size_t{3});
    if (mapping.guesses.size() != 3) return;
    CHECK(mapping.guesses[0].windowRectCall == kMessageGuess + 4 && mapping.guesses[0].metricsLoad == kMessageGuess + 10);
    CHECK(mapping.guesses[1].windowRectCall == kWarpGuess + 4 && mapping.guesses[1].metricsLoad == kWarpGuess + 10);
    CHECK(mapping.guesses[2].windowRectCall == kEdgeGuess + 4 && mapping.guesses[2].metricsLoad == kEdgeGuess + 10);
    // mov ebp, imm32; mov esi, imm32; mov ebx, imm32: the registers they load GetSystemMetrics into
    CHECK(mapping.guesses[0].movImmediate == 0xBD && mapping.guesses[1].movImmediate == 0xBE && mapping.guesses[2].movImmediate == 0xBB);
}

TEST(a_client_missing_a_guess_or_holding_two_is_left_alone)
{
    Image missing = PointerClient();
    missing.Put(kEdgeGuess + 0x13, {0xD7}); // call edi
    CHECK(LocatePointerMapping(missing.Text()).problem == LocateProblem::NoPointerMapping);
    Image twice = PointerClient();
    std::memcpy(twice.At(0x100000), twice.At(kMessageGuess), 0x16);
    CHECK(LocatePointerMapping(twice.Text()).problem == LocateProblem::AmbiguousPointerMapping);
}

TEST(guesses_through_different_imports_are_left_alone)
{
    Image image = PointerClient();
    image.Put32(kWarpGuess + 12, 0x037E93A4); // the import after GetSystemMetrics
    CHECK(LocatePointerMapping(image.Text()).problem == LocateProblem::PointerMappingDiffers);
}

namespace
{
    // The same dump's mouse-move handling (0x2A6A): it moves the mouse controller's pointer (global at 0x039A1D4C),
    // then calls the controller's routine at 0x1262F0 with 1, which shows the pointer and keeps that in its byte +0x4E.
    constexpr uint32_t kMoveCall = 0x002A6A, kShowPointer = 0x1262F0;

    Image ShowClient()
    {
        Image image;
        image.Put(kMoveCall, {0x8D, 0x4C, 0x24, 0x14, 0x8D, 0x54, 0x24, 0x24, 0x51, 0x8B, 0x0D, 0x4C, 0x1D, 0x9A, 0x03, 0x53, 0x52, 0xE8,
                                 0x50, 0x2B, 0x12, 0x00, 0x8B, 0x0D, 0x4C, 0x1D, 0x9A, 0x03, 0x6A, 0x01, 0xE8, 0x63, 0x38, 0x12, 0x00});
        image.Put(kShowPointer, {0xA1, 0xF8, 0x6B, 0x91, 0x03, 0x56, 0x8B, 0xF1, 0x8B, 0x0D, 0x6C, 0x66, 0x91, 0x03, 0x85, 0xC0, 0x74,
                                    0x3B, 0x53, 0x8B, 0x5C, 0x24, 0x0C, 0x85, 0xC9, 0x74, 0x0C, 0x53, 0xE8, 0x6F, 0x44, 0xEE, 0xFF, 0x8B,
                                    0x0D, 0x6C, 0x66, 0x91, 0x03, 0x38, 0x5E, 0x4E});
        return image;
    }
}

TEST(the_games_show_pointer_routine_is_found_through_its_mouse_move)
{
    const PointerShow show = LocatePointerShow(ShowClient().Text());
    CHECK(show.problem == LocateProblem::None);
    CHECK_EQ(show.controllerGlobal, 0x039A1D4Cu);
    CHECK_EQ(show.show, kShowPointer);
}

TEST(a_show_pointer_routine_that_keeps_its_state_elsewhere_is_left_alone)
{
    Image moved = ShowClient();
    moved.Put(kShowPointer + 0x29, {0x4F}); // cmp bl,[esi+4Fh]
    CHECK(LocatePointerShow(moved.Text()).problem == LocateProblem::PointerShowChanged);
    CHECK(LocatePointerShow(Image{}.Text()).problem == LocateProblem::NoPointerShow);
    Image other = ShowClient();
    other.Put32(kMoveCall + 24, 0x039A1D50); // the show called on another object than the one moved
    CHECK(LocatePointerShow(other.Text()).problem == LocateProblem::PointerShowChanged);
}
