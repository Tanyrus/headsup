#include "model_render.h"

#include "gpu.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace headsup
{
    namespace
    {
        constexpr DWORD kVertexFormat = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;
        static_assert(sizeof(DrawVertex) == 9 * sizeof(float), "DrawVertex must match kVertexFormat");
        constexpr D3DCOLOR kAmbient   = 0xFF707070;
        constexpr DWORD kAlphaCutoff  = 0x20;
        constexpr float kLightDown    = 0.6f; // how steeply the light falls on the model, against its share from the front

        D3DMATRIX ToD3D(const Mat4& m)
        {
            D3DMATRIX out;
            std::memcpy(&out, m.m, sizeof(out));
            return out;
        }

        int PowerOfTwo(int side)
        {
            int out = 1;
            while (out < side)
                out <<= 1;
            return out;
        }
    }

    bool ModelRenderer::Load(IDirect3DDevice8* device, ModelFile model)
    {
        Release();
        for (const ColorPicture& picture : model.pictures)
        {
            Texture& t = m_Textures[picture.name];
            if (t.texture != nullptr) continue;
            const std::vector<uint8_t> shown = ShownPixels(picture);
            t.texture = CreateTexture(device, shown.data(), picture.width, picture.height, t.u, t.v);
            if (t.texture == nullptr)
            {
                Release();
                return false;
            }
        }
        const std::vector<DrawBatch> standing = PosedBatches(model.meshes, PoseSkeleton(model.skeleton, {}, 0.0f));
        m_Bounds = BoundsOf(standing);
        m_Model  = std::move(model);
        m_Loaded = true;
        return true;
    }

    bool ModelRenderer::EnsureTarget(IDirect3DDevice8* device, int width, int height)
    {
        const int side = PowerOfTwo(std::max(width, height));
        if (m_Target != nullptr && m_Side == side) return true;
        ReleaseTarget();
        if (FAILED(device->CreateTexture(static_cast<UINT>(side), static_cast<UINT>(side), 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                D3DPOOL_DEFAULT, &m_Target)) ||
            FAILED(device->CreateDepthStencilSurface(static_cast<UINT>(side), static_cast<UINT>(side), D3DFMT_D24S8,
                D3DMULTISAMPLE_NONE, &m_Depth)))
        {
            ReleaseTarget();
            return false;
        }
        m_Side = side;
        return true;
    }

    bool ModelRenderer::Render(IDirect3DDevice8* device, double seconds, float yaw, int width, int height)
    {
        if (!m_Loaded || width <= 0 || height <= 0 || !EnsureTarget(device, width, height)) return false;
        IDirect3DSurface8* surface = nullptr;
        if (FAILED(m_Target->GetSurfaceLevel(0, &surface))) return false;
        const HRESULT bound = device->SetRenderTarget(surface, m_Depth);
        surface->Release();
        if (FAILED(bound)) return false;
        m_U = static_cast<float>(width) / static_cast<float>(m_Side);
        m_V = static_cast<float>(height) / static_cast<float>(m_Side);
        D3DVIEWPORT8 viewport{0, 0, static_cast<DWORD>(width), static_cast<DWORD>(height), 0.0f, 1.0f};
        device->SetViewport(&viewport);
        device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);

        const std::vector<const Motion*> idle = IdleMotions(m_Model);
        const float frame = idle.empty() ? 0.0f : MotionFrame(*idle.front(), seconds);
        const std::vector<DrawBatch> batches = PosedBatches(m_Model.meshes, PoseSkeleton(m_Model.skeleton, idle, frame));
        const ModelView view = FramedView(m_Bounds, yaw, static_cast<float>(width) / static_cast<float>(height));
        const D3DMATRIX world{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const D3DMATRIX eye = ToD3D(view.view), lens = ToD3D(view.projection);
        device->SetTransform(D3DTS_WORLD, &world);
        device->SetTransform(D3DTS_VIEW, &eye);
        device->SetTransform(D3DTS_PROJECTION, &lens);

        // A white light from the camera's side and above (the game's up is -y), and the vertex colors as the
        // diffuse of the parts drawn without a picture.
        D3DLIGHT8 light{};
        light.Type    = D3DLIGHT_DIRECTIONAL;
        light.Diffuse = {1.0f, 1.0f, 1.0f, 1.0f};
        const float across = std::sqrt(1.0f + kLightDown * kLightDown);
        light.Direction    = {-std::sin(yaw) / across, kLightDown / across, std::cos(yaw) / across};
        device->SetLight(0, &light);
        device->LightEnable(0, TRUE);
        D3DMATERIAL8 material{};
        material.Diffuse = material.Ambient = {1.0f, 1.0f, 1.0f, 1.0f};
        device->SetMaterial(&material);

        device->SetVertexShader(kVertexFormat);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
        device->SetRenderState(D3DRS_LIGHTING, TRUE);
        device->SetRenderState(D3DRS_AMBIENT, kAmbient);
        device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
        device->SetRenderState(D3DRS_COLORVERTEX, TRUE);
        device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
        device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
        device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHAREF, kAlphaCutoff);
        device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
        device->SetRenderState(D3DRS_COLORWRITEENABLE,
            D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
        device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
        device->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
        device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
        device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
        device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

        for (const DrawBatch& batch : batches)
        {
            if (batch.vertices.empty()) continue;
            const auto found = m_Textures.find(batch.picture);
            const bool textured = found != m_Textures.end() && found->second.texture != nullptr;
            device->SetTexture(0, textured ? found->second.texture : nullptr);
            device->SetTextureStageState(0, D3DTSS_COLOROP, textured ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, textured ? D3DTOP_SELECTARG1 : D3DTOP_SELECTARG2);
            // A picture padded to a power of two is drawn by the share of it that is the picture.
            const bool padded = textured && (found->second.u < 1.0f || found->second.v < 1.0f);
            if (padded)
            {
                const D3DMATRIX scale{found->second.u, 0, 0, 0, 0, found->second.v, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                device->SetTransform(D3DTS_TEXTURE0, &scale);
            }
            device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, padded ? D3DTTFF_COUNT2 : D3DTTFF_DISABLE);
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(batch.vertices.size() / 3), batch.vertices.data(), sizeof(DrawVertex));
        }
        device->SetTexture(0, nullptr);
        device->LightEnable(0, FALSE);
        return true;
    }

    void ModelRenderer::ReleaseTarget()
    {
        ReleaseTexture(m_Target);
        if (m_Depth != nullptr) m_Depth->Release();
        m_Depth = nullptr;
        m_Side  = 0;
    }

    void ModelRenderer::Release()
    {
        ReleaseTarget();
        for (auto& [name, t] : m_Textures)
            ReleaseTexture(t.texture);
        m_Textures.clear();
        m_Model  = {};
        m_Loaded = false;
    }
}
