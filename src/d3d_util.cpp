#include "d3d_util.h"

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

    CopyState ReadCopyState(IDirect3DDevice8* device)
    {
        CopyState state;
        state.texture = BoundTexture(device);
        DWORD value   = 0;
        auto stage    = [&](D3DTEXTURESTAGESTATETYPE type) { return SUCCEEDED(device->GetTextureStageState(0, type, &value)) ? value : 0; };
        auto render   = [&](D3DRENDERSTATETYPE type) { return SUCCEEDED(device->GetRenderState(type, &value)) ? value : 0; };
        state.colorOp   = stage(D3DTSS_COLOROP);
        state.colorArg1 = stage(D3DTSS_COLORARG1);
        state.colorArg2 = stage(D3DTSS_COLORARG2);
        state.alphaOp   = stage(D3DTSS_ALPHAOP);
        if (SUCCEEDED(device->GetVertexShader(&value))) state.vertexShader = value;
        D3DVIEWPORT8 viewport{};
        if (SUCCEEDED(device->GetViewport(&viewport)))
        {
            state.viewportX      = viewport.X;
            state.viewportY      = viewport.Y;
            state.viewportWidth  = viewport.Width;
            state.viewportHeight = viewport.Height;
        }
        state.zEnable    = render(D3DRS_ZENABLE);
        state.alphaBlend = render(D3DRS_ALPHABLENDENABLE);
        state.srcBlend   = render(D3DRS_SRCBLEND);
        state.destBlend  = render(D3DRS_DESTBLEND);
        return state;
    }

    void RestoreCopyState(IDirect3DDevice8* device, const CopyState& state, IDirect3DBaseTexture8* texture)
    {
        device->SetTexture(0, texture);
        device->SetTextureStageState(0, D3DTSS_COLOROP, state.colorOp);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, state.colorArg1);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, state.colorArg2);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, state.alphaOp);
        device->SetVertexShader(state.vertexShader);
        D3DVIEWPORT8 viewport{};
        if (SUCCEEDED(device->GetViewport(&viewport)))
        {
            viewport.X      = state.viewportX;
            viewport.Y      = state.viewportY;
            viewport.Width  = state.viewportWidth;
            viewport.Height = state.viewportHeight;
            device->SetViewport(&viewport);
        }
        device->SetRenderState(D3DRS_ZENABLE, state.zEnable);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, state.alphaBlend);
        device->SetRenderState(D3DRS_SRCBLEND, state.srcBlend);
        device->SetRenderState(D3DRS_DESTBLEND, state.destBlend);
    }

    // noinline: the scan starts at this frame; above it sit HeadsUp's callers, Ashita's hook and the game's draw call chain.
    __attribute__((noinline)) const ActorInfo* FindOwnerOnStack(const Tracker& tracker)
    {
        const auto sp       = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
        const auto* tib     = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
        const uintptr_t end = std::min<uintptr_t>(reinterpret_cast<uintptr_t>(tib->StackBase), sp + kStackScanBytes);
        return FindOwner(reinterpret_cast<const uint32_t*>(sp & ~uintptr_t{3}), reinterpret_cast<const uint32_t*>(end & ~uintptr_t{3}), tracker);
    }
}
