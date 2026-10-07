#include "d3d_util.h"

#include "outline_math.h"

#include <algorithm>

namespace headsup
{
    namespace
    {
        constexpr DWORD kWineDeclarationHandles = 0xF0000000; // Wine numbers declaration handles from here
        constexpr uintptr_t kStackScanBytes     = 0x1000;     // the game's draw call chain fits well inside this
    }

    bool IsDeclarationHandle(DWORD vertexShader)
    {
        return (vertexShader & 1) != 0 || vertexShader >= kWineDeclarationHandles; // FVF codes never have bit 0
    }

    bool BoundFvf(IDirect3DDevice8* device, DWORD& fvf)
    {
        return SUCCEEDED(device->GetVertexShader(&fvf)) && !IsDeclarationHandle(fvf);
    }

    uintptr_t BoundTexture(IDirect3DDevice8* device)
    {
        IDirect3DBaseTexture8* bound = nullptr;
        if (FAILED(device->GetTexture(0, &bound)) || bound == nullptr) return 0;
        bound->Release();
        return reinterpret_cast<uintptr_t>(bound);
    }

    // noinline: the scan starts at this frame; above it sit HeadsUp's callers, Ashita's hook and the game's draw call chain.
    __attribute__((noinline)) const ActorInfo* FindOwnerOnStack(const Tracker& tracker)
    {
        const auto sp       = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
        const auto* tib     = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
        const uintptr_t end = std::min<uintptr_t>(reinterpret_cast<uintptr_t>(tib->StackBase), sp + kStackScanBytes);
        return FindOwner(reinterpret_cast<const uint32_t*>(sp & ~uintptr_t{3}), reinterpret_cast<const uint32_t*>(end & ~uintptr_t{3}), tracker);
    }

    bool TargetScale::Read(IDirect3DSurface8* target, float backBufferWidth, float backBufferHeight)
    {
        D3DSURFACE_DESC desc{};
        if (FAILED(target->GetDesc(&desc)) || desc.Width == 0 || desc.Height == 0) return false;
        surface = reinterpret_cast<uintptr_t>(target);
        x       = backBufferWidth / static_cast<float>(desc.Width);
        y       = backBufferHeight / static_cast<float>(desc.Height);
        return true;
    }
}
