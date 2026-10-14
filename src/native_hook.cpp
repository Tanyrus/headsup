#include "native_hook.h"

#include "Ashita.h"
#include "bytes.h"
#include "name_frame.h"
#include "native_locate.h"
#include "pointer_keys.h"
#include "tracker.h"

#ifdef HEADSUP_DEV
#include "namedump.h"
#endif

#include <algorithm>
#include <climits>
#include <array>
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
    uintptr_t g_HeadsUpWindowDraw = 0;
    void HeadsUpWindowGate();
    __attribute__((used)) uint32_t HeadsUpOnWindowDraw(uint32_t mcd);
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

// Called in place of the menu window draw, a thiscall with ecx the window's MCD and no stack arguments. On 0 it jumps
// on to the window draw with ecx as it was; otherwise it returns without drawing, and with no arguments to pop.
asm(R"(
    .text
    .p2align 4
    .globl _HeadsUpWindowGate
_HeadsUpWindowGate:
    .intel_syntax noprefix
    push ecx
    push ecx
    call _HeadsUpOnWindowDraw
    add esp, 4
    pop ecx
    test eax, eax
    jnz .LHeadsUpWindowSkip
    jmp dword ptr [_g_HeadsUpWindowDraw]
.LHeadsUpWindowSkip:
    ret
    .att_syntax prefix
)");

namespace headsup
{
    namespace
    {
        constexpr const char* kGameModule = "FFXiMain.dll";
        constexpr char kCodeSection[]     = ".text";
        constexpr char kConstSection[]    = ".rdata";
        constexpr char kDataSection[]     = ".data";
        constexpr char kCodeNotRead[]     = "the client's code could not be read";
        constexpr char kCodeNotChanged[]  = "the client's code could not be changed";
        constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

        template <typename T>
        uint32_t Address(T* at)
        {
            return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(at));
        }

        static_assert(sizeof(kNameHookBytes) == kBranchLength + 1, "the hook is a jump and one nop");
        static_assert(kArrowArgumentBytes == 32, "the arrow gate's ret 32");
        constexpr uint32_t kArrowColorArgument = 4; // x, y, scale x, scale y, then the color

        // Written at Present and read by OnArrow, both on the render thread, so never at once.
        uint32_t g_PickedReturn = 0; // where the candidate's arrow call returns to
        bool g_HideArrows       = false;
        HiddenWindows g_HiddenWindows;
        std::optional<HelpBand> g_HelpBand; // windows are moved out from under it while it is set
        uint32_t g_TitleText = 0;           // the strip title's text object in its owner; 0 when it was not found
        PlacedText g_TitlePlace;
        WindowKeyList g_MovedWindows;       // those the player picked
        WindowKeyList g_SeenWindows;        // those that opened since HeadsUp loaded
        struct KeyOffset
        {
            WindowKey key;
            int x, y;
        };
        std::array<KeyOffset, kMaxWindowKeys> g_Offsets{}; // the player's, by window
        size_t g_OffsetCount = 0;
        WindowKeyList g_DrawnWindows; // those that drew since TakeOpen last ran
        WindowPlacements g_Placements;      // where each moved window came from and went
        uint32_t g_WindowFrame = 0;         // counts WindowHook::NewFrame
        int g_StripBottom = 0;              // the game's strip's lowest edge, 0 until its windows are seen
        uintptr_t g_WindowMoveTo = 0;       // MCD::MoveTo, or 0 when it was not found
        using WindowMoveToFn     = void(__attribute__((thiscall))*)(uintptr_t mcd, int x, int y);
        uint32_t g_PickedColor  = 0;
        uintptr_t g_RangeColor  = 0; // the window's thiscall that picks the candidate arrow's color
        using RangeColorFn      = uint32_t(__attribute__((thiscall))*)(const void* window, uintptr_t you, uintptr_t candidate,
            uint32_t inRange, uint32_t outOfRange, uint32_t third);
        bool g_FixPointer       = false;
        uintptr_t g_ControllerGlobal = 0; // where the game keeps its mouse controller
        uintptr_t g_ShowPointer      = 0; // the controller's thiscall that shows (1) or hides (0) the pointer
        using ShowPointerFn          = void(__attribute__((thiscall))*)(uintptr_t controller, int shown);
        uintptr_t g_MouseUse         = 0; // the controller's thiscall that turns mouse use on (1) or off (0)
        using MouseUseFn             = void(__attribute__((thiscall))*)(uintptr_t controller, int on);
        std::vector<uint8_t> g_KeepKeys;

        // In place of the game's call that turns mouse use off after a key set off a command.
        __attribute__((thiscall)) void MouseOffUnlessKept(uintptr_t controller, int on)
        {
            const auto down = [](uint8_t vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
            if (on == 0 && !g_KeepKeys.empty() && KeepsPointer(HeldKeys(down), g_KeepKeys)) return;
            reinterpret_cast<MouseUseFn>(g_MouseUse)(controller, on);
        }

        constexpr uint32_t kGuessBytes = 6; // a call through the import table, or a load from it into a register
        static_assert(kGuessBytes >= kBranchLength && kGuessBytes <= CodePatch::kMaxBytes &&
                          sizeof(kNameHookBytes) <= CodePatch::kMaxBytes,
            "each patch fits a CodePatch");

        // Return addresses of the calls into the routine, as loaded; set before the hook goes in and fixed while it is.
        std::vector<uint32_t> g_CallerReturns;
        uint32_t g_EntityCaller = 0; // the one of them entities' names come back to

        // Written at Present and read by OnName, both on the render thread, so never at once.
        IDirect3DDevice8* g_Device = nullptr;
        std::vector<NameOwner> g_Owners; // by actor
        bool g_HideNames = false;        // every name goes, whatever its owner's action: a clean screenshot's frame
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

        const uint8_t* Pointer(uint32_t address)
        {
            return reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(address));
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

        // Whether count bytes at at lie in the readable image at base, whose headers FindSection has checked.
        bool InImage(const uint8_t* base, const uint8_t* at, size_t count)
        {
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
                base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
            return at >= base && at + count <= base + nt->OptionalHeader.SizeOfImage && Readable(at, count);
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

        // A patch Stop could not take back, or another tool's patch over it that may call on into HeadsUp's gate,
        // would send the game into a freed DLL: the DLL stays loaded until the game closes, its gates passing all on.
        std::string KeptLoaded(std::string failure)
        {
            if (failure.empty()) return failure;
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(&g_HiddenWindows), &self);
            return failure + "; HeadsUp stays in memory until the game closes";
        }

        // Why the game's bytes did not all go back, in the hook's words, or an empty string.
        std::string PutBack(std::span<const CodePatch> patches, const char* changed, const char* unwritten)
        {
            switch (Restore(patches, WriteCode))
            {
            case Restored::All: return "";
            case Restored::ChangedSince: return changed;
            case Restored::NotWritten: return unwritten;
            }
            return "";
        }

        // Why the patches did not go in, or an empty string.
        std::string PutIn(std::span<CodePatch> patches)
        {
            switch (Apply(patches, WriteCode))
            {
            case Applied::All: return "";
            case Applied::None: return kCodeNotChanged;
            case Applied::Partly:
                return KeptLoaded(std::string(kCodeNotChanged) + ", and some of it could not be put back");
            }
            return "";
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
        g_Owners    = std::move(owners);
        g_HideNames = false;
    }

    void NameHook::HideAll() { g_HideNames = true; }

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
            return kCodeNotRead;
        const uint32_t moduleBase  = Address(base);
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

        CodePatch jump = Instruction(site, sizeof(kNameHookBytes), kJumpOpcode,
            BranchOperand(Address(site), Address(&HeadsUpNameGate)));
        if (const std::string failure = PutIn({&jump, 1}); !failure.empty()) return failure;
        m_Patch      = jump;
        m_ModuleBase = moduleBase;
        return "";
    }

    std::string NameHook::Stop()
    {
        if (!Running()) return "";
        const CodePatch jump = std::exchange(m_Patch, {});
        g_Owners.clear(); // the gate then resumes the routine for every name
        g_HideNames = false;
        return KeptLoaded(PutBack({&jump, 1},
            "the game's name routine was changed again after HeadsUp hooked it, so it was left as it is",
            "the game's name routine could not be put back"));
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
        if (!FindSection(base, kCodeSection, text)) return kCodeNotRead;
        const ArrowCalls calls = LocateArrowCalls(text);
        if (calls.problem != LocateProblem::None) return Describe(calls.problem);
        const uint32_t moduleBase = Address(base), gate = Address(&HeadsUpArrowGate);
        uint8_t* target = base + calls.target;
        uint8_t* picked = base + calls.picked;
        std::array<CodePatch, 2> patches = {
            Retarget(target, Address(target), gate), Retarget(picked, Address(picked), gate)};
        g_HeadsUpArrowDraw = moduleBase + calls.draw;
        g_PickedReturn     = moduleBase + calls.picked + kBranchLength;
        const RangeColor range = LocateRangeColor(text);
        g_RangeColor           = range.problem == LocateProblem::None ? moduleBase + range.routine : 0;
        if (const std::string failure = PutIn(patches); !failure.empty()) return failure;
        m_Calls = patches;
        return "";
    }

    std::string ArrowHook::Stop()
    {
        g_HideArrows = false; // also after a Start that left one of its calls behind
        if (!Running()) return "";
        const std::array<CodePatch, 2> calls = std::exchange(m_Calls, {});
        return KeptLoaded(PutBack(calls,
            "the game's target arrows were changed again after HeadsUp hooked them, so they were left as they are",
            "the game's target arrows could not be put back"));
    }

    void ArrowHook::Hide(bool hide) { g_HideArrows = hide; }
    uint32_t ArrowHook::PickedColor() const { return g_PickedColor; }
    void ArrowHook::ForgetPicked() { g_PickedColor = 0; }

    uint32_t ArrowHook::CandidateColor(const void* window, uintptr_t yourActor, uintptr_t candidateActor) const
    {
        if (!Running() || g_RangeColor == 0 || window == nullptr || yourActor == 0 || candidateActor == 0) return 0;
        return reinterpret_cast<RangeColorFn>(g_RangeColor)(window, yourActor, candidateActor, kArrowInRange, kArrowOutOfRange,
            kArrowThird);
    }
}

namespace headsup
{
    std::string WindowHook::Start()
    {
        if (Running()) return "";
        auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleA(kGameModule));
        if (base == nullptr) return std::string(kGameModule) + " is not loaded";
        ImageSection text{};
        if (!FindSection(base, kCodeSection, text)) return kCodeNotRead;
        const WindowDraw found = LocateWindowDraw(text);
        if (found.problem != LocateProblem::None) return Describe(found.problem);
        // Without the strip's text objects its title stays where the game draws it when its window moves.
        ImageSection data{};
        const StripSites strip = FindSection(base, kDataSection, data) ? LocateStripText(text, data) : StripSites{};
        g_TitleText            = strip.problem == LocateProblem::None ? strip.titleText : 0;
        uint8_t* site       = base + found.call;
        CodePatch call      = Retarget(site, Address(site), Address(&HeadsUpWindowGate));
        g_HeadsUpWindowDraw = Address(base) + found.draw;
        if (const std::string failure = PutIn({&call, 1}); !failure.empty()) return failure;
        m_Call = call;
        // Without the move the windows stay where the game puts them; hiding works either way.
        const WindowMoveSite move = LocateWindowMove(text);
        g_WindowMoveTo            = move.problem == LocateProblem::None ? Address(base) + move.moveTo : 0;
        m_MoveProblem             = move.problem == LocateProblem::None ? "" : Describe(move.problem);
        return "";
    }

    namespace
    {
        void PutWindowBack(const WindowPlacements::Placed& placed);
    }

    std::string WindowHook::Stop()
    {
        if (!Running()) return "";
        // Windows open now go back where the game put them; those closed since open there anyway.
        if (g_WindowMoveTo != 0)
            for (const WindowPlacements::Placed& placed : g_Placements.MovedSince(g_WindowFrame > 0 ? g_WindowFrame - 1 : 0))
                PutWindowBack(placed);
        g_Placements         = {};
        g_TitlePlace         = {};
        const CodePatch call = std::exchange(m_Call, {});
        g_HiddenWindows      = {};
        g_HelpBand.reset();
        g_StripBottom = 0;
        return KeptLoaded(PutBack({&call, 1},
            "the game's window draw was changed again after HeadsUp hooked it, so it was left as it is",
            "the game's window draw could not be put back"));
    }

    void WindowHook::Hide(HiddenWindows hidden) { g_HiddenWindows = hidden; }

    void WindowHook::NewFrame() { g_Placements.SetFrame(++g_WindowFrame); }

    void WindowHook::MoveFrom(std::optional<HelpBand> band) { g_HelpBand = band; }

    void WindowHook::Offset(const std::vector<WindowOffset>& offsets)
    {
        g_OffsetCount = 0;
        for (const WindowOffset& o : offsets)
            if (const std::optional<WindowKey> key = KeyFromShortName(o.name); key && g_OffsetCount < g_Offsets.size())
                g_Offsets[g_OffsetCount++] = KeyOffset{*key, o.x, o.y};
    }

    void WindowHook::Move(const std::vector<std::string>& windows)
    {
        g_MovedWindows.Clear();
        for (const std::string& name : windows)
            if (const std::optional<WindowKey> key = KeyFromShortName(name)) g_MovedWindows.Add(*key);
    }

    std::vector<std::string> WindowHook::SeenWindows() const
    {
        std::vector<std::string> seen;
        for (size_t i = 0; i < g_SeenWindows.Size(); ++i)
            seen.push_back(ShortName(g_SeenWindows[i]));
        return seen;
    }

    size_t WindowHook::SeenCount() const { return g_SeenWindows.Size(); }

    std::vector<std::string> WindowHook::TakeOpen()
    {
        std::vector<std::string> open;
        for (size_t i = 0; i < g_DrawnWindows.Size(); ++i)
            open.push_back(ShortName(g_DrawnWindows[i]));
        g_DrawnWindows.Clear();
        std::ranges::sort(open);
        return open;
    }
}

namespace headsup
{
    std::string NetStatsReader::Find()
    {
        const auto* base = reinterpret_cast<const uint8_t*>(GetModuleHandleA(kGameModule));
        if (base == nullptr) return std::string(kGameModule) + " is not loaded";
        ImageSection text{};
        if (!FindSection(base, kCodeSection, text)) return kCodeNotRead;
        const NetStats found = LocateNetStats(text);
        if (found.problem != LocateProblem::None) return Describe(found.problem);
        // The image stays mapped while the game runs, so Read reads a global checked here without checking it again.
        if (!InImage(base, Pointer(found.global), sizeof(uint32_t)))
            return "their pointer is not in the game's image";
        m_Global = found.global;
        return "";
    }

    std::string WeatherReader::Find()
    {
        const auto* base = reinterpret_cast<const uint8_t*>(GetModuleHandleA(kGameModule));
        if (base == nullptr) return std::string(kGameModule) + " is not loaded";
        ImageSection text{};
        if (!FindSection(base, kCodeSection, text)) return kCodeNotRead;
        const WeatherSite found = LocateWeather(text);
        if (found.problem != LocateProblem::None) return Describe(found.problem);
        if (!InImage(base, Pointer(found.global), sizeof(uint16_t))) return "the weather is not in the game's image";
        m_Global = found.global;
        return "";
    }

    std::vector<uint32_t> FriendListReader::Read()
    {
        // Each search reads the whole of the game's data: after this many the client is taken to have no such list.
        constexpr int kSearches = 20;
        if (m_First == nullptr && m_Searches < kSearches)
        {
            ++m_Searches;
            const auto* base = reinterpret_cast<const uint8_t*>(GetModuleHandleA(kGameModule));
            ImageSection data{};
            if (base != nullptr && FindSection(base, kDataSection, data))
                if (const std::optional<uint32_t> first = LocateFriendList(data)) m_First = data.bytes + *first;
        }
        return m_First != nullptr ? FriendCharacters(m_First) : std::vector<uint32_t>{};
    }

    std::optional<Weather> WeatherReader::Read() const
    {
        if (m_Global == 0) return std::nullopt;
        return WeatherFromId(ReadAt<uint16_t>(Pointer(m_Global), 0));
    }

    std::optional<NetCounters> NetStatsReader::Read() const
    {
        if (m_Global == 0) return std::nullopt;
        const uint32_t net = ReadAt<uint32_t>(Pointer(m_Global), 0);
        if (net == 0) return std::nullopt;
        // The client keeps them in NetCounters' order: received, sent, the meter.
        const uint8_t* counters = Pointer(net + kNetCounters);
        if (!Readable(counters, sizeof(NetCounters))) return std::nullopt;
        NetCounters raw;
        std::memcpy(&raw, counters, sizeof(raw));
        return CountersFrom(raw.received, raw.sent, raw.health);
    }
}

namespace headsup
{
    std::string StripTextReader::Find()
    {
        const auto* base = reinterpret_cast<const uint8_t*>(GetModuleHandleA(kGameModule));
        if (base == nullptr) return std::string(kGameModule) + " is not loaded";
        ImageSection text{}, data{};
        if (!FindSection(base, kCodeSection, text) || !FindSection(base, kDataSection, data)) return kCodeNotRead;
        const StripSites found = LocateStripText(text, data);
        if (found.problem != LocateProblem::None) return Describe(found.problem);
        // The image stays mapped while the game runs, so the slots checked here are read without checking them again.
        if (!InImage(base, Pointer(found.helpSlot), sizeof(uint32_t)) ||
            !InImage(base, Pointer(found.titleSlot), sizeof(uint32_t)))
            return "their slots are not in the game's image";
        m_Sites = found;
        return "";
    }

    namespace
    {
        bool ProcessReadable(uint32_t at, size_t bytes) { return Readable(Pointer(at), bytes); }
        void ProcessCopy(uint32_t at, void* into, size_t bytes) { std::memcpy(into, Pointer(at), bytes); }
        // Functions, not lambdas: a FunctionRef keeps the address of what it was made from.
        const ProcessMemory kProcessMemory{ProcessReadable, ProcessCopy};
    }

    StripRead StripTextReader::Read(StripWindow window) const
    {
        const uint32_t slot   = window == StripWindow::Help ? m_Sites.helpSlot : m_Sites.titleSlot;
        const uint32_t textAt = window == StripWindow::Help ? m_Sites.helpText : m_Sites.titleText;
        if (slot == 0) return StripRead{0, 0, {}, "not found"};
        StripWindowReader reader;
        StripObject object;
        if (const char* why = reader.Read(slot, textAt, window, kProcessMemory, object); why != nullptr)
            return StripRead{reader.Owner(), 0, {}, why};
        return StripRead{reader.Owner(), reader.Owner() + textAt, {object.begin(), object.end()}, ""};
    }

    bool StripTextReader::Read(StripWindow window, StripObject& into)
    {
        const uint32_t slot   = window == StripWindow::Help ? m_Sites.helpSlot : m_Sites.titleSlot;
        const uint32_t textAt = window == StripWindow::Help ? m_Sites.helpText : m_Sites.titleText;
        return slot != 0 &&
               m_Readers[static_cast<int>(window)].Read(slot, textAt, window, kProcessMemory, into) == nullptr;
    }
}

namespace headsup
{
    std::string PointerFix::Start()
    {
        if (Running()) return "";
        auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleA(kGameModule));
        if (base == nullptr) return std::string(kGameModule) + " is not loaded";
        ImageSection text{};
        if (!FindSection(base, kCodeSection, text)) return kCodeNotRead;
        const PointerMapping mapping = LocatePointerMapping(text);
        if (mapping.problem != LocateProblem::None) return Describe(mapping.problem);
        const PointerShow show = LocatePointerShow(text);
        if (show.problem != LocateProblem::None) return Describe(show.problem);
        const KeyHide keyHide = LocateKeyHide(text);
        if (keyHide.problem != LocateProblem::None) return Describe(keyHide.problem);
        std::vector<CodePatch> patches;
        for (const WindowGuess& guess : mapping.guesses)
        {
            uint8_t* call = base + guess.windowRectCall;
            patches.push_back(
                Instruction(call, kGuessBytes, kCallOpcode, BranchOperand(Address(call), Address(&HeadsUpWindowRect))));
            patches.push_back(
                Instruction(base + guess.metricsLoad, kGuessBytes, guess.movImmediate, Address(&HeadsUpSystemMetric)));
        }
        uint8_t* keyCall = base + keyHide.offCall;
        patches.push_back(Instruction(keyCall, kBranchLength, kCallOpcode,
            BranchOperand(Address(keyCall), Address(&MouseOffUnlessKept))));
        if (const std::string failure = PutIn(patches); !failure.empty()) return failure;
        m_Patches          = std::move(patches);
        g_ControllerGlobal = show.controllerGlobal;
        g_ShowPointer      = reinterpret_cast<uintptr_t>(base) + show.show;
        g_MouseUse         = reinterpret_cast<uintptr_t>(base) + keyHide.mouseUse;
        return "";
    }

    std::string PointerFix::Stop()
    {
        g_FixPointer = false;
        g_KeepKeys.clear();
        const std::string failure = PutBack(m_Patches,
            "the game's reading of the mouse was changed again after HeadsUp fixed it, so it was left as it is",
            "the game's reading of the mouse could not be put back");
        m_Patches.clear();
        g_ControllerGlobal = g_ShowPointer = 0;
        return KeptLoaded(failure);
    }

    void PointerFix::Enable(bool on) { g_FixPointer = on; }

    void PointerFix::KeepPointerFor(const std::vector<uint8_t>& keys)
    {
        if (keys != g_KeepKeys) g_KeepKeys = keys;
    }

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

namespace headsup
{
    namespace
    {
        // Before the window draws. The strip's own windows say where the game's strip ends. Any other idle, laid out
        // window in the game's menu space is noted; one the player picked goes down by the shift from that strip to
        // under the help bar, and one they gave an offset moves by it, through the game's own move.
        const WindowKey g_EquipKey = *KeyFromShortName("equip");

        // The equipment window's figures, which its owner placed once, follow it from at to to.
        void MoveEquipNumbers(uint32_t owner, WindowSpot at, int width, int height, WindowSpot to)
        {
            if (owner == 0) return;
            for (const NumberRun& run : kEquipNumbers)
                for (uint32_t i = 0; i < run.count; ++i)
                {
                    auto* number = reinterpret_cast<int16_t*>(owner + run.at + i * kNumberStride);
                    const WindowSpot moved = MovedNumber(
                        WindowSpot{number[kNumberX / 2], number[kNumberY / 2]}, at, width, height, to);
                    number[kNumberX / 2] = static_cast<int16_t>(moved.left);
                    number[kNumberY / 2] = static_cast<int16_t>(moved.top);
                }
        }

        // HeadsUp's own move of a window. One whose owner puts it back each frame keeps the spot its owner last asked
        // for, so the owner's check passes and the window stays.
        void MoveWindow(uint32_t mcd, const WindowKey& key, WindowSpot to)
        {
            auto* asked     = reinterpret_cast<int16_t*>(mcd + kMcdAsked);
            const int16_t x = asked[0], y = asked[1];
            reinterpret_cast<WindowMoveToFn>(g_WindowMoveTo)(mcd, to.left, to.top);
            if (!g_Placements.PutsItselfBack(key)) return;
            asked[0] = x;
            asked[1] = y;
        }

        // A window HeadsUp moved, back where the game put it while it is still open and where HeadsUp left it.
        void PutWindowBack(const WindowPlacements::Placed& placed)
        {
            const auto* mcd = reinterpret_cast<const uint8_t*>(placed.mcd);
            if (!Readable(mcd, kMcdVirtual + 1)) return;
            const uint32_t owner = ReadAt<uint32_t>(mcd + kMcdOwner, 0);
            if (owner == 0 || !Readable(reinterpret_cast<const uint8_t*>(owner), kOwnerMcd + sizeof(uint32_t)) ||
                ReadAt<uint32_t>(reinterpret_cast<const uint8_t*>(owner) + kOwnerMcd, 0) != placed.mcd ||
                ReadAt<uint32_t>(mcd + kMcdType, 0) == kWindowOpening || mcd[kMcdVirtual] != 0 ||
                ReadAt<int32_t>(mcd + kMcdMove, 0) != kMoveIdle)
                return;
            const auto edge = [&](int side) { return static_cast<int>(ReadAt<int16_t>(mcd + kMcdRect, 2 * side)); };
            const WindowSpot now{edge(0), edge(1)};
            if (now != placed.to) return; // moved since, by the game or the player
            reinterpret_cast<WindowMoveToFn>(g_WindowMoveTo)(placed.mcd, placed.from.left, placed.from.top);
            if (placed.key == g_EquipKey) MoveEquipNumbers(owner, now, edge(2) - edge(0), edge(3) - edge(1), placed.from);
            if (TitleWindow(placed.key) && g_TitleText != 0 &&
                Readable(reinterpret_cast<const uint8_t*>(owner) + g_TitleText + kStripTextSpot, 2 * sizeof(int32_t)))
            {
                auto* spot = reinterpret_cast<int32_t*>(owner + g_TitleText + kStripTextSpot);
                const WindowSpot home = g_TitlePlace.Natural(WindowSpot{spot[0], spot[1]});
                spot[0] = home.left;
                spot[1] = home.top;
            }
        }

        // The strip's title is drawn where its text object says, and the game sets that as if the window were where
        // it put it: the text goes where the window went.
        void FollowTitle(uint32_t owner, WindowSpot origin, WindowSpot at)
        {
            if (owner == 0 || g_TitleText == 0) return;
            auto* spot = reinterpret_cast<int32_t*>(owner + g_TitleText + kStripTextSpot);
            const WindowSpot seen{spot[0], spot[1]}, want = g_TitlePlace.Follow(seen, origin, at);
            if (want == seen) return;
            spot[0] = want.left;
            spot[1] = want.top;
        }

        void MoveFromHelpBar(uint32_t mcd, const WindowKey& key)
        {
            const auto field = [mcd](uint32_t at) { return reinterpret_cast<const uint8_t*>(mcd + at); };
            if (ReadAt<uint32_t>(field(kMcdOwner), 0) == 0 || ReadAt<uint32_t>(field(kMcdType), 0) == kWindowOpening ||
                *field(kMcdVirtual) != 0 || ReadAt<int32_t>(field(kMcdMove), 0) != kMoveIdle)
                return;
            const auto edge = [&](int side) { return static_cast<int>(ReadAt<int16_t>(field(kMcdRect), 2 * side)); };
            const int left = edge(0), top = edge(1), right = edge(2), bottom = edge(3);
            const HelpBand& band = *g_HelpBand;
            const WindowSpot now{left, top};
            if (!MovableWindow(key))
            {
                // The strip's bottom where the game put it; its help and title go under the top bar when asked.
                const WindowSpot origin = g_Placements.OriginOf(mcd, key, now);
                g_StripBottom           = std::max(g_StripBottom, origin.top + (bottom - top));
                WindowSpot at           = now;
                if (band.moveGameHelp && band.topBar != 0 && HelpStripWindow(key))
                    if (const std::optional<WindowSpot> to = g_Placements.PlaceFor(mcd, key, now, right - left,
                            bottom - top, 0, 0, band.topBar, INT_MAX, band.screenRight, band.screenBottom))
                    {
                        MoveWindow(mcd, key, *to);
                        at = *to;
                    }
                if (TitleWindow(key)) FollowTitle(ReadAt<uint32_t>(field(kMcdOwner), 0), origin, at);
                return;
            }
            g_SeenWindows.Add(key);
            g_DrawnWindows.Add(key);
            const KeyOffset* offset = nullptr;
            for (size_t i = 0; i < g_OffsetCount; ++i)
                if (g_Offsets[i].key == key) offset = &g_Offsets[i];
            // Down from under the game's strip once its windows were seen, so there is a shift to go by: a window
            // picked (or in a group that goes) wherever it opens, any other that opens under the bar when every window
            // goes, and with it those it would then lie over. A window whose area only reaches up there, as the chat
            // log's, starts below it and stays.
            const bool picked = g_MovedWindows.Has(key);
            const bool down   = (picked || band.allUnderBar) && g_StripBottom != 0 && band.bottom != 0;
            if (!down && offset == nullptr) return;
            if (const std::optional<WindowSpot> to = g_Placements.PlaceFor(mcd, key, now, right - left, bottom - top,
                    offset != nullptr ? offset->x : 0, offset != nullptr ? offset->y : 0,
                    down ? WindowShift(band, g_StripBottom) : 0, picked ? INT_MAX : band.bottom,
                    band.screenRight, band.screenBottom, band.allUnderBar))
            {
                MoveWindow(mcd, key, *to);
                if (key == g_EquipKey) MoveEquipNumbers(ReadAt<uint32_t>(field(kMcdOwner), 0), now, right - left,
                                           bottom - top, *to);
            }
        }
    }
}

// On the game's stack, inside its draw loop: nothing here may allocate, throw or call Ashita.
extern "C" uint32_t HeadsUpOnWindowDraw(uint32_t mcd)
{
    using namespace headsup;
    if ((!g_HiddenWindows.Any() && !g_HelpBand) || mcd == 0) return 0;
    const uint32_t descriptor = *reinterpret_cast<const uint32_t*>(mcd + kWindowDescriptor);
    if (descriptor == 0) return 0;
    const auto* name = reinterpret_cast<const char*>(descriptor + kDescriptorName);
    if (g_HelpBand && g_WindowMoveTo != 0)
        if (const std::optional<WindowKey> key = WindowKeyOf(name)) MoveFromHelpBar(mcd, *key);
    return SkipsWindow(name, g_HiddenWindows) ? 1 : 0;
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
    if (g_HideNames)
    {
        ++g_Stats.hidden;
        return 1;
    }
    if (owner->action == HookAction::Keep) return 0;
    if (owner->action == HookAction::Hide) ++g_Stats.hidden;
    else ++g_Stats.replaced;
    return 1;
}
