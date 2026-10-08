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
