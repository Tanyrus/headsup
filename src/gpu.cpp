#include "gpu.h"

#include "game_glyphs.h"
#include "utf16.h"

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace headsup
{
    namespace
    {
        struct QuadVertex
        {
            float x, y, z, rhw;
            DWORD color;
            float u, v;
        };
        constexpr DWORD kQuadFvf     = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
        constexpr float kPixelCenter = 0.5f; // Direct3D 8 samples pixel centers at half-pixel offsets
    }

    IDirect3DTexture8* CreateTexture(IDirect3DDevice8* device, const void* bgra, int width, int height, float& u,
        float& v)
    {
        const int sideX = TextureSide(width), sideY = TextureSide(height);
        IDirect3DTexture8* texture = nullptr;
        if (FAILED(device->CreateTexture(static_cast<UINT>(sideX), static_cast<UINT>(sideY), 1, 0, D3DFMT_A8R8G8B8,
                D3DPOOL_MANAGED, &texture)))
            return nullptr;
        D3DLOCKED_RECT locked{};
        if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
        {
            texture->Release();
            return nullptr;
        }
        CopyToCorner(bgra, width, height, locked.pBits, locked.Pitch, sideX, sideY);
        texture->UnlockRect(0);
        u = static_cast<float>(width) / static_cast<float>(sideX);
        v = static_cast<float>(height) / static_cast<float>(sideY);
        return texture;
    }

    void ReleaseTexture(IDirect3DTexture8*& texture)
    {
        if (texture != nullptr) texture->Release();
        texture = nullptr;
    }

    HDC GdiText::WithFont(std::string_view family, int pixelHeight, bool bold, int tracking)
    {
        if (m_Context == nullptr) m_Context = CreateCompatibleDC(nullptr);
        if (m_Context == nullptr) return nullptr;
        HFONT& font = m_Fonts.Use(std::tuple{family, pixelHeight, bold}, m_Frame);
        // A negative height asks for the character height, without the font's internal leading.
        if (font == nullptr)
            font = CreateFontW(-pixelHeight, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                Utf16(std::string(family)).c_str());
        if (font == nullptr) return nullptr;
        SelectObject(m_Context, font);
        // Set every time: the context keeps the last text's.
        SetTextCharacterExtra(m_Context, tracking);
        return m_Context;
    }

    bool GdiText::Rasterize(const char* text, const char* family, int pixelHeight, bool bold, int tracking,
        int margin, Coverage& out)
    {
        out = Coverage{};
        const HDC dc = WithFont(family, pixelHeight, bold, tracking);
        if (dc == nullptr) return false;
        const std::wstring wide = Utf16(text);
        const auto length       = static_cast<int>(wide.size());
        SIZE size{};
        if (!GetTextExtentPoint32W(dc, wide.c_str(), length, &size)) return false;
        const int width  = size.cx + 2 * margin;
        const int height = size.cy + 2 * margin;
        BITMAPINFO info{};
        info.bmiHeader.biSize        = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth       = width;
        info.bmiHeader.biHeight      = -height; // rows top to bottom
        info.bmiHeader.biPlanes      = 1;
        info.bmiHeader.biBitCount    = kBytesPerPixel * 8;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits                   = nullptr;
        const HBITMAP bitmap         = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bitmap == nullptr || bits == nullptr)
        {
            if (bitmap != nullptr) DeleteObject(bitmap);
            return false;
        }
        const HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
        const size_t pixels     = static_cast<size_t>(width) * static_cast<size_t>(height);
        std::memset(bits, 0, pixels * kBytesPerPixel);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, margin, margin, wide.c_str(), length);
        GdiFlush();
        const auto* bgrx = static_cast<const uint8_t*>(bits);
        out.width        = width;
        out.height       = height;
        out.alpha.resize(pixels);
        for (size_t i = 0; i < pixels; ++i)
        {
            const uint8_t* bgr = bgrx + i * kBytesPerPixel;
            out.alpha[i]       = std::max({bgr[0], bgr[1], bgr[2]});
        }
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        return true;
    }

    bool GdiText::Extent(const char* text, const char* family, int pixelHeight, bool bold, int& width, int& height)
    {
        const HDC dc = WithFont(family, pixelHeight, bold, 0);
        if (dc == nullptr) return false;
        const std::wstring wide = Utf16(text);
        SIZE size{};
        if (!GetTextExtentPoint32W(dc, wide.c_str(), static_cast<int>(wide.size()), &size)) return false;
        width  = size.cx;
        height = size.cy;
        return true;
    }

    float GdiText::Measure(const char* text, const char* family, int pixelHeight)
    {
        int width = 0, height = 0;
        return Extent(text, family, pixelHeight, false, width, height) ? static_cast<float>(width) : 0.0f;
    }

    LineInk GdiText::FontInk(const char* family, int pixelHeight, const char* glyphs)
    {
        const auto known = m_Inks.find(std::tuple{std::string_view(family), pixelHeight, std::string_view(glyphs)});
        if (known != m_Inks.end()) return known->second;
        auto inkOf = [&](const char* text) {
            Coverage coverage;
            return Rasterize(text, family, pixelHeight, false, 0, 0, coverage)
                       ? FindInk(coverage, kInkCoverage)
                       : InkBox{};
        };
        const std::optional<LineInk> ink = LineInkOf(inkOf(glyphs), inkOf("H"));
        // Without GDI the font's size stands in, and is not kept: the text will not draw either.
        const auto size = static_cast<float>(pixelHeight);
        if (!ink) return LineInk{0.0f, size, size};
        m_Inks.emplace(std::tuple{std::string(family), pixelHeight, std::string(glyphs)}, *ink);
        return *ink;
    }

    void GdiText::Sweep()
    {
        ++m_Frame;
        const std::vector<HFONT> idle = m_Fonts.TakeIdle(m_Frame, kIdleFrames);
        if (idle.empty()) return;
        // GDI does not delete a font selected into a context: the context takes back its own first.
        if (m_Context != nullptr) SelectObject(m_Context, GetStockObject(SYSTEM_FONT));
        for (const HFONT font : idle)
            if (font != nullptr) DeleteObject(font);
    }

    void GdiText::Release()
    {
        // The context goes first, so none of the fonts is selected into it.
        if (m_Context != nullptr) DeleteDC(m_Context);
        m_Context = nullptr;
        for (const HFONT font : m_Fonts.TakeAll())
            if (font != nullptr) DeleteObject(font);
    }

    const IconTexture* IconTextures::For(IDirect3DDevice8* device, Icon icon)
    {
        IconTexture& t = m_Textures[static_cast<size_t>(icon)];
        if (t.texture != nullptr) return &t;
        if (m_Failed) return nullptr;
        const IconBitmap& bitmap = IconImage(icon);
        const auto width = static_cast<int>(bitmap.width), height = static_cast<int>(bitmap.height);
        t.texture = CreateTexture(device, bitmap.bgra, width, height, t.u, t.v);
        if (t.texture == nullptr)
        {
            m_Failed  = true;
            m_Failure = "Direct3D could not make a " + std::to_string(width) + "x" + std::to_string(height) +
                        " icon texture";
            return nullptr;
        }
        t.width  = static_cast<float>(width);
        t.height = static_cast<float>(height);
        return &t;
    }

    std::string IconTextures::TakeFailure() { return std::exchange(m_Failure, std::string()); }

    void IconTextures::Release()
    {
        for (IconTexture& t : m_Textures)
            ReleaseTexture(t.texture);
    }

    void SetQuadStates(IDirect3DDevice8* device, bool depthTest)
    {
        device->SetVertexShader(kQuadFvf);
        device->SetRenderState(D3DRS_ZENABLE, depthTest ? D3DZB_TRUE : D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
            D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
        device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
        device->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
        device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }

    std::optional<DWORD> RecordQuadStates(IDirect3DDevice8* device)
    {
        DWORD block = 0;
        if (FAILED(device->BeginStateBlock())) return std::nullopt;
        SetQuadStates(device, true);
        if (FAILED(device->EndStateBlock(&block))) return std::nullopt;
        return block;
    }

    void DrawQuad(IDirect3DDevice8* device, const GpuQuad& quad, bool depthTest)
    {
        const float left = quad.x - kPixelCenter, top = quad.y - kPixelCenter;
        const float right = left + quad.width, bottom = top + quad.height;
        const float z     = depthTest ? quad.depth : 0.0f;
        const QuadVertex v[kQuadVertices] = {{left, top, z, 1.0f, quad.tint, quad.u0, 0.0f},
            {right, top, z, 1.0f, quad.tint, quad.u1, 0.0f}, {left, bottom, z, 1.0f, quad.tint, quad.u0, quad.v},
            {right, bottom, z, 1.0f, quad.tint, quad.u1, quad.v}};
        device->SetTexture(0, quad.texture);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, kQuadVertices - 2, v, sizeof(QuadVertex));
    }
}
