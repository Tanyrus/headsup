#include "code_patch.h"
#include "test.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>
#include <vector>

using namespace headsup;

namespace
{
    // The client as /hu codedump found it on 2026-10-07 at 0x034C0000 (RVAs 0x1000 below their offsets in that file),
    // and HeadsUp's gates in a8e18d4's headsup.dll at its preferred base 0x6CF80000.
    constexpr uint32_t kClientBase  = 0x034C0000;
    constexpr uint32_t kWindowCall  = kClientBase + 0x15F729, kWindowDraw = kClientBase + 0x1177D0;
    constexpr uint32_t kNameHook    = kClientBase + 0x0863E1;
    constexpr uint32_t kTargetArrow = kClientBase + 0x150555, kPickedArrow = kClientBase + 0x150591;
    constexpr uint32_t kGuessCall   = kClientBase + 0x00191B, kMouseOff = kClientBase + 0x123D7C;
    constexpr uint32_t kNameGate = 0x6CFE1CC0, kArrowGate = 0x6CFE1D20, kWindowGate = 0x6CFE1D40;
    constexpr uint32_t kWindowRect = 0x6CFE1E10, kSystemMetric = 0x6CFE1EC0, kMouseOffUnlessKept = 0x6CFE2650;

    using Bytes = std::vector<uint8_t>;

    Bytes Held(const uint8_t* at, uint32_t count) { return Bytes(at, at + count); }

    // The draw loop around its call to the window draw: mov ecx,esi; call 0x1177D0; mov eax,[esp+0Ch].
    Bytes WindowLoop() { return {0x8B, 0xCE, 0xE8, 0xA2, 0x80, 0xFB, 0xFF, 0x8B, 0x44, 0x24, 0x0C}; }

    // The pointer fix's first guess, call [GetWindowRect] and mov ebp,[GetSystemMetrics], then the call that hides the
    // pointer after a key's command.
    Bytes PointerCode()
    {
        return {0xFF, 0x15, 0xE4, 0x93, 0x7E, 0x03, 0x8B, 0x2D, 0xA0, 0x93, 0x7E, 0x03, 0xE8, 0xEF, 0x21, 0x00, 0x00};
    }
    constexpr size_t kLoadAt = 6, kMouseOffAt = 12;
    std::array<CodePatch, 3> PointerPatches(Bytes& code)
    {
        return {Instruction(code.data(), 6, 0xE8, BranchOperand(kGuessCall, kWindowRect)),
            Instruction(code.data() + kLoadAt, 6, 0xBD, kSystemMetric),
            Instruction(code.data() + kMouseOffAt, 5, 0xE8, BranchOperand(kMouseOff, kMouseOffUnlessKept))};
    }

    // The target window's two arrow calls into the shape draw at 0x11FC80.
    constexpr size_t kPickedAt = kPickedArrow - kTargetArrow;
    const Bytes kTargetOperand = {0x26, 0xF7, 0xFC, 0xFF}, kPickedOperand = {0xEA, 0xF6, 0xFC, 0xFF};
    Bytes ArrowCalls()
    {
        Bytes code(kPickedAt + kBranchLength, 0x90);
        code[0] = code[kPickedAt] = 0xE8;
        std::memcpy(&code[1], kTargetOperand.data(), kTargetOperand.size());
        std::memcpy(&code[kPickedAt + 1], kPickedOperand.data(), kPickedOperand.size());
        return code;
    }
    std::array<CodePatch, 2> ArrowPatches(Bytes& code)
    {
        return {Retarget(code.data(), kTargetArrow, kArrowGate),
            Retarget(code.data() + kPickedAt, kPickedArrow, kArrowGate)};
    }

    // Writes as the plugin does once Windows unprotects the bytes, noting every write tried; Windows refuses any at
    // `locked`, and the tries numbered in `refused` (from 0).
    struct Writer
    {
        std::vector<const uint8_t*> locked;
        std::vector<size_t> refused;
        std::vector<std::pair<const uint8_t*, Bytes>> tried;
        bool operator()(uint8_t* at, const uint8_t* bytes, size_t count)
        {
            const bool refuse = std::find(locked.begin(), locked.end(), at) != locked.end() ||
                                std::find(refused.begin(), refused.end(), tried.size()) != refused.end();
            tried.emplace_back(at, Bytes(bytes, bytes + count));
            if (refuse) return false;
            std::memcpy(at, bytes, count);
            return true;
        }
    };
}

TEST(the_help_bars_call_goes_to_the_gate_by_its_operand_alone)
{
    Bytes loop           = WindowLoop();
    const CodePatch call = Retarget(loop.data() + 2, kWindowCall, kWindowGate);
    CHECK(call.at == loop.data() + 3);
    CHECK_EQ(call.bytes, 4u);
    CHECK(Held(call.ours, call.bytes) == (Bytes{0x12, 0x26, 0x9C, 0x69})); // 0x6CFE1D40 - 0x0361F72E
    // Sent back to the window draw behind it, the call holds the dump's own operand.
    const CodePatch back = Retarget(loop.data() + 2, kWindowCall, kWindowDraw);
    CHECK(Held(back.ours, back.bytes) == Held(loop.data() + 3, 4));
}

TEST(an_instruction_is_its_opcode_and_operand_then_nops)
{
    uint8_t site[]       = {0x33, 0xD2, 0x33, 0xED, 0x33, 0xF6}; // the name routine's, kNameHookBytes
    const CodePatch jump = Instruction(site, sizeof(site), 0xE9, BranchOperand(kNameHook, kNameGate));
    CHECK(jump.at == site);
    CHECK(Held(jump.ours, jump.bytes) == (Bytes{0xE9, 0xDA, 0xB8, 0xA9, 0x69, 0x90}));
    // The guess's call goes to HeadsUp's, its load loads HeadsUp's address (mov ebp, imm32), and the call after a
    // key, as long as the game's, needs no nop.
    Bytes code               = PointerCode();
    const CodePatch rect     = Instruction(code.data(), 6, 0xE8, BranchOperand(kGuessCall, kWindowRect));
    const CodePatch metric   = Instruction(code.data() + kLoadAt, 6, 0xBD, kSystemMetric);
    const CodePatch mouseOff =
        Instruction(code.data() + kMouseOffAt, 5, 0xE8, BranchOperand(kMouseOff, kMouseOffUnlessKept));
    CHECK(Held(rect.ours, rect.bytes) == (Bytes{0xE8, 0xF0, 0x04, 0xB2, 0x69, 0x90}));
    CHECK(Held(metric.ours, metric.bytes) == (Bytes{0xBD, 0xC0, 0x1E, 0xFE, 0x6C, 0x90}));
    CHECK(Held(mouseOff.ours, mouseOff.bytes) == (Bytes{0xE8, 0xCF, 0xE8, 0x9F, 0x69}));
}

TEST(applying_writes_ours_and_keeps_the_games_bytes)
{
    Bytes loop     = WindowLoop();
    CodePatch call = Retarget(loop.data() + 2, kWindowCall, kWindowGate);
    Writer write;
    CHECK(Apply({&call, 1}, write) == Applied::All);
    CHECK(loop == (Bytes{0x8B, 0xCE, 0xE8, 0x12, 0x26, 0x9C, 0x69, 0x8B, 0x44, 0x24, 0x0C}));
    CHECK(Held(call.game, call.bytes) == (Bytes{0xA2, 0x80, 0xFB, 0xFF}));
    CHECK_EQ(write.tried.size(), size_t{1});
}

TEST(restoring_puts_the_games_bytes_back_while_ours_are_still_there)
{
    Bytes loop     = WindowLoop();
    CodePatch call = Retarget(loop.data() + 2, kWindowCall, kWindowGate);
    Writer write;
    CHECK(Apply({&call, 1}, write) == Applied::All && loop != WindowLoop());
    CHECK(Restore({&call, 1}, write) == Restored::All);
    CHECK(loop == WindowLoop());
}

TEST(bytes_another_tool_wrote_over_ours_are_left_as_they_are_and_said)
{
    Bytes code                       = ArrowCalls();
    std::array<CodePatch, 2> patches = ArrowPatches(code);
    Writer write;
    CHECK(Apply(patches, write) == Applied::All && patches[0].at == code.data() + 1);
    const Bytes theirs = {0x78, 0x56, 0x34, 0x12}; // to its own gate, which may still call on into HeadsUp's
    std::memcpy(patches[0].at, theirs.data(), theirs.size());
    write.tried.clear();
    CHECK(Restore(patches, write) == Restored::ChangedSince);
    CHECK(Held(patches[0].at, 4) == theirs);
    CHECK(Held(patches[1].at, 4) == kPickedOperand);
    CHECK_EQ(write.tried.size(), size_t{1});
}

TEST(a_patch_windows_keeps_from_going_back_is_said_and_the_last_failure_says_why)
{
    Bytes code                       = ArrowCalls();
    std::array<CodePatch, 2> patches = ArrowPatches(code);
    Writer write;
    CHECK(Apply(patches, write) == Applied::All && patches[1].at == code.data() + kPickedAt + 1);
    write.locked = {patches[1].at};
    CHECK(Restore(patches, write) == Restored::NotWritten);
    CHECK(Held(patches[0].at, 4) == kTargetOperand);

    for (const bool changedFirst : {true, false})
    {
        Bytes again                     = ArrowCalls();
        std::array<CodePatch, 2> hooked = ArrowPatches(again);
        Writer locks;
        CHECK(Apply(hooked, locks) == Applied::All && hooked[0].at == again.data() + 1);
        hooked[changedFirst ? 0 : 1].at[0] ^= 0xFF;
        locks.locked = {hooked[changedFirst ? 1 : 0].at};
        CHECK(Restore(hooked, locks) == (changedFirst ? Restored::NotWritten : Restored::ChangedSince));
    }
}

TEST(patches_go_in_all_or_none_and_the_written_come_out_last_first)
{
    Bytes code                       = PointerCode();
    std::array<CodePatch, 3> patches = PointerPatches(code);
    Writer write;
    write.locked = {code.data() + kMouseOffAt};
    CHECK(Apply(patches, write) == Applied::None);
    CHECK(code == PointerCode());
    CHECK_EQ(write.tried.size(), size_t{5});
    const uint8_t* order[] = {code.data(), code.data() + kLoadAt, code.data() + kMouseOffAt, code.data() + kLoadAt,
        code.data()};
    for (size_t i = 0; i < 5; ++i)
        CHECK(write.tried[i].first == order[i]);
    CHECK(write.tried[3].second == Held(PointerCode().data() + kLoadAt, 6));
    CHECK(write.tried[4].second == Held(PointerCode().data(), 6));
}

TEST(another_tool_keeping_our_jump_but_changing_its_operand_or_its_nop_is_left_alone)
{
    // Chaining onto a hook by its operand alone keeps the opcode; the nop after it can change on its own too.
    for (const auto& [from, to] : {std::pair{1u, 5u}, std::pair{5u, 6u}})
    {
        uint8_t site[]  = {0x33, 0xD2, 0x33, 0xED, 0x33, 0xF6};
        CodePatch jump  = Instruction(site, sizeof(site), 0xE9, BranchOperand(kNameHook, kNameGate));
        Writer write;
        CHECK(Apply({&jump, 1}, write) == Applied::All && site[0] == 0xE9);
        for (uint32_t i = from; i < to; ++i)
            site[i] ^= 0x5A;
        const Bytes theirs = Held(site, sizeof(site));
        write.tried.clear();
        CHECK(Restore({&jump, 1}, write) == Restored::ChangedSince);
        CHECK(Held(site, sizeof(site)) == theirs);
        CHECK(write.tried.empty());
    }
}

TEST(a_refused_way_back_leaves_ours_in_the_client_and_says_so)
{
    Bytes code                       = PointerCode();
    std::array<CodePatch, 3> patches = PointerPatches(code);
    Writer write;
    write.locked  = {code.data() + kMouseOffAt};
    write.refused = {3}; // the load's way back, the first try after the key call's refusal
    CHECK(Apply(patches, write) == Applied::Partly);
    CHECK(Held(code.data(), 6) == Held(PointerCode().data(), 6));
    CHECK(Held(code.data() + kLoadAt, 6) == Held(patches[1].ours, 6));
    CHECK(Held(code.data() + kMouseOffAt, 5) == Held(PointerCode().data() + kMouseOffAt, 5));
    CHECK_EQ(write.tried.size(), size_t{5}); // the call still went back after the load's refusal
}
