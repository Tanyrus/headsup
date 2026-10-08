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
