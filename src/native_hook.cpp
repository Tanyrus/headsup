#include "native_hook.h"

#include "Ashita.h"
#include "name_frame.h"
#include "native_locate.h"
#include "tracker.h"

#ifdef HEADSUP_DEV
#include "namedump.h"
#endif

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

extern "C"
{
    uintptr_t g_HeadsUpNameResume = 0;
    uintptr_t g_HeadsUpNameExit   = 0;
    void HeadsUpNameGate();
    __attribute__((used)) uint32_t HeadsUpOnName(uint32_t frame);
    uintptr_t g_HeadsUpArrowDraw = 0;
    void HeadsUpArrowGate();
    __attribute__((used)) uint32_t HeadsUpOnArrow(const uint32_t* arguments, uint32_t returnAddress);
    BOOL __stdcall HeadsUpWindowRect(HWND window, RECT* rect);
    int __stdcall HeadsUpSystemMetric(int index);
}

// Entered by a jump from the routine's hook, so esp is the routine's frame once the flags and registers saved here are
// skipped. The game's x87 stack is not empty there and HeadsUp's code needs it empty, hence fxsave and fninit (keeping
// the game's control word). On 0 it runs the routine's own xor edx,edx; xor ebp,ebp; xor esi,esi (kNameHookBytes),
// which the jump replaced, before resuming.
asm(R"(
    .text
    .p2align 4
    .globl _HeadsUpNameGate
_HeadsUpNameGate:
    .intel_syntax noprefix
    pushfd
    pushad
    mov eax, esp
    lea ecx, [eax + 36]
    sub esp, 544
    and esp, 0xFFFFFFF0
    mov [esp + 512], eax
    fxsave [esp]
    fninit
    fldcw [esp]
    cld
    push ecx
    call _HeadsUpOnName
    add esp, 4
    mov [esp + 516], eax
    fxrstor [esp]
    mov eax, [esp + 516]
    mov esp, [esp + 512]
    test eax, eax
    jz .LHeadsUpNameResume
    popad
    popfd
    jmp dword ptr [_g_HeadsUpNameExit]
.LHeadsUpNameResume:
    popad
    popfd
    xor edx, edx
    xor ebp, ebp
    xor esi, esi
    jmp dword ptr [_g_HeadsUpNameResume]
    .att_syntax prefix
)");

// Called in place of the shape draw, a thiscall: ecx is the arrow's shape and the stack holds the return address, then
// the eight arguments. A call boundary, so the x87 stack is empty. On 0 it jumps on to the shape draw with all of that
// as it was; otherwise it returns and pops the arguments as the shape draw would (kArrowArgumentBytes).
asm(R"(
    .text
    .p2align 4
    .globl _HeadsUpArrowGate
_HeadsUpArrowGate:
    .intel_syntax noprefix
    push ecx
    lea eax, [esp + 8]
    push dword ptr [esp + 4]
    push eax
    call _HeadsUpOnArrow
    add esp, 8
    pop ecx
    test eax, eax
    jnz .LHeadsUpArrowSkip
    jmp dword ptr [_g_HeadsUpArrowDraw]
.LHeadsUpArrowSkip:
    ret 32
    .att_syntax prefix
)");

namespace headsup
{
    namespace
    {
        constexpr const char* kGameModule = "FFXiMain.dll";
        constexpr char kCodeSection[]     = ".text";
        constexpr char kConstSection[]    = ".rdata";
        constexpr uint8_t kJumpOpcode     = 0xE9;
        constexpr uint8_t kNop            = 0x90;
        constexpr uint32_t kJumpLength    = 5;
        constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

        static_assert(sizeof(kNameHookBytes) == kJumpLength + 1, "the hook is a jump and one nop");
        static_assert(kArrowArgumentBytes == 32, "the arrow gate's ret 32");
        constexpr uint32_t kCallLength        = 5;
        constexpr uint32_t kArrowColorArgument = 4; // x, y, scale x, scale y, then the color

        // Written at Present and read by OnArrow, both on the render thread, so never at once.
        uint32_t g_PickedReturn = 0; // where the candidate's arrow call returns to
        bool g_HideArrows       = false;
        uint32_t g_PickedColor  = 0;
        bool g_FixPointer       = false;
        uintptr_t g_ControllerGlobal = 0; // where the game keeps its mouse controller
        uintptr_t g_ShowPointer      = 0; // the controller's thiscall that shows (1) or hides (0) the pointer
        using ShowPointerFn          = void(__attribute__((thiscall))*)(uintptr_t controller, int shown);

        constexpr uint8_t kCallOpcode = 0xE8;
        constexpr uint32_t kGuessBytes = 6; // a call through the import table, or a load from it into a register

        // Return addresses of the calls into the routine, as loaded; set before the hook goes in and fixed while it is.
        std::vector<uint32_t> g_CallerReturns;
        uint32_t g_EntityCaller = 0; // the one of them entities' names come back to

        // Written at Present and read by OnName, both on the render thread, so never at once.
        IDirect3DDevice8* g_Device = nullptr;
        std::vector<NameOwner> g_Owners; // by actor
        NameFrame g_Drawn[kMaxEntities];
        uint32_t g_DrawnFrame[kMaxEntities] = {};
        uint32_t g_Frame                    = 1;
        NameStats g_Stats;
        IDirect3DSurface8* g_SceneTarget    = nullptr; // identities only: the game owns them
        IDirect3DSurface8* g_SceneDepth     = nullptr;

        const NameOwner* FindOwner(uint32_t actor)
        {
            const auto it = std::lower_bound(g_Owners.begin(), g_Owners.end(), actor,
                [](const NameOwner& owner, uint32_t value) { return owner.actor < value; });
            return it != g_Owners.end() && it->actor == actor && it->index < kMaxEntities ? &*it : nullptr;
        }

        // Where the game is drawing names right now: the scene's image and depth buffer.
        void NoteScene()
        {
            if (g_SceneTarget != nullptr || g_Device == nullptr) return;
            IDirect3DSurface8* target = nullptr;
            IDirect3DSurface8* depth  = nullptr;
            if (FAILED(g_Device->GetRenderTarget(&target)) || target == nullptr) return;
            if (FAILED(g_Device->GetDepthStencilSurface(&depth))) depth = nullptr;
            g_SceneTarget = target;
            g_SceneDepth  = depth;
            target->Release();
            if (depth != nullptr) depth->Release();
        }

        bool Readable(const uint8_t* at, size_t bytes)
        {
            while (bytes > 0)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (VirtualQuery(at, &info, sizeof(info)) != sizeof(info)) return false;
                if (info.State != MEM_COMMIT || (info.Protect & kReadable) == 0 || (info.Protect & PAGE_GUARD) != 0)
                    return false;
                const auto* end   = static_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize;
                const size_t step = std::min(bytes, static_cast<size_t>(end - at));
                at += step;
                bytes -= step;
            }
            return true;
        }

        bool FindSection(const uint8_t* base, const char* name, ImageSection& section)
        {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
            const auto* header = IMAGE_FIRST_SECTION(nt);
            for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++header)
            {
                if (std::strncmp(reinterpret_cast<const char*>(header->Name), name, sizeof(header->Name)) != 0) continue;
                section = {header->VirtualAddress, base + header->VirtualAddress, header->Misc.VirtualSize};
                return Readable(section.bytes, section.size);
            }
            return false;
        }

        bool WriteCode(uint8_t* at, const uint8_t* bytes, size_t count)
        {
            DWORD old = 0;
            if (!VirtualProtect(at, count, PAGE_EXECUTE_READWRITE, &old)) return false;
            std::memcpy(at, bytes, count);
            FlushInstructionCache(GetCurrentProcess(), at, count);
            VirtualProtect(at, count, old, &old);
            return true;
        }
    }

    std::span<const uint32_t> NameHook::CallerReturns() const { return g_CallerReturns; }

    void NameHook::SetDevice(IDirect3DDevice8* device) { g_Device = device; }

    void NameHook::NewFrame(std::vector<NameOwner> owners)
    {
        ++g_Frame;
        g_Stats       = {};
        g_SceneTarget = nullptr;
        g_SceneDepth  = nullptr;
        std::sort(owners.begin(), owners.end(), [](const NameOwner& a, const NameOwner& b) { return a.actor < b.actor; });
        g_Owners = std::move(owners);
    }

    const NameFrame* NameHook::Drawn(uint16_t index) const
    {
        return index < kMaxEntities && g_DrawnFrame[index] == g_Frame ? &g_Drawn[index] : nullptr;
    }

    NameStats NameHook::Stats() const { return g_Stats; }

    IDirect3DSurface8* NameHook::SceneTarget() const { return g_SceneTarget; }
    IDirect3DSurface8* NameHook::SceneDepth() const { return g_SceneDepth; }

    std::string NameHook::Start()
    {
        if (Running()) return "";
        const auto* base = reinterpret_cast<const uint8_t*>(GetModuleHandleA(kGameModule));
        if (base == nullptr) return std::string(kGameModule) + " is not loaded";
        ImageSection text{}, rdata{};
        if (!FindSection(base, kCodeSection, text) || !FindSection(base, kConstSection, rdata))
            return "the client's code could not be read";
        const uint32_t moduleBase  = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base));
        const NameRoutine routine = LocateNameRoutine(moduleBase, text, rdata);
        if (routine.problem != LocateProblem::None) return Describe(routine.problem);

        auto* site = const_cast<uint8_t*>(base) + routine.hook;
        if (std::memcmp(site, kNameHookBytes, sizeof(kNameHookBytes)) != 0) return Describe(LocateProblem::PatchSiteChanged);
        g_CallerReturns.clear();
        for (const uint32_t rva : routine.callerReturns)
            g_CallerReturns.push_back(moduleBase + rva);
        g_EntityCaller      = moduleBase + routine.entityCaller;
        g_HeadsUpNameResume = moduleBase + routine.resume;
        g_HeadsUpNameExit   = moduleBase + routine.exit;

        const int32_t jump = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&HeadsUpNameGate) -
                                                  (reinterpret_cast<uintptr_t>(site) + kJumpLength));
        m_Patch[0] = kJumpOpcode;
        std::memcpy(m_Patch + 1, &jump, sizeof(jump));
        m_Patch[kJumpLength] = kNop;
        if (!WriteCode(site, m_Patch, sizeof(m_Patch))) return "the client's code could not be changed";
        m_Site       = site;
        m_ModuleBase = moduleBase;
        return "";
    }

    std::string NameHook::Stop()
    {
        if (!Running()) return "";
        uint8_t* site = std::exchange(m_Site, nullptr);
        if (std::memcmp(site, m_Patch, sizeof(m_Patch)) != 0)
            return "the game's name routine was changed again after HeadsUp hooked it, so it was left as it is";
        if (!WriteCode(site, kNameHookBytes, sizeof(kNameHookBytes))) return "the game's name routine could not be put back";
        return "";
    }
}

namespace headsup
{
    std::string ArrowHook::Start()
    {
        if (Running()) return "";
        auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleA(kGameModule));
        if (base == nullptr) return std::string(kGameModule) + " is not loaded";
        ImageSection text{};
        if (!FindSection(base, kCodeSection, text)) return "the client's code could not be read";
        const ArrowCalls calls = LocateArrowCalls(text);
        if (calls.problem != LocateProblem::None) return Describe(calls.problem);
        const auto moduleBase   = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(base));
        const auto gate         = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&HeadsUpArrowGate));
        const uint32_t sites[2] = {calls.target, calls.picked};
        uint8_t* operands[2]    = {base + calls.target + 1, base + calls.picked + 1};
        for (int i = 0; i < 2; ++i)
        {
            std::memcpy(&m_Operands[i], operands[i], sizeof(uint32_t));
            m_Patched[i] = gate - (moduleBase + sites[i] + kCallLength);
        }
        g_HeadsUpArrowDraw = moduleBase + calls.draw;
        g_PickedReturn     = moduleBase + calls.picked + kCallLength;
        for (int i = 0; i < 2; ++i)
        {
            if (WriteCode(operands[i], reinterpret_cast<const uint8_t*>(&m_Patched[i]), sizeof(uint32_t))) continue;
            if (i > 0) WriteCode(operands[0], reinterpret_cast<const uint8_t*>(&m_Operands[0]), sizeof(uint32_t));
            return "the client's code could not be changed";
        }
        m_Calls[0] = operands[0];
        m_Calls[1] = operands[1];
        return "";
    }

    std::string ArrowHook::Stop()
    {
        if (!Running()) return "";
        std::string failure;
        for (int i = 0; i < 2; ++i)
        {
            uint8_t* operand = std::exchange(m_Calls[i], nullptr);
            if (std::memcmp(operand, &m_Patched[i], sizeof(uint32_t)) != 0)
                failure = "the game's target arrows were changed again after HeadsUp hooked them, so they were left as they are";
            else if (!WriteCode(operand, reinterpret_cast<const uint8_t*>(&m_Operands[i]), sizeof(uint32_t)))
                failure = "the game's target arrows could not be put back";
        }
        return failure;
    }

    void ArrowHook::Hide(bool hide) { g_HideArrows = hide; }
    uint32_t ArrowHook::PickedColor() const { return g_PickedColor; }
    void ArrowHook::ForgetPicked() { g_PickedColor = 0; }
}

namespace headsup
{
    std::string PointerFix::Start()
    {
        if (Running()) return "";
        auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleA(kGameModule));
        if (base == nullptr) return std::string(kGameModule) + " is not loaded";
        ImageSection text{};
        if (!FindSection(base, kCodeSection, text)) return "the client's code could not be read";
        const PointerMapping mapping = LocatePointerMapping(text);
        if (mapping.problem != LocateProblem::None) return Describe(mapping.problem);
        const PointerShow show = LocatePointerShow(text);
        if (show.problem != LocateProblem::None) return Describe(show.problem);
        const auto windowRect = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&HeadsUpWindowRect));
        const auto metric     = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&HeadsUpSystemMetric));
        std::vector<Patch> patches;
        for (const WindowGuess& guess : mapping.guesses)
        {
            Patch call{base + guess.windowRectCall, {}, {}};
            const uint32_t to = windowRect - static_cast<uint32_t>(reinterpret_cast<uintptr_t>(call.at) + kCallLength);
            call.ours[0] = kCallOpcode;
            std::memcpy(call.ours + 1, &to, sizeof(to));
            call.ours[kGuessBytes - 1] = kNop;
            Patch load{base + guess.metricsLoad, {}, {}};
            load.ours[0] = guess.movImmediate;
            std::memcpy(load.ours + 1, &metric, sizeof(metric));
            load.ours[kGuessBytes - 1] = kNop;
            patches.push_back(call);
            patches.push_back(load);
        }
        for (size_t i = 0; i < patches.size(); ++i)
        {
            std::memcpy(patches[i].game, patches[i].at, kGuessBytes);
            if (WriteCode(patches[i].at, patches[i].ours, kGuessBytes)) continue;
            while (i-- > 0)
                WriteCode(patches[i].at, patches[i].game, kGuessBytes);
            return "the client's code could not be changed";
        }
        m_Patches          = std::move(patches);
        g_ControllerGlobal = show.controllerGlobal;
        g_ShowPointer      = reinterpret_cast<uintptr_t>(base) + show.show;
        return "";
    }

    std::string PointerFix::Stop()
    {
        std::string failure;
        for (const Patch& patch : m_Patches)
        {
            if (std::memcmp(patch.at, patch.ours, kGuessBytes) != 0)
                failure = "the game's reading of the mouse was changed again after HeadsUp fixed it, so it was left as it is";
            else if (!WriteCode(patch.at, patch.game, kGuessBytes))
                failure = "the game's reading of the mouse could not be put back";
        }
        m_Patches.clear();
        g_ControllerGlobal = g_ShowPointer = 0;
        return failure;
    }

    void PointerFix::Enable(bool on) { g_FixPointer = on; }

    void PointerFix::RevealUnderWindow()
    {
        if (!g_FixPointer || g_ControllerGlobal == 0 || g_ShowPointer == 0) return;
        const uintptr_t controller = *reinterpret_cast<const uintptr_t*>(g_ControllerGlobal);
        if (controller == 0 || *reinterpret_cast<const uint8_t*>(controller + kPointerShownOffset) != 0) return;
        reinterpret_cast<ShowPointerFn>(g_ShowPointer)(controller, 1);
    }
}

// In place of the game's GetWindowRect where it guesses its client area: the client area itself, in screen pixels.
extern "C" BOOL __stdcall HeadsUpWindowRect(HWND window, RECT* rect)
{
    RECT client{};
    if (!headsup::g_FixPointer || !GetClientRect(window, &client)) return GetWindowRect(window, rect);
    POINT corners[2] = {{client.left, client.top}, {client.right, client.bottom}};
    MapWindowPoints(window, nullptr, corners, 2);
    *rect = RECT{corners[0].x, corners[0].y, corners[1].x, corners[1].y};
    return TRUE;
}

// And in place of its GetSystemMetrics there, asked only for the borders and caption it then subtracts: none.
extern "C" int __stdcall HeadsUpSystemMetric(int index) { return headsup::g_FixPointer ? 0 : GetSystemMetrics(index); }

// On the game's stack, inside the target window's draw: nothing here may allocate, throw or call Ashita.
extern "C" uint32_t HeadsUpOnArrow(const uint32_t* arguments, uint32_t returnAddress)
{
    using namespace headsup;
    if (returnAddress == g_PickedReturn) g_PickedColor = arguments[kArrowColorArgument];
    return g_HideArrows ? 1 : 0;
}

// On the game's stack, inside its name routine: nothing here may allocate, throw or call Ashita.
extern "C" uint32_t HeadsUpOnName(uint32_t frame)
{
    using namespace headsup;
    const auto* bytes          = reinterpret_cast<const uint8_t*>(frame);
    const NameFrame name       = ReadNameFrame(bytes);
    const bool fromRoutine = FromNameRoutine(name, g_CallerReturns);
    const auto* text       = fromRoutine ? reinterpret_cast<const uint8_t*>(name.text) : nullptr;
#ifdef HEADSUP_DEV
    CaptureNameCall(bytes, text, name.length);
#endif
    // The other caller draws world labels, whose frames hold no actor: whatever is there is stale.
    if (!fromRoutine || name.caller != g_EntityCaller || !Placeable(name) || !SingleLine(text, name.length)) return 0;
    const NameOwner* owner = FindOwner(name.actor);
    if (owner == nullptr) return 0;
    NoteScene();
    g_Drawn[owner->index]      = name;
    g_DrawnFrame[owner->index] = g_Frame;
    ++g_Stats.names;
    if (!owner->replace) return 0;
    ++g_Stats.replaced;
    return 1;
}
