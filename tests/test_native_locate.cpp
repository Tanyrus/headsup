#include "code_patch.h"
#include "native_locate.h"
#include "test.h"

#include <algorithm>
#include <climits>

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
        std::vector<uint8_t> text  = std::vector<uint8_t>(0x200000, 0xCC);
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
    // Where the same draw asks for the color of the candidate's arrow: it pushes the two colors that are not the
    // usual one, then that one from edi, the candidate and you, and calls the routine at 0x1515A0 on the window.
    constexpr uint32_t kRangeColorAsk = 0x1514D3;
    constexpr uint32_t kRangeColor    = 0x1515A0;

    Image RangeClient()
    {
        Image image;
        image.Put(kRangeColorAsk, {0x68, 0x40, 0xD0, 0xD0, 0xE0, 0x68, 0x40, 0x40, 0xD0, 0xE0, 0x57, 0x52, 0x50, 0x8B, 0xCE, 0xE8,
                                      0xB9, 0x00, 0x00, 0x00});
        return image;
    }
}

TEST(the_candidate_arrows_color_comes_from_one_routine)
{
    const RangeColor found = LocateRangeColor(RangeClient().Text());
    CHECK(found.problem == LocateProblem::None);
    CHECK_EQ(found.routine, kRangeColor);
}

TEST(a_client_that_asks_for_the_arrows_color_another_way_has_no_routine)
{
    Image image = RangeClient();
    image.Put(kRangeColorAsk + 1, {0x41}); // another color
    CHECK(LocateRangeColor(image.Text()).problem == LocateProblem::NoRangeColor);
    CHECK(LocateRangeColor(Image{}.Text()).problem == LocateProblem::NoRangeColor);
    Image twice = RangeClient();
    std::memcpy(twice.At(0x100000), twice.At(kRangeColorAsk), 0x14);
    CHECK(LocateRangeColor(twice.Text()).problem == LocateProblem::AmbiguousRangeColor);
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

namespace
{
    // The end of the same dump's command check (0x124D5F): mouse use on (1) for a few commands, off (0) for the rest,
    // each through the routine at 0x126F70 on the mouse controller.
    constexpr uint32_t kMouseUseCalls = 0x124D5F, kMouseUse = 0x126F70;

    Image KeyHideClient()
    {
        Image image;
        image.Put(kMouseUseCalls, {0x8B, 0x0D, 0x4C, 0x1D, 0x9A, 0x03, 0x6A, 0x01, 0xE8, 0x04, 0x22, 0x00, 0x00, 0x8A, 0x44, 0x24, 0x14,
                                      0x5F, 0x5E, 0x59, 0xC3, 0x8B, 0x0D, 0x4C, 0x1D, 0x9A, 0x03, 0x6A, 0x00, 0xE8, 0xEF, 0x21, 0x00,
                                      0x00, 0x8A, 0x44, 0x24, 0x14, 0x5F, 0x5E, 0x59, 0xC3});
        return image;
    }
}

TEST(the_hide_after_a_keys_command_is_the_second_of_two_mouse_use_calls)
{
    const KeyHide hide = LocateKeyHide(KeyHideClient().Text());
    CHECK(hide.problem == LocateProblem::None);
    CHECK_EQ(hide.offCall, kMouseUseCalls + 0x1D);
    CHECK_EQ(hide.mouseUse, kMouseUse);
}

TEST(mouse_use_calls_into_different_routines_are_left_alone)
{
    Image image = KeyHideClient();
    image.Put32(kMouseUseCalls + 0x1E, 0x000021F5); // six bytes past the routine
    CHECK(LocateKeyHide(image.Text()).problem == LocateProblem::KeyHideDiffers);
    CHECK(LocateKeyHide(Image{}.Text()).problem == LocateProblem::NoKeyHide);
}

namespace
{
    // The Phoenix client's draw loop (from RVA 0x15F727, its call at 0x15F729) and the window draw it calls (RVA
    // 0x1177D0), from the facts.
    constexpr uint32_t kWindowLoop = 0x15F727, kWindowRoutine = 0x1177D0;
    const std::initializer_list<uint8_t> kLoopTail = {
        0x8B, 0x44, 0x24, 0x0C, 0x85, 0xC0, 0x75, 0xC6, 0xFE, 0xC3, 0x80, 0xFB, 0x09, 0x7C, 0xAD};

    void PutWindowLoop(Image& image, uint32_t at)
    {
        image.Put(at, {0x8B, 0xCE, 0xE8});
        image.Put32(at + 3, kWindowRoutine - (at + 2 + 5));
        image.Put(at + 7, kLoopTail);
    }

    // Before the loop, its list: xor bl,bl; mov ecx,list; call First, as the client of 2026-10-13 held it at
    // 0x15F6E8 with its list at 0x03AE1838.
    constexpr uint32_t kWindowListMove = 0x15F6E8, kWindowListAt = 0x03AE1838;

    Image WindowClient()
    {
        Image image;
        PutWindowLoop(image, kWindowLoop);
        image.Put(kWindowRoutine, {0x83, 0xEC, 0x24, 0x53, 0x55, 0x56, 0x57, 0x8B, 0xE9, 0xE8});
        image.Put(kWindowListMove, {0x32, 0xDB, 0xB9, 0x38, 0x18, 0xAE, 0x03, 0xE8, 0x0C, 0x6E, 0xFC, 0xFF});
        return image;
    }
}

TEST(the_window_draw_call_is_found_by_its_loop_and_checked_by_the_routines_start)
{
    const WindowDraw found = LocateWindowDraw(WindowClient().Text());
    CHECK(found.problem == LocateProblem::None && found.call == kWindowLoop + 2 && found.draw == kWindowRoutine);
    Image changed = WindowClient();
    changed.Put(kWindowRoutine, {0x55});
    CHECK(LocateWindowDraw(changed.Text()).problem == LocateProblem::WindowDrawChanged);
    Image twice = WindowClient();
    PutWindowLoop(twice, 0x160000);
    CHECK(LocateWindowDraw(twice.Text()).problem == LocateProblem::AmbiguousWindowDraw);
    CHECK(LocateWindowDraw(Image{}.Text()).problem == LocateProblem::NoWindowDraw);
}

TEST(the_window_list_is_the_one_the_draw_loop_starts_on)
{
    CHECK(LocateWindowDraw(WindowClient().Text()).list == kWindowListAt);
    Image otherRegister = WindowClient();
    otherRegister.Put(kWindowListMove + 2, {0xBA}); // mov edx: not the list the loop's call is given
    const WindowDraw without = LocateWindowDraw(otherRegister.Text());
    CHECK(without.problem == LocateProblem::None && without.list == 0);
    Image farBack = WindowClient();
    farBack.Put(kWindowListMove, {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90});
    farBack.Put(kWindowListMove - 0x40, {0x32, 0xDB, 0xB9, 0x38, 0x18, 0xAE, 0x03, 0xE8, 0x0C, 0x6E, 0xFC, 0xFF});
    CHECK(LocateWindowDraw(farBack.Text()).list == 0);
}

TEST(a_window_draw_call_that_leaves_the_code_is_a_changed_client)
{
    const uint32_t next = kWindowLoop + 7; // the call's rel32 counts from the instruction after it
    for (const uint32_t target : {0x0800u, kTextRva + 0x180000u - 4u, 0x7FFFFFF0u})
    {
        Image image = WindowClient();
        image.Put32(kWindowLoop + 3, target - next);
        CHECK(LocateWindowDraw(image.Text()).problem == LocateProblem::WindowDrawChanged);
    }
}

namespace
{
    // The window names as the client's code dump of 2026-10-07 holds them, each 16 bytes.
    constexpr const char* kHelpBarNames[] = {"menu    helpwind", "menu    titlewin"};
    constexpr const char* kNetstatName    = "menu    netstat ";
    constexpr HiddenWindows kNone{}, kHelpBar{.helpBar = true}, kNetstat{.netstat = true},
        kBoth{.helpBar = true, .netstat = true};
}

TEST(the_title_and_help_windows_are_skipped_only_while_the_help_bar_is_hidden)
{
    for (const char* name : kHelpBarNames)
    {
        CHECK(SkipsWindow(name, kHelpBar) && SkipsWindow(name, kBoth));
        CHECK(!SkipsWindow(name, kNone) && !SkipsWindow(name, kNetstat));
    }
    CHECK(!SkipsWindow("menu    helpwinD", kBoth) && !SkipsWindow("menu    titlewiN", kBoth)); // all 16 bytes count
}

TEST(the_connection_readout_is_skipped_only_while_it_is_hidden)
{
    CHECK(SkipsWindow(kNetstatName, kNetstat) && SkipsWindow(kNetstatName, kBoth));
    CHECK(!SkipsWindow(kNetstatName, kNone) && !SkipsWindow(kNetstatName, kHelpBar));
    // Its name ends in a space: the same letters ended any other way are another window.
    CHECK(!SkipsWindow("menu    netstat", kBoth) && !SkipsWindow("menu    netstatX", kBoth));
}

TEST(no_other_window_and_no_name_is_skipped)
{
    CHECK(!SkipsWindow("menu    inventor", kBoth));
    CHECK(!SkipsWindow(nullptr, kBoth));
}

TEST(the_hook_is_wanted_while_either_window_is_hidden)
{
    CHECK(kHelpBar.Any() && kNetstat.Any() && kBoth.Any());
    CHECK(!kNone.Any());
}

namespace
{
    // The client of 2026-10-09 (the research's dump, loaded at kClientBase): the top strip's catalog records, its two
    // SetText entries, the shared SetText and the level picker.
    constexpr uint32_t kStripTextRva = 0x130000, kStripDataRva = 0x370000;

    struct StripImage
    {
        std::vector<uint8_t> text = std::vector<uint8_t>(0x100000, 0xCC);
        std::vector<uint8_t> data = std::vector<uint8_t>(0x10000, 0x00);
        void Code(uint32_t rva, std::initializer_list<uint8_t> bytes)
        {
            std::memcpy(text.data() + (rva - kStripTextRva), bytes.begin(), bytes.size());
        }
        void Data(uint32_t rva, const void* bytes, size_t size)
        {
            std::memcpy(data.data() + (rva - kStripDataRva), bytes, size);
        }
        void Record(uint32_t rva, std::initializer_list<uint8_t> head, const char* name, uint32_t slot)
        {
            Data(rva, head.begin(), head.size());
            Data(rva + 8, name, 16);
            Data(rva + 0x28, &slot, sizeof(slot));
        }
        ImageSection Text() const { return {kStripTextRva, text.data(), static_cast<uint32_t>(text.size())}; }
        ImageSection Dat() const { return {kStripDataRva, data.data(), static_cast<uint32_t>(data.size())}; }
    };

    StripImage StripClient()
    {
        StripImage image;
        image.Record(0x3727CC, {0x2B, 0x00, 0x01, 0x00, 0x35, 0x07, 0x00, 0x00}, "menu    helpwind",
            kClientBase + 0x5781F8);
        image.Record(0x3727F8, {0x93, 0x80, 0x00, 0x00, 0x05, 0x02, 0x00, 0x00}, "menu    titlewin",
            kClientBase + 0x57847C);
        image.Code(0x13A320, {0x83, 0xC1, 0x20, 0xE9, 0x08, 0x6D, 0x0E, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                                 0x90, 0x83, 0xC1, 0x20, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x0D});
        image.Code(0x150CD0, {0x81, 0xC1, 0xA4, 0x00, 0x00, 0x00, 0xE9, 0x55, 0x03, 0x0D, 0x00, 0x90, 0x90, 0x90, 0x90,
                                 0x90, 0x8B, 0x44, 0x24, 0x04, 0x81, 0xEC, 0xCC, 0x00, 0x00, 0x00});
        image.Code(0x221030, {0x51, 0x8B, 0x44, 0x24, 0x08, 0x53, 0x55, 0x56, 0x85, 0xC0, 0x57, 0x8B, 0xE9, 0x75, 0x14});
        image.Code(0x221680, {0xA1, 0xFC, 0x81, 0xA3, 0x03, 0x53, 0x55, 0x56, 0x8B, 0xF1, 0x8A, 0x48, 0x18, 0x33, 0xDB,
                                 0x57, 0x3A, 0xCB, 0x74, 0x4E, 0x8D, 0xBE, 0xD5, 0x02, 0x00, 0x00});
        return image;
    }
}

TEST(the_top_strips_text_is_found_through_the_menu_catalog_and_its_setters)
{
    const StripImage image = StripClient();
    const StripSites found = LocateStripText(image.Text(), image.Dat());
    CHECK(found.problem == LocateProblem::None);
    CHECK_EQ(found.helpSlot, kClientBase + 0x5781F8u);
    CHECK_EQ(found.titleSlot, kClientBase + 0x57847Cu);
    CHECK(found.helpText == 0x20u && found.titleText == 0xA4u);
}

TEST(a_client_whose_strip_is_not_as_found_is_left_alone)
{
    StripImage noRecord = StripClient();
    noRecord.Data(0x3727CC, "x", 1);
    CHECK(LocateStripText(noRecord.Text(), noRecord.Dat()).problem == LocateProblem::NoStripText);
    StripImage twice = StripClient();
    twice.Record(0x374000, {0x93, 0x80, 0x00, 0x00, 0x05, 0x02, 0x00, 0x00}, "menu    titlewin", 0);
    CHECK(LocateStripText(twice.Text(), twice.Dat()).problem == LocateProblem::AmbiguousStripText);
    StripImage elsewhere = StripClient();
    elsewhere.Code(0x13A324, {0x09}); // the help jump lands one byte past the shared SetText
    CHECK(LocateStripText(elsewhere.Text(), elsewhere.Dat()).problem == LocateProblem::StripTextChanged);
    StripImage moved = StripClient();
    moved.Code(0x221696, {0xD6}); // the picker's buffers no longer at 0x74 + 3 x 0xCB
    CHECK(LocateStripText(moved.Text(), moved.Dat()).problem == LocateProblem::StripTextChanged);
}

TEST(only_the_two_strip_windows_names_are_theirs)
{
    CHECK(IsStripWindow("menu    helpwind", StripWindow::Help) && IsStripWindow("menu    titlewin", StripWindow::Title));
    CHECK(!IsStripWindow("menu    titlewin", StripWindow::Help) && !IsStripWindow("menu    helpwinD", StripWindow::Help));
    CHECK(!IsStripWindow(nullptr, StripWindow::Title));
}

namespace
{
    // The same dump's connection readout: its two loads of the counters' block (mov eax,[net]; add eax,4638h; push
    // eax; call a getter) at RVAs 0x2013B6 and 0x201406, and the 3-second update at 0xFCB16 reading [net]+4190h, all
    // through the global at 0x0399FD98.
    constexpr uint32_t kNetTextRva = 0xF0000;

    struct NetImage
    {
        std::vector<uint8_t> text = std::vector<uint8_t>(0x120000, 0xCC);
        void Put(uint32_t rva, std::initializer_list<uint8_t> bytes)
        {
            std::memcpy(text.data() + (rva - kNetTextRva), bytes.begin(), bytes.size());
        }
        ImageSection Text() const { return {kNetTextRva, text.data(), static_cast<uint32_t>(text.size())}; }
    };

    NetImage NetClient()
    {
        NetImage image;
        image.Put(0x2013B0, {0x53, 0xE8, 0xCA, 0x83, 0xFC, 0xFF, 0xA1, 0x98, 0xFD, 0x99, 0x03, 0x05, 0x38, 0x46, 0x00, 0x00,
                                0x50, 0xE8, 0x8A, 0xB7, 0xEF, 0xFF});
        image.Put(0x201400, {0x53, 0xE8, 0x7A, 0x83, 0xFC, 0xFF, 0xA1, 0x98, 0xFD, 0x99, 0x03, 0x05, 0x38, 0x46, 0x00, 0x00,
                                0x50, 0xE8, 0x4A, 0xB7, 0xEF, 0xFF});
        image.Put(0xFCB10, {0x56, 0xE8, 0x2A, 0xFF, 0xFF, 0xFF, 0x33, 0xC0, 0x68, 0xB8, 0x0B, 0x00, 0x00, 0x89, 0x46, 0x0C,
                               0x89, 0x46, 0x10, 0x8B, 0x15, 0x98, 0xFD, 0x99, 0x03, 0x8B, 0x82, 0x90, 0x41, 0x00, 0x00, 0x50,
                               0x56, 0xE8, 0x2A, 0xFF, 0xFF, 0xFF});
        return image;
    }
}

TEST(the_connection_counters_are_found_through_the_readouts_two_loads_of_them)
{
    const NetStats found = LocateNetStats(NetClient().Text());
    CHECK(found.problem == LocateProblem::None);
    CHECK_EQ(found.global, 0x034C0000u + 0x4DFD98u);
}

TEST(a_client_without_the_readout_or_with_loads_that_disagree_is_left_alone)
{
    CHECK(LocateNetStats(NetImage{}.Text()).problem == LocateProblem::NoNetStats);
    NetImage apart = NetClient();
    apart.Put(0x201407, {0x9C}); // the second load reads another global
    CHECK(LocateNetStats(apart.Text()).problem == LocateProblem::AmbiguousNetStats);
    NetImage third = NetClient();
    third.Put(0x150000, {0xA1, 0x98, 0xFD, 0x99, 0x03, 0x05, 0x38, 0x46, 0x00, 0x00, 0x50, 0xE8});
    CHECK(LocateNetStats(third.Text()).problem == LocateProblem::AmbiguousNetStats);
    NetImage moved = NetClient();
    moved.Put(0xFCB2B, {0x94}); // the update reads its silence from another field
    CHECK(LocateNetStats(moved.Text()).problem == LocateProblem::NetStatsChanged);
}

TEST(counters_are_taken_only_as_the_client_keeps_them)
{
    CHECK(CountersFrom(227, 175, 100) == (NetCounters{227, 175, 100})); // R 227, S 175, the meter full
    CHECK(CountersFrom(0, 0, 0).has_value());                         // a minute of silence empties the meter
    CHECK(!CountersFrom(227, 175, 101).has_value());
    CHECK(!CountersFrom(227, 175, -1).has_value());
    CHECK(!CountersFrom(-1, 175, 100).has_value());
    CHECK(!CountersFrom(227, kMaxRate + 1, 100).has_value());
}

namespace
{
    constexpr uint32_t kMoveToRva = 0x119110; // where the code dump of 2026-10-07 has it

    void PutMoveTo(Image& image, uint32_t rva)
    {
        image.Put(rva, {0x83, 0xEC, 0x0C, 0x66, 0x8B, 0x44, 0x24, 0x14, 0x53, 0x55, 0x8B, 0x6C, 0x24, 0x18, 0x56, 0x8B,
            0xF1, 0x57, 0x33, 0xFF, 0x8D, 0x5E, 0x14, 0x66, 0x89, 0x6E, 0x52, 0x8B, 0xCB, 0x66, 0x89, 0x46, 0x54});
    }
}

TEST(the_window_move_is_found_once_by_its_start)
{
    Image image;
    PutMoveTo(image, kMoveToRva);
    const WindowMoveSite found = LocateWindowMove(image.Text());
    CHECK(found.problem == LocateProblem::None && found.moveTo == kMoveToRva);
    PutMoveTo(image, kMoveToRva + 0x1000);
    CHECK(LocateWindowMove(image.Text()).problem == LocateProblem::AmbiguousWindowMove);
    Image changed;
    PutMoveTo(changed, kMoveToRva);
    changed.Put(kMoveToRva + 26, {0x53}); // it stores the requested x somewhere else: another routine
    CHECK(LocateWindowMove(changed.Text()).problem == LocateProblem::NoWindowMove);
}

namespace
{
    // The builder of a window from the menu catalog, at RVA 0x117F40 in the client of 2026-10-22, called once at
    // 0x15E4A1: push ebx; push ebp; push esi; mov esi,ecx; mov ecx,[esp+10h]; push edi; mov [esi+4],ecx; call.
    constexpr uint32_t kBuildRva = 0x117F40, kBuildCallRva = 0x15E4A1;
    void PutBuild(Image& image, uint32_t at)
    {
        image.Put(at, {0x53, 0x55, 0x56, 0x8B, 0xF1, 0x8B, 0x4C, 0x24, 0x10, 0x57, 0x89, 0x4E, 0x04, 0xE8, 0xBE, 0x33,
                          0x00, 0x00, 0x8B, 0xD8, 0x85, 0xDB});
    }
    void PutCall(Image& image, uint32_t at, uint32_t to)
    {
        image.Put(at, {0xE8});
        image.Put32(at + 1, to - (at + kBranchLength));
    }
}

TEST(the_window_builder_is_found_with_its_one_call)
{
    Image image;
    CHECK(LocateWindowBuild(image.Text()).problem == LocateProblem::NoWindowBuild);
    PutBuild(image, kBuildRva);
    CHECK(LocateWindowBuild(image.Text()).problem == LocateProblem::NoWindowBuild); // never called
    PutCall(image, kBuildCallRva, kBuildRva);
    const WindowBuildSite found = LocateWindowBuild(image.Text());
    CHECK(found.problem == LocateProblem::None && found.call == kBuildCallRva && found.build == kBuildRva);
    PutCall(image, kBuildCallRva + 0x100, kBuildRva);
    CHECK(LocateWindowBuild(image.Text()).problem == LocateProblem::AmbiguousWindowBuild);
    Image twice;
    PutBuild(twice, kBuildRva);
    PutBuild(twice, kBuildRva + 0x1000);
    PutCall(twice, kBuildCallRva, kBuildRva);
    CHECK(LocateWindowBuild(twice.Text()).problem == LocateProblem::AmbiguousWindowBuild);
}

namespace
{
    // MCD::GetRect's start, at RVA 0x118F70 in the client of 2026-10-13: sub esp,0Ch; push esi; mov esi,ecx; mov al,
    // [esi+99h]; test al,al; jz, its rectangle copied into the place it is given.
    constexpr uint32_t kGetRectRva = 0x118F70;
    void PutGetRect(Image& image, uint32_t at)
    {
        image.Put(at, {0x83, 0xEC, 0x0C, 0x56, 0x8B, 0xF1, 0x8A, 0x86, 0x99, 0x00, 0x00, 0x00, 0x84, 0xC0, 0x0F, 0x84});
    }
}

namespace
{
    // MCD::GetEntryRect(int16* out, index, lit, pressed), at RVA 0x118900 in the client of 2026-10-19.
    constexpr uint32_t kEntryRectRva = 0x118900;
    void PutEntryRect(Image& image, uint32_t at)
    {
        image.Put(at, {0x8B, 0x44, 0x24, 0x08, 0x56, 0x50, 0x8B, 0xF1, 0xE8, 0xC3, 0xF8, 0xFF, 0xFF, 0x85, 0xC0, 0x74, 0x67,
                          0x0F, 0xBF, 0x48, 0x2A, 0x0F, 0xBF, 0x50, 0x2C});
    }
}

TEST(the_windows_entry_rectangle_is_found_beside_its_move)
{
    Image image;
    PutMoveTo(image, kMoveToRva);
    PutEntryRect(image, kEntryRectRva);
    CHECK(LocateWindowMove(image.Text()).entryRect == kEntryRectRva);
    PutEntryRect(image, kEntryRectRva + 0x2000);
    CHECK(LocateWindowMove(image.Text()).entryRect == 0); // two: neither is taken
}

TEST(the_windows_rectangle_copy_is_found_beside_its_move)
{
    Image image;
    PutMoveTo(image, kMoveToRva);
    PutGetRect(image, kGetRectRva);
    CHECK(LocateWindowMove(image.Text()).getRect == kGetRectRva);
    PutGetRect(image, kGetRectRva + 0x2000);
    CHECK(LocateWindowMove(image.Text()).getRect == 0); // two: neither is taken
    Image without;
    PutMoveTo(without, kMoveToRva);
    const WindowMoveSite found = LocateWindowMove(without.Text());
    CHECK(found.problem == LocateProblem::None && found.getRect == 0); // the move works without it
}

namespace
{
    // Where Wide Scan draws its grid label, at RVA 0x1FAA75 in the client of 2026-10-13: the strip title's owner from a
    // global, its window's rectangle by MCD::GetRect, then the label at the left, 3 below the bottom.
    constexpr uint32_t kScanLabelRva = 0x1FAA75;
    constexpr uint32_t kScanLabelCall = kScanLabelRva + 17;
    void PutScanLabel(Image& image, uint32_t at, uint32_t calls)
    {
        image.Put(at, {0x8B, 0x15, 0x7C, 0x84, 0xA3, 0x03, 0x83, 0xC4, 0x10, 0x8D, 0x4C, 0x24, 0x04, 0x51, 0x8B, 0x4A,
                          0x08, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x4C, 0x24, 0x0A, 0x8B, 0x54, 0x24, 0x04, 0x8D, 0x44,
                          0x24, 0x10, 0x68, 0x80, 0x80, 0x80, 0x80, 0x83, 0xC1, 0x03});
        image.Put32(at + 18, calls - (at + 17 + kBranchLength));
    }
}

TEST(wide_scans_grid_label_takes_the_title_windows_place_from_one_call_to_get_rect)
{
    Image image;
    PutScanLabel(image, kScanLabelRva, kGetRectRva);
    const TitleRectSite found = LocateScanLabel(image.Text(), kGetRectRva);
    CHECK(found.problem == LocateProblem::None);
    CHECK_EQ(found.call, kScanLabelCall);
    CHECK(LocateScanLabel(image.Text(), 0).problem == LocateProblem::NoScanLabel); // GetRect itself not found
    CHECK(LocateScanLabel(Image{}.Text(), kGetRectRva).problem == LocateProblem::NoScanLabel);
    Image elsewhere;
    PutScanLabel(elsewhere, kScanLabelRva, kGetRectRva + 0x40);
    CHECK(LocateScanLabel(elsewhere.Text(), kGetRectRva).problem == LocateProblem::ScanLabelChanged);
    PutScanLabel(image, 0x100000, kGetRectRva);
    CHECK(LocateScanLabel(image.Text(), kGetRectRva).problem == LocateProblem::AmbiguousScanLabel);
}

namespace
{
    // Where an item list draws its count at the strip title's corner (RVA 0x1CB590 in the client of 2026-10-19, which
    // five lists call): the title's owner from a global, its window's rectangle by MCD::GetRect, then the slash and
    // the two numbers from that rectangle.
    constexpr uint32_t kStripCountRva  = 0x1CB590;
    constexpr uint32_t kStripCountCall = kStripCountRva + 58;
    void PutStripCount(Image& image, uint32_t at, uint32_t calls)
    {
        image.Put(at, {0xA1, 0x7C, 0x84, 0xA3, 0x03, 0x83, 0xEC, 0x2C, 0x53, 0x33, 0xDB, 0x56, 0x3B, 0xC3, 0x57, 0x0F,
                          0x84, 0x87, 0x01, 0x00, 0x00, 0x39, 0x58, 0x08, 0x0F, 0x84, 0x7E, 0x01, 0x00, 0x00, 0x8D, 0x4C,
                          0x24, 0x0C, 0x66, 0x89, 0x5C, 0x24, 0x0C, 0x66, 0x89, 0x5C, 0x24, 0x0E, 0x66, 0x89, 0x5C, 0x24,
                          0x10, 0x66, 0x89, 0x5C, 0x24, 0x12, 0x51, 0x8B, 0x48, 0x08, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x8B,
                          0x7C, 0x24, 0x3C});
        image.Put32(at + 59, calls - (at + 58 + kBranchLength));
    }
}

TEST(an_item_lists_count_takes_the_title_windows_place_from_one_call_to_get_rect)
{
    Image image;
    PutStripCount(image, kStripCountRva, kGetRectRva);
    const TitleRectSite found = LocateStripCount(image.Text(), kGetRectRva);
    CHECK(found.problem == LocateProblem::None);
    CHECK_EQ(found.call, kStripCountCall);
    CHECK(LocateStripCount(image.Text(), 0).problem == LocateProblem::NoStripCount);
    CHECK(LocateStripCount(Image{}.Text(), kGetRectRva).problem == LocateProblem::NoStripCount);
    Image elsewhere;
    PutStripCount(elsewhere, kStripCountRva, kGetRectRva + 0x40);
    CHECK(LocateStripCount(elsewhere.Text(), kGetRectRva).problem == LocateProblem::StripCountChanged);
    PutStripCount(image, 0x100000, kGetRectRva);
    CHECK(LocateStripCount(image.Text(), kGetRectRva).problem == LocateProblem::AmbiguousStripCount);
}

TEST(only_a_countdown_above_zero_is_a_move_under_way)
{
    CHECK(!MoveUnderWay(kMoveIdle));
    CHECK(!MoveUnderWay(0)); // the frame the game laid the window out, as a window opening is: it may be placed
    CHECK(MoveUnderWay(1));
    CHECK(MoveUnderWay(12));
}

TEST(a_window_is_known_by_its_name_past_menu_and_written_without_its_padding)
{
    const std::optional<WindowKey> equip = WindowKeyOf("menu    equip   ");
    CHECK(equip && ShortName(*equip) == "equip" && KeyFromShortName("equip") == equip);
    CHECK(ShortName(*WindowKeyOf("menu    statcom2")) == "statcom2");
    CHECK(!WindowKeyOf("main    equip   ") && !WindowKeyOf(nullptr)); // not a menu window
    // A name no window has: too long, empty, or with characters the game's names never use.
    for (const char* bad : {"", "equipment", "Equip", "eq ip", "eq-ip"})
        CHECK(!KeyFromShortName(bad));
}

TEST(the_strip_windows_are_never_moved_and_the_equipment_window_has_a_name)
{
    for (const char* strip : {"menu    helpwind", "menu    titlewin", "menu    netstat "})
        CHECK(!MovableWindow(*WindowKeyOf(strip)));
    CHECK(MovableWindow(*KeyFromShortName("equip")));
    CHECK(std::string(WindowLabel("equip")) == "Equipment" && WindowLabel("chmkface") == nullptr);
}

TEST(the_window_list_holds_each_key_once_and_no_more_than_it_has_room_for)
{
    WindowKeyList list;
    const WindowKey equip = *KeyFromShortName("equip");
    CHECK(list.Add(equip) && !list.Add(equip) && list.Size() == 1 && list.Has(equip));
    CHECK(!list.Has(*KeyFromShortName("inventor")));
    for (size_t i = 1; i < kMaxWindowKeys; ++i)
        CHECK(list.Add(*KeyFromShortName("w" + std::to_string(i))));
    CHECK(!list.Add(*KeyFromShortName("full")) && list.Size() == kMaxWindowKeys);
    list.Clear();
    CHECK(list.Size() == 0 && !list.Has(equip));
}

namespace
{
    constexpr int kScreenRight = 1920, kScreenBottom = 1080, kLeft = 100, kWidth = 200;

    // Where a window kLeft from the screen's left goes down to, by shift alone.
    std::optional<int> TopFor(WindowPlacements& placements, uint32_t mcd, const WindowKey& key, int top, int height,
        int shift)
    {
        const std::optional<WindowSpot> to = placements.PlaceFor(mcd, key, WindowSpot{kLeft, top}, kWidth, height, 0,
            shift, kScreenRight, kScreenBottom);
        if (to) CHECK_EQ(to->left, kLeft); // a shift moves nothing sideways
        return to ? std::optional<int>(to->top) : std::nullopt;
    }
}

TEST(every_moved_window_goes_down_by_the_same_shift_and_keeps_its_place_beside_the_others)
{
    WindowPlacements placements;
    const WindowKey status = *KeyFromShortName("statcom"), gil = *KeyFromShortName("money");
    // The status window over the gil box: both go down 130, so the box stays under it.
    CHECK(TopFor(placements, 0x1000, status, 60, 120, 130) == 190);
    CHECK(TopFor(placements, 0x2000, gil, 180, 40, 130) == 310);
    // Moved, each is where HeadsUp put it: nothing more, so nothing adds up.
    CHECK(!TopFor(placements, 0x1000, status, 190, 120, 130));
    CHECK(!TopFor(placements, 0x2000, gil, 310, 40, 130));
    // The help bar grows: they follow from where the game had them.
    CHECK(TopFor(placements, 0x1000, status, 190, 120, 150) == 210);
}

TEST(a_window_the_game_places_again_goes_down_from_its_new_place)
{
    WindowPlacements placements;
    const WindowKey inventory = *KeyFromShortName("inventor");
    CHECK(TopFor(placements, 0x1000, inventory, 60, 300, 130) == 190);
    CHECK(TopFor(placements, 0x1000, inventory, 100, 300, 130) == 230); // the game moved it to 100
    // Another window at the same address is another window.
    CHECK(TopFor(placements, 0x1000, *KeyFromShortName("equip"), 190, 300, 130) == 320);
}

TEST(a_tall_window_goes_only_as_low_as_the_screen_lets_it_and_never_up)
{
    WindowPlacements placements;
    const WindowKey tall = *KeyFromShortName("fulllog");
    CHECK(TopFor(placements, 0x1000, tall, 60, 960, 130) == 120); // its bottom at the screen's
    CHECK(!TopFor(placements, 0x2000, tall, 60, 1020, 130));      // one already that low stays
    CHECK(!TopFor(placements, 0x3000, tall, 200, 1000, 130));     // and none goes up to fit
}

TEST(an_offset_moves_a_window_either_way_from_where_the_game_puts_it_and_keeps_it_on_screen)
{
    WindowPlacements placements;
    const WindowKey equip = *KeyFromShortName("equip");
    const auto place = [&](uint32_t mcd, WindowSpot now, int dx, int dy) {
        return placements.PlaceFor(mcd, equip, now, kWidth, 300, dx, dy, kScreenRight, kScreenBottom);
    };
    CHECK(place(0x1000, {100, 60}, 40, -20) == (WindowSpot{140, 40}));
    CHECK(!place(0x1000, {140, 40}, 40, -20)); // where it was put: no more
    // Kept on screen: not past either side, the top or the bottom.
    CHECK(place(0x3000, {100, 60}, -500, -500) == (WindowSpot{0, 0}));
    CHECK(place(0x4000, {100, 60}, 5000, 5000) == (WindowSpot{kScreenRight - kWidth, kScreenBottom - 300}));
}

TEST(window_places_read_back_what_they_wrote_and_keep_a_place_at_the_origin)
{
    const std::vector<WindowPlace> places = WindowPlacesFrom("equip:100,200 statcom:0,0");
    CHECK(places == (std::vector<WindowPlace>{{"equip", 100, 200}, {"statcom", 0, 0}}));
    CHECK(WindowPlacesFrom(WindowPlaceText(places)) == places);
    // A strip window, a name no window has, a repeat, garbled numbers: all dropped.
    CHECK(WindowPlacesFrom("helpwind:5,5 equipment:5,5 equip:1,1 equip:2,2 inventor:x,3 itemctrl:4").size() == 1);
    // Off the screen's top left or far past it, a place is held to the screen's corner and to the limit.
    CHECK(WindowPlacesFrom("equip:99999,-40") == (std::vector<WindowPlace>{{"equip", kMaxWindowOffset, 0}}));
    CHECK(WindowPlacesFrom("").empty() && WindowPlaceText({}).empty());
}

TEST(hidden_windows_read_back_what_they_wrote_and_drop_what_no_window_could_take)
{
    const std::vector<std::string> hidden = HiddenWindowsFrom("statcom inventor helpwind equipment statcom");
    CHECK((hidden == std::vector<std::string>{"statcom", "inventor"}));
    CHECK(HiddenWindowsFrom(HiddenWindowsText(hidden)) == hidden);
    CHECK(HiddenWindowsFrom("").empty() && HiddenWindowsText({}).empty());
}

TEST(the_weather_is_found_once_by_its_effects_range_check)
{
    // From the client: mov ax,[03945FE0]; cmp ax,4; jb +7E; cmp ax,20.
    constexpr uint32_t kWeatherRead = kTextRva + 0x120000, kWeatherGlobal = 0x03945FE0;
    const auto put = [&](Image& image, uint32_t at) {
        image.Put(at, {0x66, 0xA1});
        image.Put32(at + 2, kWeatherGlobal);
        image.Put(at + 6, {0x66, 0x3D, 0x04, 0x00, 0x72, 0x7E, 0x66, 0x3D, 0x14, 0x00});
    };
    Image image;
    CHECK(LocateWeather(image.Text()).problem == LocateProblem::NoWeather);
    put(image, kWeatherRead);
    const WeatherSite found = LocateWeather(image.Text());
    CHECK(found.problem == LocateProblem::None && found.global == kWeatherGlobal);
    put(image, kWeatherRead + 0x100);
    CHECK(LocateWeather(image.Text()).problem == LocateProblem::AmbiguousWeather);
    Image changed;
    put(changed, kWeatherRead);
    changed.Put(kWeatherRead + 14, {0x15}); // 21 weathers: another check, not the client's
    CHECK(LocateWeather(changed.Text()).problem == LocateProblem::NoWeather);
}

TEST(every_client_window_is_listed_once_named_ones_first_and_never_the_strips)
{
    const std::vector<std::string>& all = AllWindows();
    CHECK(all.size() > 400 && all.front() == "equip" && WindowLabel(all.front()) != nullptr);
    for (const char* strip : {"helpwind", "titlewin", "netstat"})
        CHECK(std::ranges::find(all, strip) == all.end());
    for (const std::string& name : all)
        CHECK(std::ranges::count(all, name) == 1 && KeyFromShortName(name).has_value());
    // The named ones come before any without a name.
    const auto firstUnnamed = std::ranges::find_if(all, [](const std::string& n) { return WindowLabel(n) == nullptr; });
    CHECK(std::all_of(firstUnnamed, all.end(), [](const std::string& n) { return WindowLabel(n) == nullptr; }));
}

TEST(where_the_game_put_a_window_is_where_it_came_from_once_moved_else_where_it_is)
{
    WindowPlacements placements;
    const WindowKey menu = *KeyFromShortName("menuwind");
    CHECK(placements.PlaceFor(0x2000, menu, {0, 60}, 200, 300, 0, 130, kScreenRight, kScreenBottom) ==
          (WindowSpot{0, 190}));
    CHECK(placements.OriginOf(0x2000, menu, {0, 190}) == (WindowSpot{0, 60}));
    CHECK(placements.OriginOf(0x2000, menu, {0, 75}) == (WindowSpot{0, 75}));
    CHECK(HelpStripWindow(*WindowKeyOf("menu    helpwind")) && !HelpStripWindow(*WindowKeyOf("menu    netstat ")));
    // The connection readout is no part of the strip, but goes where the strip goes.
    CHECK(FollowsHelpStrip(*WindowKeyOf("menu    netstat ")) && FollowsHelpStrip(*WindowKeyOf("menu    titlewin")));
    CHECK(!FollowsHelpStrip(*KeyFromShortName("equip")));
}

TEST(a_window_the_game_moves_goes_down_from_where_it_meant_and_is_not_moved_again)
{
    WindowPlacements placements;
    const WindowKey profile = *KeyFromShortName("persona");
    // The owner asks for 16, 32; HeadsUp takes it 16 lower at once, and the frame's pass finds it placed.
    CHECK(placements.Moved(0x1000, profile, {16, 32}, 112, 190, 0, 16, kScreenRight, kScreenBottom) ==
          (WindowSpot{16, 48}));
    CHECK(!placements.PlaceFor(0x1000, profile, {16, 48}, 112, 190, 0, 16, kScreenRight, kScreenBottom));
    CHECK(placements.OriginOf(0x1000, profile, {16, 48}) == (WindowSpot{16, 32}));
    // Held to the screen as the frame's pass holds it.
    CHECK(placements.Moved(0x2000, profile, {16, 100}, 112, 1000, 0, 130, kScreenRight, kScreenBottom) ==
          (WindowSpot{16, 100}));
}

TEST(a_windows_course_is_where_headsup_puts_it_while_it_stands_at_either_end)
{
    WindowPlacements placements;
    const WindowKey level = *KeyFromShortName("level");
    // Unknown: where it stands, both ways.
    CHECK(placements.CourseOf(0x1000, level, {16, 208}) == (WindowCourse{{16, 208}, {16, 208}}));
    // Planned as it was built and not moved yet, then moved: the same course.
    placements.Moved(0x1000, level, {16, 208}, 112, 88, 0, 16, kScreenRight, kScreenBottom);
    CHECK(placements.CourseOf(0x1000, level, {16, 208}) == (WindowCourse{{16, 208}, {16, 224}}));
    CHECK(placements.CourseOf(0x1000, level, {16, 224}) == (WindowCourse{{16, 208}, {16, 224}}));
    // Put somewhere else since, by the game or the player: where it stands.
    CHECK(placements.CourseOf(0x1000, level, {16, 300}) == (WindowCourse{{16, 300}, {16, 300}}));
}

TEST(a_top_the_game_asks_for_where_the_window_stands_is_the_one_it_had)
{
    // Profile put at a new top: that is the game's.
    CHECK(GameMeant({16, 32}, {16, 64}, {16, 48}) == (WindowSpot{16, 32}));
    // Besieged slid sideways from where it stands, and the Level box put beside the moved Profile: the top is the one
    // HeadsUp gave it or means to, so the game's is still the origin's.
    CHECK(GameMeant({1768, 64}, {1792, 64}, {1792, 48}) == (WindowSpot{1768, 48}));
    CHECK(GameMeant({16, 224}, {16, 224}, {16, 208}) == (WindowSpot{16, 208}));
}

TEST(an_equipment_figure_still_in_the_windows_old_place_moves_with_it_and_no_other)
{
    const WindowSpot at{100, 60}, to{100, 190};
    CHECK(MovedNumber({140, 200}, at, 300, 250, to) == (WindowSpot{140, 330})); // Attack, in the old place
    CHECK(MovedNumber({140, 330}, at, 300, 250, to) == (WindowSpot{140, 330})); // placed already: below the old place
    CHECK(MovedNumber({0, 0}, at, 300, 250, to) == (WindowSpot{0, 0}));         // never placed
    CHECK(MovedNumber({90, 100}, at, 300, 250, to) == (WindowSpot{90, 100}));   // left of the window
}

TEST(the_windows_to_put_back_are_those_moved_that_drew_lately)
{
    WindowPlacements placements;
    const WindowKey equip = *KeyFromShortName("equip"), status = *KeyFromShortName("statcom");
    placements.SetFrame(1);
    CHECK(placements.PlaceFor(0x1000, equip, WindowSpot{10, 20}, 100, 100, 0, 5, 1000, 1000));
    CHECK(!placements.PlaceFor(0x2000, status, WindowSpot{30, 40}, 100, 100, 0, 0, 1000, 1000)); // not moved
    placements.SetFrame(5);
    CHECK(placements.MovedSince(1).size() == 1);
    const WindowPlacements::Placed moved = placements.MovedSince(1).front();
    CHECK(moved.mcd == 0x1000 && moved.from == (WindowSpot{10, 20}) && moved.to == (WindowSpot{10, 25}));
    CHECK(placements.MovedSince(4).empty()); // not drawn since: closed, and opens where the game puts it
}

namespace
{
    // The game's cast bar: its owner puts it back at (16, 47) whenever it is anywhere else. Each frame HeadsUp finds
    // it there and asks to move it 23 down, under the bar.
    std::optional<WindowSpot> PlaceCastBar(WindowPlacements& placements, uint32_t frame, WindowSpot now)
    {
        placements.SetFrame(frame);
        return placements.PlaceFor(0x1000, *KeyFromShortName("casttime"), now, 128, 24, 0, 23, 1920, 1080);
    }
}

TEST(a_window_its_owner_puts_back_frame_after_frame_is_known_to_place_itself)
{
    WindowPlacements placements;
    const WindowKey cast = *KeyFromShortName("casttime");
    const WindowSpot games{16, 47};
    CHECK(PlaceCastBar(placements, 1, games) == (WindowSpot{16, 70})); // moved for the first time
    CHECK(!placements.PutsItselfBack(cast));
    PlaceCastBar(placements, 2, games); // back where the game had it: once
    PlaceCastBar(placements, 3, games);
    CHECK(!placements.PutsItselfBack(cast));
    CHECK(PlaceCastBar(placements, 4, games) == (WindowSpot{16, 70})); // a third time running: still to be moved
    CHECK(placements.PutsItselfBack(cast));
    CHECK(!placements.PutsItselfBack(*KeyFromShortName("equip")));
    // Left where HeadsUp put it from then on, it needs no move.
    CHECK(!PlaceCastBar(placements, 5, WindowSpot{16, 70}));
}

TEST(a_window_the_game_moved_back_now_and_then_does_not_place_itself)
{
    WindowPlacements placements;
    const WindowKey cast = *KeyFromShortName("casttime");
    const WindowSpot games{16, 47};
    // As a list does on a page change: put back once, then left where HeadsUp moves it for a while.
    for (uint32_t frame = 1; frame < 200; frame += 20)
    {
        PlaceCastBar(placements, frame, games);
        PlaceCastBar(placements, frame + 1, WindowSpot{16, 70});
    }
    CHECK(!placements.PutsItselfBack(cast));
}

// The title window as the test client had it with the Items menu open (2026-10-12): the game puts the window at
// (16, 17) and its text at (20, 24), in the menu's 1920 by 1080; HeadsUp moved the window down 40 and the text stayed.
TEST(a_text_the_game_places_once_follows_the_window_headsup_moves)
{
    const WindowSpot origin{16, 17}, moved{16, 57};
    PlacedText title;
    CHECK(title.Follow({20, 24}, origin, origin) == (WindowSpot{20, 24})); // nothing moved: where the game has it
    CHECK(title.Follow({20, 24}, origin, moved) == (WindowSpot{20, 64}));  // down with its window
    CHECK(title.Follow({20, 64}, origin, moved) == (WindowSpot{20, 64}));  // and it stays, frame after frame
    CHECK(title.Natural({20, 64}) == (WindowSpot{20, 24}));                // where to put it back
    // The game places it again, as for a new caption: it goes with the window again, from there.
    CHECK(title.Follow({22, 24}, origin, moved) == (WindowSpot{22, 64}));
    // The window moves on, as when the bar's height changes: from where the game had the text, not from the last.
    CHECK(title.Follow({22, 64}, origin, {16, 70}) == (WindowSpot{22, 77}));
    CHECK(title.Follow({22, 77}, origin, origin) == (WindowSpot{22, 24})); // and home with it
    CHECK(title.Natural({22, 24}) == (WindowSpot{22, 24}));
}

// A short bar moves the window less than its own height, so the moved text is still inside where the window was: it
// must not be taken for one the game just placed and moved again.
TEST(a_text_moved_less_than_its_windows_height_is_not_moved_twice)
{
    const WindowSpot origin{16, 17}, moved{16, 37};
    PlacedText title;
    CHECK(title.Follow({20, 24}, origin, moved) == (WindowSpot{20, 44}));
    for (int frame = 0; frame < 3; ++frame)
        CHECK(title.Follow({20, 44}, origin, moved) == (WindowSpot{20, 44}));
    PlacedText fresh; // a text HeadsUp never moved is its own natural place
    CHECK(fresh.Natural({20, 24}) == (WindowSpot{20, 24}));
}


TEST(an_owners_copy_of_its_windows_place_moves_with_the_window)
{
    // As read from the test client (2026-10-15): the Level box's owner keeps its window's rectangle at +0x14 exactly,
    // the craft skill list's keeps the same left, top and right with the list's full height.
    using Place = std::array<int16_t, 4>;
    CHECK((MovedPlaceCopy(Place{16, 208, 128, 296}, Place{16, 208, 128, 296}, 0, 48) == Place{16, 256, 128, 344}));
    CHECK((MovedPlaceCopy(Place{130, 48, 382, 184}, Place{130, 48, 382, 140}, 0, 48) == Place{130, 96, 382, 232}));
    CHECK((MovedPlaceCopy(Place{130, 48, 382, 184}, Place{130, 48, 382, 140}, 6, -10) == Place{136, 38, 388, 174}));
    // Whatever else an owner keeps there is left alone.
    CHECK(!MovedPlaceCopy(Place{0, 48, 382, 184}, Place{130, 48, 382, 140}, 0, 48));
    CHECK(!MovedPlaceCopy(Place{130, 49, 382, 184}, Place{130, 48, 382, 140}, 0, 48));
    CHECK(!MovedPlaceCopy(Place{130, 48, 380, 184}, Place{130, 48, 382, 140}, 0, 48));
    CHECK(!MovedPlaceCopy(Place{130, 48, 382, 20}, Place{130, 48, 382, 140}, 0, 48)); // a bottom above its top
    // Where the class's layout says nothing, only a copy that is the window's rectangle exactly is taken for one.
    CHECK((MovedPlaceCopy(Place{16, 48, 382, 104}, Place{16, 48, 382, 104}, 0, 16, PlaceMatch::Exact) == Place{16, 64, 382, 120}));
    CHECK(!MovedPlaceCopy(Place{130, 48, 382, 184}, Place{130, 48, 382, 140}, 0, 48, PlaceMatch::Exact));
}

TEST(an_owners_copy_of_an_entrys_corner_moves_with_the_window_while_it_is_inside_it)
{
    // The Linkshell item's window (link2): the owner keeps its middle button's corner, the window at 1792, 48.
    const WindowRect window{1792, 48, 1904, 106};
    CHECK((MovedEntryPlace(WindowSpot{1808, 70}, window, 0, 16) == WindowSpot{1808, 86}));
    CHECK((MovedEntryPlace(WindowSpot{1792, 48}, window, -4, 16) == WindowSpot{1788, 64}));
    // A point outside the window is no place in it: left alone.
    CHECK(!MovedEntryPlace(WindowSpot{1808, 106}, window, 0, 16) && !MovedEntryPlace(WindowSpot{0, 0}, window, 0, 16));
}

TEST(an_owners_copy_of_an_entrys_corner_is_found_after_its_layouts_call_for_the_entrys_rectangle)
{
    // The Linkshell item's owner (the client's 0x1E5F90 of 2026-10-19): the call at 0x1E5FDE, then
    // mov cx,[esp+0Ch]; mov ax,[esi+14h]; mov dx,[esp+0Eh]; mov [esi+18h],cx; mov ecx,[esi+8]; push 1; push eax;
    // mov [esi+1Ah],dx.
    constexpr uintptr_t entryRect = 0x118900, at = 0x1E5FDE;
    const std::vector<uint8_t> corner{0xE8, 0x1D, 0x29, 0xF3, 0xFF, 0x66, 0x8B, 0x4C, 0x24, 0x0C, 0x66, 0x8B, 0x46, 0x14,
        0x66, 0x8B, 0x54, 0x24, 0x0E, 0x66, 0x89, 0x4E, 0x18, 0x8B, 0x4E, 0x08, 0x6A, 0x01, 0x50, 0x66, 0x89, 0x56, 0x1A};
    CHECK(EntryPlaceOffset(corner, at, entryRect) == std::optional<uint32_t>(0x18));
    CHECK(!EntryPlaceOffset(corner, at, entryRect + 0x10)); // a call to another routine
    // Only its left kept, or the two halves in two objects: no corner.
    std::vector<uint8_t> left = corner;
    left.resize(left.size() - 4);
    CHECK(!EntryPlaceOffset(left, at, entryRect));
    std::vector<uint8_t> apart = corner;
    apart[apart.size() - 2] = 0x57; // mov [edi+1Ah],dx
    CHECK(!EntryPlaceOffset(apart, at, entryRect));
}

TEST(a_layout_that_starts_with_its_base_classs_layout_names_it)
{
    // The Linkshell menu's owner (link13, Set as No. 1 / No. 2 / Drop; the client's 0x1E6510 of 2026-10-27): push esi;
    // mov esi,ecx; call 0x1E5F90, the Linkshell item's layout, which keeps the first entry's corner.
    constexpr uintptr_t at = 0x1E6510;
    const std::vector<uint8_t> link13{0x56, 0x8B, 0xF1, 0xE8, 0x78, 0xFA, 0xFF, 0xFF, 0x8B, 0x0D, 0x20, 0x35, 0x7F, 0x03};
    CHECK(BaseLayout(link13, at) == std::optional<uintptr_t>(0x1E5F90));
    // Started with other work, or a call later on: none.
    const std::vector<uint8_t> work{0x83, 0xEC, 0x08, 0xE8, 0x78, 0xFA, 0xFF, 0xFF};
    CHECK(!BaseLayout(work, at));
    std::vector<uint8_t> late = link13;
    late.insert(late.begin() + 3, {0x8B, 0x46, 0x14, 0x85, 0xC0, 0x74, 0x10, 0x90, 0x90});
    CHECK(!BaseLayout(late, at));
    CHECK(!BaseLayout(std::vector<uint8_t>{0x56, 0x8B, 0xF1}, at));
}

TEST(an_owners_place_copy_is_found_in_its_layouts_call_for_the_windows_rectangle)
{
    // The client's own bytes (2026-10-13 build): each layout pushes the address of its copy and calls MCD::GetRect.
    constexpr uintptr_t getRect = 0x118F70;
    // Combat and magic skills: lea eax,[ecx+18h]; mov ecx,[ecx+8]; push eax; call GetRect.
    const std::vector<uint8_t> skills{0x8D, 0x41, 0x18, 0x8B, 0x49, 0x08, 0x50, 0xE8, 0xC4, 0x0B, 0xFD, 0xFF};
    CHECK(PlaceCopyOffset(skills, 0x1483A0, getRect) == std::optional<uint32_t>(0x18));
    // Craft skills: mov ecx,[esi+8]; lea eax,[esi+14h]; push eax; call GetRect.
    const std::vector<uint8_t> craft{0x8B, 0x4E, 0x08, 0x8D, 0x46, 0x14, 0x50, 0xE8, 0x3A, 0x07, 0xFD, 0xFF};
    CHECK(PlaceCopyOffset(craft, 0x14882A, getRect) == std::optional<uint32_t>(0x14));
    // The Level box moves its window first (a call to MCD::MoveTo), then: mov ecx,[esi+8]; lea edx,[esi+14h]; push edx.
    const std::vector<uint8_t> level{0xE8, 0x78, 0x1A, 0xFD, 0xFF, 0x8B, 0x4E, 0x08, 0x8D, 0x56, 0x14, 0x52, 0xE8, 0xCC, 0x18,
        0xFD, 0xFF};
    CHECK(PlaceCopyOffset(level, 0x147693, getRect) == std::optional<uint32_t>(0x14));
    // Trade (the owner of your side): mov ecx,[esi+8]; lea eax,[esi+18h]; push eax; mov word [esi+0BCh],1; call.
    const std::vector<uint8_t> trade{0x8B, 0x4E, 0x08, 0x8D, 0x46, 0x18, 0x50, 0x66, 0xC7, 0x86, 0xBC, 0x00, 0x00, 0x00, 0x01,
        0x00, 0xE8, 0xD7, 0x7B, 0xFC, 0xFF};
    CHECK(PlaceCopyOffset(trade, 0x151384, getRect) == std::optional<uint32_t>(0x18));
    // A layout that returns before any such call keeps none: the code after its ret and padding is another
    // function's (the client's 0x12BF50 is three instructions and a ret, before a layout that does call it).
    std::vector<uint8_t> shortLayout{0x8B, 0x49, 0x08, 0x6A, 0x00, 0x6A, 0x01, 0xE8, 0x54, 0xD3, 0xFE, 0xFF, 0xC3, 0x90, 0x90, 0x90};
    shortLayout.insert(shortLayout.end(), craft.begin(), craft.end());
    constexpr uintptr_t landsAt = 0xFC6A6; // where the craft bytes' call lands when they start at 0x12BF60
    CHECK(!PlaceCopyOffset(shortLayout, 0x12BF50, landsAt));
    // A ret in the middle of a layout, with its code going on after it, does not end the search.
    std::vector<uint8_t> earlyReturn{0x85, 0xC0, 0x75, 0x01, 0xC3};
    earlyReturn.insert(earlyReturn.end(), craft.begin(), craft.end());
    CHECK(PlaceCopyOffset(earlyReturn, 0x148825, getRect) == std::optional<uint32_t>(0x14));
    // Font Colors' sample (0x198C16): add ecx,14h; push ecx; mov ecx,eax; call, with ecx the owner.
    const std::vector<uint8_t> sample{0x83, 0xC1, 0x14, 0x51, 0x8B, 0xC8, 0xE8, 0x4F, 0x03, 0xF8, 0xFF};
    CHECK(PlaceCopyOffset(sample, 0x198C16, getRect) == std::optional<uint32_t>(0x14));
    const std::vector<uint8_t> subtracted{0x83, 0xE9, 0x14, 0x51, 0x8B, 0xC8, 0xE8, 0x4F, 0x03, 0xF8, 0xFF};
    CHECK(!PlaceCopyOffset(subtracted, 0x198C16, getRect)); // sub ecx,14h: no field of the owner
    // No call for the rectangle, another register pushed than the one loaded, or the call elsewhere: no copy.
    CHECK(!PlaceCopyOffset(craft, 0x14882A, getRect + 0x10));
    const std::vector<uint8_t> otherPush{0x8B, 0x4E, 0x08, 0x8D, 0x46, 0x14, 0x52, 0xE8, 0x3A, 0x07, 0xFD, 0xFF};
    CHECK(!PlaceCopyOffset(otherPush, 0x14882A, getRect));
    CHECK(!PlaceCopyOffset(std::vector<uint8_t>{0x50, 0xE8, 0x3A, 0x07, 0xFD, 0xFF}, 0x148830, getRect));
    // lea eax,[esi]; nop; push eax: no displacement, and no copy read off the byte after it.
    const std::vector<uint8_t> noDisplacement{0x8D, 0x06, 0x90, 0x50, 0xE8, 0x3A, 0x07, 0xFD, 0xFF};
    CHECK(!PlaceCopyOffset(noDisplacement, 0x14882D, getRect));
}

TEST(the_config_getter_is_the_first_place_its_bytes_are)
{
    // From the client: mov ecx,[config]; test ecx,ecx; jz; mov eax,[esp+4]; push eax; call get; ret; or eax,-1; ret.
    constexpr uint32_t kGetter = kTextRva + 0x153850;
    const auto put = [&](Image& image, uint32_t at) {
        image.Put(at, {0x8B, 0x0D, 0xA8, 0x3C, 0x57, 0x10, 0x85, 0xC9, 0x74, 0x0C, 0x8B, 0x44, 0x24, 0x04, 0x50, 0xE8, 0x10,
                          0x20, 0x30, 0x00, 0xC3, 0x83, 0xC8, 0xFF, 0xC3});
    };
    Image image;
    CHECK(LocateConfigGet(image.Text()).problem == LocateProblem::NoConfigGet);
    put(image, kGetter);
    // The client has a second getter of the same bytes after it, for another table: Ashita's config addon takes the first.
    put(image, kGetter + 0x20);
    const ConfigGetSite found = LocateConfigGet(image.Text());
    CHECK(found.problem == LocateProblem::None && found.entry == kGetter);
}

TEST(the_search_servers_address_is_where_the_search_clients_setup_clears_it)
{
    // From the client (RVA 0xE1470): xor ebx,ebx; push edi; mov [address],ebx; mov [port],bx; call.
    constexpr uint32_t kSetup = kTextRva + 0xE1470, kAddress = 0x0396E8BC;
    const auto put = [](Image& image, uint32_t at, uint32_t address, uint32_t port) {
        image.Put(at, {0x33, 0xDB, 0x57, 0x89, 0x1D});
        image.Put32(at + 5, address);
        image.Put(at + 9, {0x66, 0x89, 0x1D});
        image.Put32(at + 12, port);
        image.Put(at + 16, {0xE8});
    };
    Image image;
    CHECK(LocateSearchAddress(image.Text()).problem == LocateProblem::NoSearchAddress);
    put(image, kSetup, kAddress, kAddress + 4);
    const SearchAddressSite found = LocateSearchAddress(image.Text());
    CHECK(found.problem == LocateProblem::None && found.address == kAddress && found.port == kAddress + 4);
    // Two clears not side by side are some other setup's.
    Image apart;
    put(apart, kSetup, kAddress, kAddress + 8);
    CHECK(LocateSearchAddress(apart.Text()).problem == LocateProblem::NoSearchAddress);
}

namespace
{
    // The Network window's two PlayOnline getters as the code dump of 2026-10-26 has them, their global moved to the
    // base the counters' fixture uses.
    NetImage PlayOnlineClient()
    {
        NetImage image;
        image.Put(0xF6F00, {0xA1, 0x08, 0xF9, 0x99, 0x03, 0x85, 0xC0, 0x74, 0x05, 0x66, 0x8B, 0x40, 0x0E, 0xC3, 0x33, 0xC0, 0xC3});
        image.Put(0xF6F30, {0xA1, 0x08, 0xF9, 0x99, 0x03, 0x85, 0xC0, 0x74, 0x06, 0x66, 0x8B, 0x40, 0x0A, 0xEB, 0x02, 0x33, 0xC0,
                               0x66, 0x3D, 0xC8, 0x00, 0x73, 0x06, 0x25, 0xFF, 0xFF, 0x00, 0x00, 0xC3, 0xB8, 0xC8, 0x00, 0x00,
                               0x00, 0xC3});
        return image;
    }
}

TEST(the_playonline_counts_are_found_through_the_network_windows_two_getters)
{
    const PlayOnlineSite found = LocatePlayOnline(PlayOnlineClient().Text());
    CHECK(found.problem == LocateProblem::None);
    CHECK_EQ(found.global, 0x034C0000u + 0x4DF908u);
}

TEST(a_client_without_the_getters_or_with_getters_that_disagree_is_left_alone)
{
    CHECK(LocatePlayOnline(NetImage{}.Text()).problem == LocateProblem::NoPlayOnline);
    NetImage apart = PlayOnlineClient();
    apart.Put(0xF6F01, {0x0C}); // the envelope's count read from another global
    CHECK(LocatePlayOnline(apart.Text()).problem == LocateProblem::AmbiguousPlayOnline);
    NetImage twice = PlayOnlineClient();
    twice.Put(0x150000, {0xA1, 0x08, 0xF9, 0x99, 0x03, 0x85, 0xC0, 0x74, 0x05, 0x66, 0x8B, 0x40, 0x0E, 0xC3});
    CHECK(LocatePlayOnline(twice.Text()).problem == LocateProblem::AmbiguousPlayOnline);
}

TEST(playonline_counts_are_taken_as_the_network_window_shows_them)
{
    // A message and a friend request beside the messenger, nothing beside the envelope (the test server, 2026-10-26).
    CHECK(PlayOnlineFrom(2, 0) == (PlayOnlineCounts{2, 0}));
    CHECK(PlayOnlineFrom(250, 3) == (PlayOnlineCounts{200, 3})); // the window shows at most 200 messages
}
