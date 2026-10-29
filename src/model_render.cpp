#include "model_render.h"

#include "argb.h"
#include "gpu.h"
#include "outline_math.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace headsup
{
    namespace
    {
        constexpr DWORD kVertexFormat = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1;
        static_assert(sizeof(DrawVertex) == 9 * sizeof(float), "DrawVertex must match kVertexFormat");
        constexpr D3DCOLOR kAmbient  = 0xFF707070;
        constexpr DWORD kAlphaCutoff = 0x20;
        constexpr float kLightDown   = 0.6f; // how steeply the light falls on the model, against its share from the front

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

    bool ModelPicture::Begin(IDirect3DDevice8* device, int width, int height, uint32_t background)
    {
        if (device == nullptr || width <= 0 || height <= 0) return false;
        const int side = PowerOfTwo(std::max(width, height));
        if (m_Target == nullptr || m_Side != side)
        {
            Release();
            if (FAILED(device->CreateTexture(static_cast<UINT>(side), static_cast<UINT>(side), 1, D3DUSAGE_RENDERTARGET,
                    D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &m_Target)) ||
                FAILED(device->CreateDepthStencilSurface(static_cast<UINT>(side), static_cast<UINT>(side), D3DFMT_D24S8,
                    D3DMULTISAMPLE_NONE, &m_Depth)))
            {
                Release();
                return false;
            }
            m_Side = side;
        }
        IDirect3DSurface8* surface = nullptr;
        if (FAILED(m_Target->GetSurfaceLevel(0, &surface))) return false;
        const HRESULT bound = device->SetRenderTarget(surface, m_Depth);
        surface->Release();
        if (FAILED(bound)) return false;
        m_Width  = width;
        m_Height = height;
        m_U      = static_cast<float>(width) / static_cast<float>(m_Side);
        m_V      = static_cast<float>(height) / static_cast<float>(m_Side);
        D3DVIEWPORT8 viewport{0, 0, static_cast<DWORD>(width), static_cast<DWORD>(height), 0.0f, 1.0f};
        device->SetViewport(&viewport);
        device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, background, 1.0f, 0);
        return true;
    }

    void ModelPicture::Release()
    {
        ReleaseTexture(m_Target);
        if (m_Depth != nullptr) m_Depth->Release();
        m_Depth = nullptr;
        m_Side  = 0;
    }

    bool DrawnModel::Load(IDirect3DDevice8* device, ModelFile model)
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
        m_Bounds = BoundsOf(PosedBatches(model.meshes, PoseSkeleton(model.skeleton, {}, 0.0f)));
        m_Model  = std::move(model);
        m_Loaded = true;
        return true;
    }

    std::vector<DrawBatch> DrawnModel::Posed(double seconds, const Vec3& at) const
    {
        const std::vector<const Motion*> idle = IdleMotions(m_Model);
        const float frame = idle.empty() ? 0.0f : MotionFrame(*idle.front(), seconds);
        std::vector<DrawBatch> batches = PosedBatches(m_Model.meshes, PoseSkeleton(m_Model.skeleton, idle, frame));
        Move(batches, at);
        return batches;
    }

    void DrawnModel::DrawBatches(IDirect3DDevice8* device, const std::vector<DrawBatch>& batches, bool lit) const
    {
        for (const DrawBatch& batch : batches)
        {
            if (batch.vertices.empty()) continue;
            const auto found     = m_Textures.find(batch.picture);
            const bool textured  = found != m_Textures.end() && found->second.texture != nullptr;
            device->SetTexture(0, textured ? found->second.texture : nullptr);
            if (lit) device->SetTextureStageState(0, D3DTSS_COLOROP, textured ? D3DTOP_MODULATE : D3DTOP_SELECTARG2);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG1, textured ? D3DTA_TEXTURE : D3DTA_DIFFUSE);
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
    }

    void DrawnModel::Draw(IDirect3DDevice8* device, const std::vector<DrawBatch>& batches, const ModelView& view, float yaw,
        const ModelOutline* outline, int width, int height) const
    {
        if (!m_Loaded) return;
        const D3DMATRIX world{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const D3DMATRIX eye = ToD3D(view.view), lens = ToD3D(view.projection);
        device->SetTransform(D3DTS_WORLD, &world);
        device->SetTransform(D3DTS_VIEW, &eye);
        device->SetTransform(D3DTS_PROJECTION, &lens);
        device->SetVertexShader(kVertexFormat);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
        device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
        device->SetRenderState(D3DRS_ALPHAREF, kAlphaCutoff);
        device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
        device->SetRenderState(D3DRS_STENCILMASK, 0xFF);
        device->SetRenderState(D3DRS_STENCILWRITEMASK, 0xFF);
        device->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
        device->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
        device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
        device->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
        device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
        device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
        device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        constexpr DWORD kAllColors =
            D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA;

        if (outline != nullptr && outline->copies > 0)
        {
            // As in the world: the model marked in the stencil alone, then its copies shifted round it in the
            // outline's color where it is not; the model is drawn over them after.
            device->SetRenderState(D3DRS_LIGHTING, FALSE);
            device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            device->SetRenderState(D3DRS_STENCILENABLE, TRUE);
            device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
            device->SetRenderState(D3DRS_STENCILREF, outline->stencil);
            device->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
            device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
            device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
            DrawBatches(device, batches, false);

            const bool faint = outline->alpha < kOpaqueAlpha;
            device->SetRenderState(D3DRS_COLORWRITEENABLE, kAllColors);
            device->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_NOTEQUAL);
            // The copies overlap: a faint one marks what it drew so the next leaves it, and its color is blended once.
            device->SetRenderState(D3DRS_STENCILPASS, faint ? D3DSTENCILOP_REPLACE : D3DSTENCILOP_KEEP);
            device->SetRenderState(D3DRS_TEXTUREFACTOR, WithAlpha(Opaque(outline->argb), outline->alpha));
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, faint ? TRUE : FALSE);
            device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            device->SetRenderState(D3DRS_ALPHAREF, ScaledAlphaRef(kAlphaCutoff, outline->alpha));
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, faint ? D3DTOP_MODULATE : D3DTOP_SELECTARG1);
            device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
            for (int copy = 0; copy < outline->copies; ++copy)
            {
                float dx = 0.0f, dy = 0.0f;
                OutlineOffset(copy, outline->copies, outline->pixels, static_cast<float>(width), static_cast<float>(height), dx, dy);
                const D3DMATRIX shifted = ToD3D(ShiftProjection(view.projection, dx, dy));
                device->SetTransform(D3DTS_PROJECTION, &shifted);
                DrawBatches(device, batches, false);
            }
            device->SetTransform(D3DTS_PROJECTION, &lens);
            device->SetRenderState(D3DRS_ALPHAREF, kAlphaCutoff);
            device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        }

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
        device->SetRenderState(D3DRS_LIGHTING, TRUE);
        device->SetRenderState(D3DRS_AMBIENT, kAmbient);
        device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
        device->SetRenderState(D3DRS_COLORVERTEX, TRUE);
        device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
        device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, kAllColors);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        DrawBatches(device, batches, true);
        device->LightEnable(0, FALSE);
    }

    void DrawnModel::Release()
    {
        for (auto& [name, t] : m_Textures)
            ReleaseTexture(t.texture);
        m_Textures.clear();
        m_Model  = {};
        m_Loaded = false;
    }
}
