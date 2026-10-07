#include "nameplate_render.h"

#include "nameplate.h"
#include "shapes.h"
#include "text_image.h"
#include "text_raster.h"

#include <algorithm>
#include <cmath>
#include <cstring>

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
        constexpr DWORD kQuadFvf          = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
        constexpr float kOutlinePerHeight = 0.1f;  // outline width, per pixel of text or cursor height
        constexpr float kPixelCenter      = 0.5f;  // Direct3D 8 samples pixel centers at half-pixel offsets
        constexpr int kBytesPerPixel      = 4;

        int OutlineRadius(int pixelHeight)
        {
            return std::max(1, static_cast<int>(std::lround(static_cast<float>(pixelHeight) * kOutlinePerHeight)));
        }

        void ReleaseText(IDirect3DTexture8*& texture)
        {
            if (texture != nullptr) texture->Release();
            texture = nullptr;
        }
    }

    IDirect3DTexture8* NameplateRenderer::CreateTexture(const void* bgra, int width, int height, float& u, float& v)
    {
        const int sideX = TextureSide(width), sideY = TextureSide(height);
        IDirect3DTexture8* texture = nullptr;
        if (FAILED(m_Device->CreateTexture(static_cast<UINT>(sideX), static_cast<UINT>(sideY), 1, 0, D3DFMT_A8R8G8B8,
                D3DPOOL_MANAGED, &texture)))
            return nullptr;
        D3DLOCKED_RECT locked{};
        if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
        {
            texture->Release();
            return nullptr;
        }
        // The rest of the texture stays transparent, so filtering at the image's edge blends with nothing.
        const auto* source = static_cast<const uint8_t*>(bgra);
        for (int y = 0; y < sideY; ++y)
        {
            auto* row = static_cast<uint8_t*>(locked.pBits) + static_cast<ptrdiff_t>(y) * locked.Pitch;
            std::memset(row, 0, static_cast<size_t>(sideX) * kBytesPerPixel);
            if (y < height)
                std::memcpy(row, source + static_cast<size_t>(y) * width * kBytesPerPixel, static_cast<size_t>(width) * kBytesPerPixel);
        }
        texture->UnlockRect(0);
        u = static_cast<float>(width) / static_cast<float>(sideX);
        v = static_cast<float>(height) / static_cast<float>(sideY);
        return texture;
    }

    bool NameplateRenderer::Prepare(TextTexture& t, const char* text, uint32_t color, int pixelHeight, const Settings& settings)
    {
        const uint32_t outline = ToArgb(settings.textOutline);
        const std::string key  = std::string(text) + '\n' + settings.fontName + ' ' +
                                std::to_string(settings.fontBold) + ' ' + std::to_string(pixelHeight) + ' ' +
                                std::to_string(color) + ' ' + std::to_string(outline);
        if (t.texture != nullptr && t.key == key) return true;
        const int radius = OutlineRadius(pixelHeight);
        Coverage coverage;
        if (!RasterizeText(text, settings.fontName.c_str(), pixelHeight, settings.fontBold, radius, coverage)) return false;
        return Upload(t, OutlinedText(coverage, radius, color, outline), key);
    }

    bool NameplateRenderer::PrepareCursor(TextTexture& t, uint32_t color, int pixelHeight, const Settings& settings)
    {
        const uint32_t outline = ToArgb(settings.textOutline);
        const std::string key  = std::string(settings.cursorFeather ? "feather " : "arrow ") + std::to_string(pixelHeight) +
                                ' ' + std::to_string(color) + ' ' + std::to_string(outline);
        if (t.texture != nullptr && t.key == key) return true;
        const int radius        = OutlineRadius(pixelHeight);
        const Shape& shape      = settings.cursorFeather ? FeatherShape() : ArrowShape();
        const Coverage coverage = ShapeCoverage(shape, pixelHeight, radius);
        if (!Upload(t, OutlinedText(coverage, radius, color, outline), key)) return false;
        const float width = static_cast<float>(coverage.width - 2 * radius);
        t.tip             = (static_cast<float>(radius) + ShapeTip(shape) * width) / static_cast<float>(coverage.width);
        return true;
    }

    bool NameplateRenderer::Upload(TextTexture& t, const Image& image, std::string key)
    {
        float u = 0.0f, v = 0.0f;
        IDirect3DTexture8* texture = CreateTexture(image.argb.data(), image.width, image.height, u, v);
        if (texture == nullptr) return false;
        ReleaseText(t.texture);
        t = TextTexture{texture, std::move(key), static_cast<float>(image.width), static_cast<float>(image.height), u, v};
        return true;
    }

    IDirect3DTexture8* NameplateRenderer::IconTexture(Icon icon)
    {
        IDirect3DTexture8*& texture = m_IconTextures[static_cast<size_t>(icon)];
        if (texture != nullptr || m_IconsFailed) return texture;
        const IconBitmap& bitmap = IconImage(icon);
        float u = 0.0f, v = 0.0f;
        texture = CreateTexture(bitmap.bgra, static_cast<int>(bitmap.width), static_cast<int>(bitmap.height), u, v);
        if (texture == nullptr) m_IconsFailed = m_IconFailurePending = true;
        return texture;
    }

    void NameplateRenderer::Update(const Tracker& tracker, const OutlineRenderer& outline, const Settings& settings, float toX,
        float toY, const CursorTargets& cursors, double now)
    {
        ++m_Frame;
        m_Shown.clear();
        m_Quads.clear();
        m_CursorNames.clear();
        const uint32_t iconTint  = ToArgb(settings.iconTint);
        const float screenWidth  = outline.BackBufferWidth();
        const float screenHeight = outline.BackBufferHeight();
        // Cursors are drawn after every nameplate, so another name never covers one.
        std::vector<Quad> cursorQuads;
        auto addQuad = [&](std::vector<Quad>& into, IDirect3DTexture8* texture, float x, float y, float width, float height, float u,
                           float v, float depth, uint32_t tint) {
            // Whole pixels in the target keep the text as sharp as it was drawn.
            into.push_back(Quad{texture, std::round(x * toX), std::round(y * toY), width, height, u, v, depth, tint});
        };
        if (NameplatesOn(settings) && !m_Failed && m_Device != nullptr)
        {
            for (const ActorPtr actor : tracker.Actors())
            {
                const ActorInfo* info = tracker.Find(actor);
                if (info == nullptr) continue;
                const ScreenBox* plate = outline.NameplateBox(info->index);
                if (plate == nullptr) continue;
                // Checked where our name goes: a seated player's head can be in view with the game's name above the screen.
                const ScreenBox* whole = outline.WholeNameplate(info->index);
                ScreenBox anchor       = PlaceName(*plate, whole, outline.SceneCamera(), info->feet, info->pose);
                if (!NameOnScreen(&anchor, screenWidth, screenHeight)) continue;
                // Labels and mob icons wait until the camera sees the entity well enough; a name never does.
                const bool steady = LabelVisible(plate, outline.MeshDraws(info->index), outline.PlateFramesInRow(info->index),
                    screenWidth, screenHeight);
                const bool showName  = ReplacesName(settings, *info);
                const bool showLabel = steady && settings.showLabels && info->alive && info->label.text[0] != '\0';
                const int iconCount  = steady && settings.showIcons && info->alive && !m_IconsFailed ? info->icons.count : 0;
                const bool picked    = settings.replaceCursor && info->index == cursors.subTarget;
                const bool showCursor = picked || (settings.replaceCursor && info->index == cursors.target);
                if (!showName && !showLabel && iconCount == 0 && !showCursor) continue;
                if (m_Shown.size() == kMaxPlates) break;
                Plate& p = m_Plates[info->index];
                p.frame  = m_Frame;

                // Scale smoothly like the game's names, by the height of its letters. Text is drawn at a nearby size and
                // stretched to fit, so it is redrawn only when its size moves a step.
                const float scale      = settings.scaleWithDistance ? DistanceScale(plate->maxY - plate->minY, screenHeight) : 1.0f;
                const float nameShown  = static_cast<float>(settings.nameSize) * scale * toY;
                const float labelShown = static_cast<float>(settings.labelSize) * scale * toY;
                const float iconSize   = static_cast<float>(settings.iconSize) * scale;
                const float cursorShown = static_cast<float>(settings.cursorSize) * scale * toY;
                auto raster            = [&](float shown, int current) {
                    return settings.scaleWithDistance ? RasterHeight(shown, current) : std::max(1, static_cast<int>(std::lround(shown)));
                };
                p.nameRaster              = raster(nameShown, p.nameRaster);
                p.labelRaster             = raster(labelShown, p.labelRaster);
                p.cursorRaster            = raster(cursorShown, p.cursorRaster);
                const uint32_t cursorColor = ToArgb(picked           ? settings.subCursorColor
                                                    : cursors.locked ? settings.lockedCursorColor
                                                                     : settings.cursorColor);
                const uint32_t nameColor =
                    settings.ownNameColor ? ToArgb(settings.nameColor) : outline.NameplateColor(info->index);
                const uint32_t labelColor = ToArgb(settings.labelColor[static_cast<int>(info->label.shade)]);
                if ((showName && !Prepare(p.name, info->name, nameColor, p.nameRaster, settings)) ||
                    (showLabel && !Prepare(p.label, info->label.text, labelColor, p.labelRaster, settings)) ||
                    (showCursor && !PrepareCursor(p.cursor, cursorColor, p.cursorRaster, settings)))
                {
                    m_Failed = m_FailurePending = true;
                    break;
                }
                const float nameFit  = nameShown / static_cast<float>(p.nameRaster);
                const float labelFit  = labelShown / static_cast<float>(p.labelRaster);
                const float cursorFit = cursorShown / static_cast<float>(p.cursorRaster);
                // A player's icons are as tall as the name's letters.
                const int nameIconCount = showName && settings.showPlayerIcons && !m_IconsFailed ? info->nameIcons.count : 0;
                const float nameIconSize = nameShown / toY;

                const LineSizes sizes{showName ? p.name.width * nameFit / toX : 0.0f, showName ? p.name.height * nameFit / toY : 0.0f,
                    showLabel ? p.label.width * labelFit / toX : 0.0f, showLabel ? p.label.height * labelFit / toY : 0.0f, iconCount,
                    iconSize, showCursor ? p.cursor.width * cursorFit / toX : 0.0f, showCursor ? p.cursor.height * cursorFit / toY : 0.0f,
                    p.cursor.tip, nameIconCount, nameIconSize, settings.centerNameAndIcons};
                // Over the entity: a player's name and icons are centered together, so the letters alone sit off to one side.
                if (showName)
                {
                    const float raise = static_cast<float>(settings.nameRaise) * scale;
                    anchor.minY -= raise;
                    anchor.maxY -= raise;
                }
                const NameplateLayout layout = LayoutNameplate(anchor, sizes, showName);
                const float depth            = outline.NameplateDepth(info->index);
                if (showName)
                    addQuad(m_Quads, p.name.texture, layout.nameX, layout.nameY, p.name.width * nameFit, p.name.height * nameFit, p.name.u,
                        p.name.v, depth, kWhite);
                if (showLabel)
                    addQuad(m_Quads, p.label.texture, layout.labelX, layout.labelY, p.label.width * labelFit, p.label.height * labelFit,
                        p.label.u, p.label.v, depth, kWhite);
                int drawn = 0;
                for (; drawn < iconCount; ++drawn)
                {
                    IDirect3DTexture8* icon = IconTexture(info->icons.icons[drawn]);
                    if (icon == nullptr) break;
                    addQuad(m_Quads, icon, layout.iconsX + static_cast<float>(drawn) * layout.iconStep, layout.iconsY, iconSize * toX,
                        iconSize * toY, 1.0f, 1.0f, depth, iconTint);
                }
                for (int i = 0; i < nameIconCount; ++i)
                {
                    const Icon kind         = info->nameIcons.icons[i];
                    IDirect3DTexture8* icon = IconTexture(kind);
                    if (icon == nullptr) break;
                    addQuad(m_Quads, icon, layout.nameIconsX + static_cast<float>(i) * layout.nameIconStep, layout.nameIconsY,
                        nameIconSize * toX, nameIconSize * toY, 1.0f, 1.0f, depth, kind == Icon::Linkshell ? info->linkshellArgb : kWhite);
                }
                if (showCursor)
                {
                    addQuad(cursorQuads, p.cursor.texture, layout.cursorX, layout.cursorY + CursorBob(now, sizes.cursorHeight),
                        p.cursor.width * cursorFit, p.cursor.height * cursorFit, p.cursor.u, p.cursor.v, depth, kWhite);
                    m_CursorNames.push_back(CursorName{info->index, whole != nullptr ? *whole : *plate});
                }
                auto shownHeight = [&](bool shown, float pixels) { return shown ? static_cast<int>(std::lround(pixels / toY)) : 0; };
                m_Shown.push_back(Shown{info->index, layout.nameX, layout.nameY, layout.labelX, layout.labelY, layout.iconsX,
                    layout.iconsY, shownHeight(showName, nameShown), shownHeight(showLabel, labelShown),
                    static_cast<int>(std::lround(iconSize)), drawn, nameColor});
            }
        }

        m_Quads.insert(m_Quads.end(), cursorQuads.begin(), cursorQuads.end());

        // Mobs without a nameplate this frame give their textures back.
        for (auto it = m_Plates.begin(); it != m_Plates.end();)
        {
            if (it->second.frame == m_Frame)
            {
                ++it;
                continue;
            }
            ReleaseText(it->second.name.texture);
            ReleaseText(it->second.label.texture);
            ReleaseText(it->second.cursor.texture);
            it = m_Plates.erase(it);
        }
    }

    void NameplateRenderer::Clear()
    {
        m_Quads.clear();
        m_Shown.clear();
        m_CursorNames.clear();
    }

    void NameplateRenderer::Draw(bool depthTest)
    {
        if (m_Quads.empty() || m_Device == nullptr) return;
        IDirect3DDevice8* d = m_Device;
        d->SetVertexShader(kQuadFvf);
        d->SetRenderState(D3DRS_ZENABLE, depthTest ? D3DZB_TRUE : D3DZB_FALSE);
        d->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
        d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        d->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        d->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        d->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        d->SetRenderState(D3DRS_FOGENABLE, FALSE);
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        d->SetRenderState(D3DRS_LIGHTING, FALSE);
        d->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        d->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
            D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        d->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        d->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        d->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        d->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        d->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
        d->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
        d->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
        d->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
        d->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
        d->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        d->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        for (const Quad& q : m_Quads)
        {
            const float left = q.x - kPixelCenter, top = q.y - kPixelCenter;
            const float right = left + q.width, bottom = top + q.height;
            const float z         = depthTest ? q.depth : 0.0f;
            const QuadVertex v[4] = {{left, top, z, 1.0f, q.tint, 0.0f, 0.0f}, {right, top, z, 1.0f, q.tint, q.u, 0.0f},
                {left, bottom, z, 1.0f, q.tint, 0.0f, q.v}, {right, bottom, z, 1.0f, q.tint, q.u, q.v}};
            d->SetTexture(0, q.texture);
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(QuadVertex));
        }
        d->SetTexture(0, nullptr); // the plates' textures may be released before the game binds its own
    }

    void NameplateRenderer::Release()
    {
        for (auto& [index, plate] : m_Plates)
        {
            ReleaseText(plate.name.texture);
            ReleaseText(plate.label.texture);
            ReleaseText(plate.cursor.texture);
        }
        m_Plates.clear();
        for (IDirect3DTexture8*& texture : m_IconTextures)
            ReleaseText(texture);
        m_Quads.clear();
        m_Shown.clear();
    }

    bool NameplateRenderer::TakeFailure()
    {
        const bool pending = m_FailurePending;
        m_FailurePending   = false;
        return pending;
    }

    bool NameplateRenderer::TakeIconFailure()
    {
        const bool pending   = m_IconFailurePending;
        m_IconFailurePending = false;
        return pending;
    }
}
