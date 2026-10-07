#include "nameplate_render.h"

#include "game_glyphs.h"
#include "shapes.h"
#include "text_raster.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

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
        constexpr float kOutlinePerHeight = 0.1f; // outline width, per pixel of text or cursor height
        constexpr float kPixelCenter      = 0.5f; // Direct3D 8 samples pixel centers at half-pixel offsets
        constexpr float kPercent          = 100.0f;

        int OutlineRadius(int pixelHeight)
        {
            return std::max(1, static_cast<int>(std::lround(static_cast<float>(pixelHeight) * kOutlinePerHeight)));
        }

        void ReleaseTexture(IDirect3DTexture8*& texture)
        {
            if (texture != nullptr) texture->Release();
            texture = nullptr;
        }
    }

    void NameplateRenderer::Fail(std::string what)
    {
        m_Failed = true;
        m_Failure = std::move(what);
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

    bool NameplateRenderer::Prepare(PlateTexture& t, const char* text, uint32_t color, int pixelHeight, const Settings& settings)
    {
        TextureKey key{text, settings.fontName, settings.fontBold, pixelHeight, color, ToArgb(settings.textOutline)};
        if (t.texture != nullptr && t.key == key) return true;
        const int radius = OutlineRadius(pixelHeight);
        Coverage coverage;
        if (!RasterizeText(text, settings.fontName.c_str(), pixelHeight, settings.fontBold, radius, coverage))
        {
            Fail("GDI could not draw text in " + settings.fontName + " at " + std::to_string(pixelHeight) + " px");
            return false;
        }
        return Upload(t, Outlined(coverage, radius, color, key.outline), std::move(key));
    }

    bool NameplateRenderer::PrepareCursor(PlateTexture& t, uint32_t color, int pixelHeight, const Settings& settings)
    {
        TextureKey key{"", settings.cursorFeather ? "feather" : "arrow", false, pixelHeight, color, ToArgb(settings.textOutline)};
        if (t.texture != nullptr && t.key == key) return true;
        const int radius        = OutlineRadius(pixelHeight);
        const Shape& shape      = settings.cursorFeather ? FeatherShape() : ArrowShape();
        const Coverage coverage = ShapeCoverage(shape, pixelHeight, radius);
        if (!Upload(t, Outlined(coverage, radius, color, key.outline), std::move(key))) return false;
        const float width = static_cast<float>(coverage.width - 2 * radius);
        t.tip             = (static_cast<float>(radius) + ShapeTip(shape) * width) / static_cast<float>(coverage.width);
        return true;
    }

    bool NameplateRenderer::Upload(PlateTexture& t, const Image& image, TextureKey key)
    {
        float u = 0.0f, v = 0.0f;
        IDirect3DTexture8* texture = CreateTexture(image.argb.data(), image.width, image.height, u, v);
        if (texture == nullptr)
        {
            Fail("Direct3D could not make a " + std::to_string(image.width) + "x" + std::to_string(image.height) + " texture");
            return false;
        }
        ReleaseTexture(t.texture);
        t = PlateTexture{texture, std::move(key), static_cast<float>(image.width), static_cast<float>(image.height), u, v};
        return true;
    }

    const NameplateRenderer::IconTexture* NameplateRenderer::Icon(headsup::Icon icon)
    {
        IconTexture& t = m_IconTextures[static_cast<size_t>(icon)];
        if (t.texture != nullptr) return &t;
        if (m_IconsFailed) return nullptr;
        const IconBitmap& bitmap = IconImage(icon);
        t.texture = CreateTexture(bitmap.bgra, static_cast<int>(bitmap.width), static_cast<int>(bitmap.height), t.u, t.v);
        if (t.texture != nullptr) return &t;
        m_IconsFailed = true;
        m_IconFailure = "Direct3D could not make a " + std::to_string(bitmap.width) + "x" + std::to_string(bitmap.height) +
                        " icon texture";
        return nullptr;
    }

    void NameplateRenderer::Update(const Tracker& tracker, const GameNames& names, const Settings& settings, float toX,
        float toY, const CursorTargets& targets, double now)
    {
        ++m_Frame;
        m_Shown.clear();
        m_Quads.clear();
        m_CursorNames.clear();
        const uint32_t iconTint  = ToArgb(settings.iconTint);
        const float screenWidth  = names.BackBufferWidth();
        const float screenHeight = names.BackBufferHeight();
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
                const ScreenBox* plate = names.NameplateBox(info->index);
                if (plate == nullptr) continue;
                // Checked where our name goes: a seated player's head can be in view with the game's name above the screen.
                const ScreenBox* whole = names.WholeNameplate(info->index);
                ScreenBox anchor       = PlaceName(*plate, whole, names.SceneCamera(), info->feet, info->pose);
                if (!NameOnScreen(anchor, screenWidth, screenHeight)) continue;
                const PlateLines lines = ChooseLines(PlateFacts{info->index, ReplacesName(settings, *info),
                                                         Steady(names.MeshDraws(info->index), names.PlateFramesInRow(info->index)),
                                                         info->alive, info->label.text[0] != '\0', info->icons.count, info->nameIcons.left.count,
                                                         info->nameIcons.right.count},
                    settings, targets, m_IconsFailed);
                if (!lines.Any()) continue;
                Plate& p = m_Plates[info->index];
                p.frame  = m_Frame;

                // Scale smoothly like the game's names, by the height of its letters. Text is drawn at a nearby size and
                // stretched to fit, so it is redrawn only when its size moves a step.
                const float scale       = settings.scaleWithDistance ? DistanceScale(names.NameSize(info->index), screenHeight) : 1.0f;
                const float nameShown   = static_cast<float>(settings.nameSize) * scale * toY;
                const float labelShown  = static_cast<float>(settings.labelSize) * scale * toY;
                const float iconSize    = static_cast<float>(settings.iconSize) * scale;
                const float cursorShown = static_cast<float>(settings.cursorSize) * scale * toY;
                auto raster             = [&](float shown, int current) {
                    return settings.scaleWithDistance ? RasterHeight(shown, current) : std::max(1, static_cast<int>(std::lround(shown)));
                };
                p.nameRaster   = raster(nameShown, p.nameRaster);
                p.labelRaster  = raster(labelShown, p.labelRaster);
                p.cursorRaster = raster(cursorShown, p.cursorRaster);
                const bool showCursor      = lines.cursor != CursorKind::None;
                const uint32_t cursorColor = ToArgb(lines.cursor == CursorKind::SubTarget ? settings.subCursorColor
                                                    : lines.cursor == CursorKind::Locked  ? settings.lockedCursorColor
                                                                                          : settings.cursorColor);
                const uint32_t nameColor = settings.ownNameColor ? ToArgb(settings.nameColor) : names.NameplateColor(info->index);
                const uint32_t labelColor = ToArgb(settings.labelColor[static_cast<int>(info->label.shade)]);
                if ((lines.name && !Prepare(p.name, info->name, nameColor, p.nameRaster, settings)) ||
                    (lines.label && !Prepare(p.label, info->label.text, labelColor, p.labelRaster, settings)) ||
                    (showCursor && !PrepareCursor(p.cursor, cursorColor, p.cursorRaster, settings)))
                    break;
                const float nameFit   = nameShown / static_cast<float>(p.nameRaster);
                const float labelFit  = labelShown / static_cast<float>(p.labelRaster);
                const float cursorFit = cursorShown / static_cast<float>(p.cursorRaster);
                // A player's icons are a share of the name's height.
                const float nameIconSize = nameShown / toY * static_cast<float>(settings.playerIconSize) / kPercent;

                LineSizes sizes;
                if (lines.name) sizes.nameWidth = p.name.width * nameFit / toX, sizes.nameHeight = p.name.height * nameFit / toY;
                if (lines.label) sizes.labelWidth = p.label.width * labelFit / toX, sizes.labelHeight = p.label.height * labelFit / toY;
                sizes.iconCount = lines.icons;
                sizes.iconSize  = iconSize;
                if (showCursor)
                {
                    sizes.cursorWidth  = p.cursor.width * cursorFit / toX;
                    sizes.cursorHeight = p.cursor.height * cursorFit / toY;
                    sizes.cursorTip    = p.cursor.tip;
                }
                sizes.leftIconCount      = lines.leftIcons;
                sizes.rightIconCount     = lines.rightIcons;
                sizes.nameIconSize       = nameIconSize;
                sizes.centerNameAndIcons = settings.centerNameAndIcons;
                if (lines.name)
                {
                    const float raise = static_cast<float>(settings.nameRaise) * scale;
                    anchor.minY -= raise;
                    anchor.maxY -= raise;
                }
                const NameplateLayout layout = LayoutNameplate(anchor, sizes, lines.name);
                const float depth            = names.NameplateDepth(info->index);
                if (lines.name)
                    addQuad(m_Quads, p.name.texture, layout.nameX, layout.nameY, p.name.width * nameFit, p.name.height * nameFit, p.name.u,
                        p.name.v, depth, kWhite);
                if (lines.label)
                    addQuad(m_Quads, p.label.texture, layout.labelX, layout.labelY, p.label.width * labelFit, p.label.height * labelFit,
                        p.label.u, p.label.v, depth, kWhite);
                int drawn = 0;
                for (; drawn < lines.icons; ++drawn)
                {
                    const IconTexture* icon = Icon(info->icons.icons[drawn]);
                    if (icon == nullptr) break;
                    addQuad(m_Quads, icon->texture, layout.iconsX + static_cast<float>(drawn) * layout.iconStep, layout.iconsY,
                        iconSize * toX, iconSize * toY, icon->u, icon->v, depth, iconTint);
                }
                auto addNameIcons = [&](const IconSet& row, int count, float x) {
                    for (int i = 0; i < count; ++i)
                    {
                        const headsup::Icon kind = row.icons[i];
                        const IconTexture* icon  = Icon(kind);
                        if (icon == nullptr) return;
                        addQuad(m_Quads, icon->texture, x + static_cast<float>(i) * layout.nameIconStep, layout.nameIconsY,
                            nameIconSize * toX, nameIconSize * toY, icon->u, icon->v, depth,
                            kind == headsup::Icon::Linkshell ? info->linkshellArgb : kWhite);
                    }
                };
                addNameIcons(info->nameIcons.left, lines.leftIcons, layout.leftIconsX);
                addNameIcons(info->nameIcons.right, lines.rightIcons, layout.rightIconsX);
                if (showCursor)
                {
                    addQuad(cursorQuads, p.cursor.texture, layout.cursorX, layout.cursorY + CursorBob(now, sizes.cursorHeight),
                        p.cursor.width * cursorFit, p.cursor.height * cursorFit, p.cursor.u, p.cursor.v, depth, kWhite);
                    m_CursorNames.push_back(CursorName{info->index, whole != nullptr ? *whole : *plate});
                }
                auto shownHeight = [&](bool shown, float pixels) { return shown ? static_cast<int>(std::lround(pixels / toY)) : 0; };
                m_Shown.push_back(Shown{info->index, layout.nameX, layout.nameY, layout.labelX, layout.labelY, layout.iconsX,
                    layout.iconsY, shownHeight(lines.name, nameShown), shownHeight(lines.label, labelShown),
                    static_cast<int>(std::lround(iconSize)), drawn, nameColor});
            }
        }

        // Nearer names over farther ones, then every cursor on top.
        std::stable_sort(m_Quads.begin(), m_Quads.end(), [](const Quad& a, const Quad& b) { return a.depth > b.depth; });
        m_Quads.insert(m_Quads.end(), cursorQuads.begin(), cursorQuads.end());

        // Entities without a nameplate this frame give their textures back.
        for (auto it = m_Plates.begin(); it != m_Plates.end();)
        {
            if (it->second.frame == m_Frame)
            {
                ++it;
                continue;
            }
            ReleaseTexture(it->second.name.texture);
            ReleaseTexture(it->second.label.texture);
            ReleaseTexture(it->second.cursor.texture);
            it = m_Plates.erase(it);
        }
    }

    void NameplateRenderer::Clear()
    {
        m_Quads.clear();
        m_Shown.clear();
        m_CursorNames.clear();
    }

    void NameplateRenderer::ReleasePlates()
    {
        for (auto& [index, plate] : m_Plates)
        {
            ReleaseTexture(plate.name.texture);
            ReleaseTexture(plate.label.texture);
            ReleaseTexture(plate.cursor.texture);
        }
        m_Plates.clear();
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
            const float z     = depthTest ? q.depth : 0.0f;
            const QuadVertex v[kQuadVertices] = {{left, top, z, 1.0f, q.tint, 0.0f, 0.0f}, {right, top, z, 1.0f, q.tint, q.u, 0.0f},
                {left, bottom, z, 1.0f, q.tint, 0.0f, q.v}, {right, bottom, z, 1.0f, q.tint, q.u, q.v}};
            d->SetTexture(0, q.texture);
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, kQuadVertices - 2, v, sizeof(QuadVertex));
        }
        d->SetTexture(0, nullptr); // the plates' textures may be released before the game binds its own
    }

    void NameplateRenderer::Release()
    {
        ReleasePlates();
        for (IconTexture& icon : m_IconTextures)
            ReleaseTexture(icon.texture);
        Clear();
    }

    std::string NameplateRenderer::TakeFailure() { return std::exchange(m_Failure, std::string()); }

    std::string NameplateRenderer::TakeIconFailure() { return std::exchange(m_IconFailure, std::string()); }
}
