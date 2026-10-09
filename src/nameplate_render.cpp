#include "nameplate_render.h"

#include "argb.h"
#include "utf16.h"

#include <algorithm>
#include <chrono>
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
        constexpr float kOutlinePerHeight = 0.1f; // the cursor's outline width, per pixel of its height
        constexpr float kPixelCenter      = 0.5f; // Direct3D 8 samples pixel centers at half-pixel offsets
        constexpr float kPercent          = 100.0f;
        // From the mockup's CSS: a 22 px name's shadows blur 2 and 3 px, 1 px lower, and its glow 14 px.
        constexpr float kShadowPerHeight     = 0.12f; // the shadow's blur radius, per pixel of text height
        constexpr float kShadowDropPerHeight = 0.05f;
        constexpr float kGlowPerHeight       = 0.64f; // the glow's blur radius, per pixel of name height
        constexpr float kOrnamentShadowPerHeight = 0.3f; // the diamond's shadow blur: 3 px on a 9 px ornament

        uint32_t CursorColor(CursorKind kind, const Settings& settings)
        {
            return ToArgb(kind == CursorKind::SubTarget    ? settings.subCursorColor
                          : kind == CursorKind::OutOfRange ? settings.outOfRangeCursorColor
                          : kind == CursorKind::Locked     ? settings.lockedCursorColor
                                                           : settings.cursorColor);
        }

        // Adds the time from its making to its end to spent.
        class Stopwatch
        {
        public:
            explicit Stopwatch(double& spentMs) : m_Spent(spentMs), m_Start(std::chrono::steady_clock::now()) {}
            ~Stopwatch() { m_Spent += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_Start).count(); }
            Stopwatch(const Stopwatch&)            = delete;
            Stopwatch& operator=(const Stopwatch&) = delete;

        private:
            double& m_Spent;
            std::chrono::steady_clock::time_point m_Start;
        };

        // A texture is scaled from the height it was drawn at, which lags a size step while it waits to be redrawn.
        float FitOf(float shown, int drawnHeight)
        {
            return drawnHeight > 0 ? shown / static_cast<float>(drawnHeight) : 0.0f;
        }

        int ExactHeight(float shown)
        {
            return std::max(1, static_cast<int>(std::lround(shown)));
        }

        int OutlineRadius(int pixelHeight)
        {
            return std::max(1, static_cast<int>(std::lround(static_cast<float>(pixelHeight) * kOutlinePerHeight)));
        }

        void ReleaseTexture(IDirect3DTexture8*& texture)
        {
            if (texture != nullptr) texture->Release();
            texture = nullptr;
        }

        // markX is where the text from byte markAt on starts.
        bool RasterizeText(const char* text, const char* family, int pixelHeight, bool bold, int margin, size_t markAt, Coverage& out,
            int& markX)
        {
            markX    = kNoMarkColumn;
            out      = Coverage{};
            HDC dc   = CreateCompatibleDC(nullptr);
            // A negative height asks for the character height, without the font's internal leading.
            HFONT font = CreateFontW(-pixelHeight, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, Utf16(family).c_str());
            bool drawn = false;
            if (dc != nullptr && font != nullptr)
            {
                const HGDIOBJ oldFont = SelectObject(dc, font);
                const std::wstring wide = Utf16(text);
                const auto length       = static_cast<int>(wide.size());
                SIZE size{};
                GetTextExtentPoint32W(dc, wide.c_str(), length, &size);
                if (markAt != Label::kNoMark)
                {
                    const std::wstring before = Utf16(std::string(text).substr(0, markAt));
                    SIZE start{};
                    GetTextExtentPoint32W(dc, before.c_str(), static_cast<int>(before.size()), &start);
                    markX = margin + start.cx;
                }
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
                if (bitmap != nullptr && bits != nullptr)
                {
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
                    drawn = true;
                }
                if (bitmap != nullptr) DeleteObject(bitmap);
                SelectObject(dc, oldFont);
            }
            if (font != nullptr) DeleteObject(font);
            if (dc != nullptr) DeleteDC(dc);
            return drawn;
        }
    }

    void NameplateRenderer::FailNames(std::string what)
    {
        m_NamesFailed = true;
        m_NameFailure = std::move(what);
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

    bool NameplateRenderer::Prepare(PlateTexture& t, const char* text, const TextLook& look, int pixelHeight)
    {
        TextureKey key{text, pixelHeight, 0, look};
        if (t.texture != nullptr && t.key == key) return true;
        if (!RedrawNow(t.texture != nullptr, m_RedrawMs)) return true;
        const Stopwatch stopwatch(m_RedrawMs);
        const auto height    = static_cast<float>(pixelHeight);
        const float shadow   = height * kShadowPerHeight;
        const int drop       = static_cast<int>(std::lround(height * kShadowDropPerHeight));
        const float glowBlur = look.glow != 0 ? height * kGlowPerHeight * look.glowSize : 0.0f;
        const int margin     = BlurMargin(std::max(glowBlur, shadow + static_cast<float>(drop)));
        Coverage coverage;
        int markX = kNoMarkColumn;
        if (!RasterizeText(text, look.line.font.c_str(), pixelHeight, look.line.bold, margin, look.markAt, coverage, markX))
        {
            FailNames("GDI could not draw text in " + look.line.font + " at " + std::to_string(pixelHeight) + " px");
            return false;
        }
        const Layers layers{shadow, drop, glowBlur, look.color, look.line.shadowColor, look.glow, look.glowStrength,
            look.line.shadowStrength, markX, look.markColor};
        if (!Upload(t, Styled(coverage, layers), std::move(key))) return false;
        t.halo = static_cast<float>(margin);
        return true;
    }

    bool NameplateRenderer::PrepareOrnament(PlateTexture& t, uint32_t color, int pixelHeight, int width, const LineStyle& shadowLine)
    {
        TextureKey key{"", pixelHeight, width, TextLook{shadowLine, color}};
        if (t.texture != nullptr && t.key == key) return true;
        if (!RedrawNow(t.texture != nullptr, m_RedrawMs)) return true;
        const Stopwatch stopwatch(m_RedrawMs);
        const float shadow = static_cast<float>(pixelHeight) * kOrnamentShadowPerHeight;
        if (!Upload(t, Ornament(width, pixelHeight, shadow, shadowLine.shadowStrength, color, shadowLine.shadowColor), std::move(key)))
            return false;
        t.halo = static_cast<float>(BlurMargin(shadow));
        return true;
    }

    bool NameplateRenderer::PrepareCursor(PlateTexture& t, uint32_t color, int pixelHeight, const Settings& settings)
    {
        const LineStyle shape{settings.cursorFeather ? "feather" : "arrow", false, ToArgb(settings.textOutline)};
        TextureKey key{"", pixelHeight, 0, TextLook{shape, color}};
        if (t.texture != nullptr && t.key == key) return true;
        if (!RedrawNow(t.texture != nullptr, m_RedrawMs)) return true;
        const Stopwatch stopwatch(m_RedrawMs);
        const int radius        = OutlineRadius(pixelHeight);
        const Shape& outline    = settings.cursorFeather ? FeatherShape() : ArrowShape();
        const Coverage coverage = ShapeCoverage(outline, pixelHeight, radius);
        if (!Upload(t, Outlined(coverage, radius, color, shape.shadowColor), std::move(key))) return false;
        const float width = static_cast<float>(coverage.width - 2 * radius);
        t.tip             = (static_cast<float>(radius) + ShapeTip(outline) * width) / static_cast<float>(coverage.width);
        return true;
    }

    bool NameplateRenderer::Upload(PlateTexture& t, const Image& image, TextureKey key)
    {
        float u = 0.0f, v = 0.0f;
        IDirect3DTexture8* texture = CreateTexture(image.argb.data(), image.width, image.height, u, v);
        if (texture == nullptr)
        {
            FailNames("Direct3D could not make a " + std::to_string(image.width) + "x" + std::to_string(image.height) + " texture");
            return false;
        }
        ReleaseTexture(t.texture);
        t = PlateTexture{texture, std::move(key), static_cast<float>(image.width), static_cast<float>(image.height), u, v};
        return true;
    }

    const NameplateRenderer::IconTexture* NameplateRenderer::IconTextureFor(Icon icon)
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

    void NameplateRenderer::Update(const Tracker& tracker, const GameNames& names, const NameHook& hook, const Settings& settings, float toX,
        float toY, const CursorTargets& targets, double now, uint16_t selfIndex, const std::vector<TimerLine>& selfTimers)
    {
        ++m_Frame;
        m_RedrawMs = 0.0;
        m_Shown.clear();
        m_Quads.clear();
        m_CursorNames.clear();
        const uint32_t iconTint  = ToArgb(settings.iconTint);
        const float screenWidth  = names.BackBufferWidth();
        const float screenHeight = names.BackBufferHeight();
        // Cursors are drawn after every nameplate, so another name never covers one.
        std::vector<Quad> cursorQuads;
        int rank     = kPlainRank; // of the plate whose quads are being added
        auto addQuad = [&](std::vector<Quad>& into, IDirect3DTexture8* texture, float x, float y, float width, float height, float u,
                           float v, float depth, uint32_t tint) {
            // Whole pixels in the target keep the text as sharp as it was drawn.
            into.push_back(Quad{texture, std::round(x * toX), std::round(y * toY), width, height, u, v, depth, tint, rank});
        };
        if (NameplatesOn(settings) && !m_NamesFailed && m_Device != nullptr)
        {
            for (const ActorPtr actor : tracker.Actors())
            {
                const ActorInfo* info = tracker.Find(actor);
                if (info == nullptr) continue;
                const NameFrame* frame = hook.Drawn(info->index);
                if (frame == nullptr) continue;
                const DrawnName gameName = NameFromFrame(*frame, names.TargetScaleX(), names.TargetScaleY());
                // Checked where our name goes: a seated player's head can be in view with the game's name above the screen.
                ScreenBox anchor = PlaceName(gameName.box, names.SceneCamera(), info->feet, info->pose);
                if (!NameOnScreen(anchor, screenWidth, screenHeight)) continue;
                // The game drawing the name this frame is all a mob's lines wait for.
                const PlateLines lines = ChooseLines(*info, tracker.Player().engaged, settings, targets, m_IconsFailed);
                const bool timersHere = info->index == selfIndex && !selfTimers.empty();
                if (!lines.Any() && !timersHere) continue;
                Plate& p = m_Plates[info->index];
                rank     = PlateRank(info->index, targets);
                p.frame  = m_Frame;

                const bool self         = info->index == selfIndex;
                const bool scales       = ScalesWithDistance(settings, info->kind, self);
                p.scale                 = EaseScale(p.scale, DistanceScale(gameName.box.Height(), screenHeight), now - p.scaleTime);
                p.scaleTime             = now;
                const float scale       = scales ? p.scale : 1.0f;
                const float nameShown   = static_cast<float>(NameSize(settings, info->kind, self)) * scale * toY;
                const float labelShown  = static_cast<float>(settings.labelSize) * scale * toY;
                const float timerShown  = static_cast<float>(settings.timerSize) * scale * toY;
                const float iconSize    = static_cast<float>(settings.iconSize) * scale;
                const float cursorShown = static_cast<float>(settings.cursorSize) * (settings.scaleCursor ? p.scale : 1.0f) * toY;
                auto raster             = [&](float shown, int current) {
                    return scales ? RasterHeight(shown, current) : ExactHeight(shown);
                };
                p.nameRaster   = raster(nameShown, p.nameRaster);
                p.labelRaster  = raster(labelShown, p.labelRaster);
                p.cursorRaster = settings.scaleCursor ? RasterHeight(cursorShown, p.cursorRaster) : ExactHeight(cursorShown);
                const float ornamentShown = static_cast<float>(settings.ornamentThickness) * scale * toY;
                p.ornamentRaster          = raster(ornamentShown, p.ornamentRaster);
                const int ornamentWidth   = std::max(1, static_cast<int>(std::lround(static_cast<float>(p.ornamentRaster) *
                                                                                   static_cast<float>(settings.ornamentWidth) /
                                                                                   static_cast<float>(settings.ornamentThickness))));
                const bool showCursor      = lines.cursor != CursorKind::None;
                const uint32_t cursorColor = CursorColor(lines.cursor, settings);
                const uint32_t nameColor = settings.ownNameColor ? ToArgb(settings.nameColor) : gameName.color;
                const uint32_t labelColor = ToArgb(settings.labelColor[static_cast<int>(info->label.shade)]);
                const PlateStyle style    = StyleFor(*info, lines.label, settings);
                const LineStyle nameLine  = NameLineStyle(settings);
                TextLook nameLook{nameLine, nameColor};
                if (style.glow)
                {
                    nameLook.glow         = style.glowColor;
                    nameLook.glowStrength = style.glowStrength;
                    nameLook.glowSize     = style.glowSize;
                }
                TextLook labelLook{LevelLineStyle(settings), labelColor};
                labelLook.markAt    = info->label.markAt;
                labelLook.markColor = Lighter(ToArgb(settings.color[CategoryIndex(Category::Placeholder)]));
                if ((lines.name && !Prepare(p.name, info->name, nameLook, p.nameRaster)) ||
                    (lines.label && !Prepare(p.label, info->label.text, labelLook, p.labelRaster)) ||
                    (style.ornament && !PrepareOrnament(p.ornament, style.ornamentColor, p.ornamentRaster, ornamentWidth, nameLine)) ||
                    (showCursor && !PrepareCursor(p.cursor, cursorColor, p.cursorRaster, settings)))
                    break;
                const float nameFit     = FitOf(nameShown, p.name.key.height);
                const float labelFit    = FitOf(labelShown, p.label.key.height);
                const float cursorFit   = FitOf(cursorShown, p.cursor.key.height);
                const float ornamentFit = FitOf(ornamentShown, p.ornament.key.height);
                for (size_t i = timersHere ? selfTimers.size() : 0; i < p.timers.size(); ++i)
                    ReleaseTexture(p.timers[i].texture);
                p.timers.resize(timersHere ? selfTimers.size() : 0);
                p.timerRaster          = timersHere ? raster(timerShown, p.timerRaster) : 0;
                const uint32_t upColor = ToArgb(settings.color[CategoryIndex(Category::Placeholder)]);
                int timersReady        = 0;
                for (; timersReady < static_cast<int>(p.timers.size()); ++timersReady)
                {
                    const TimerLine& line = selfTimers[timersReady];
                    const TextLook timerLook{labelLook.line, line.up ? upColor : kWhite};
                    if (!Prepare(p.timers[timersReady], line.text.c_str(), timerLook, p.timerRaster)) break;
                }
                const float playerIconSize = nameShown / toY * static_cast<float>(settings.playerIconSize) / kPercent;

                LineSizes sizes;
                if (lines.name)
                {
                    sizes.nameWidth  = (p.name.width - 2.0f * p.name.halo) * nameFit / toX;
                    sizes.nameHeight = (p.name.height - 2.0f * p.name.halo) * nameFit / toY;
                }
                if (style.ornament)
                {
                    sizes.ornamentWidth  = (p.ornament.width - 2.0f * p.ornament.halo) * ornamentFit / toX;
                    sizes.ornamentHeight = (p.ornament.height - 2.0f * p.ornament.halo) * ornamentFit / toY;
                }
                if (lines.label)
                {
                    sizes.labelWidth  = (p.label.width - 2.0f * p.label.halo) * labelFit / toX;
                    sizes.labelHeight = (p.label.height - 2.0f * p.label.halo) * labelFit / toY;
                }
                sizes.mobIconCount = lines.mobIconCount;
                sizes.iconSize     = iconSize;
                if (showCursor)
                {
                    sizes.cursorWidth  = p.cursor.width * cursorFit / toX;
                    sizes.cursorHeight = p.cursor.height * cursorFit / toY;
                    sizes.cursorTip    = p.cursor.tip;
                }
                sizes.leftIconCount      = lines.leftIconCount;
                sizes.rightIconCount     = lines.rightIconCount;
                sizes.playerIconSize     = playerIconSize;
                sizes.centerNameAndIcons = settings.centerNameAndIcons;
                sizes.timerCount         = timersReady;
                for (int i = 0; i < timersReady; ++i)
                    sizes.timerHeight = std::max(sizes.timerHeight,
                        (p.timers[i].height - 2.0f * p.timers[i].halo) * FitOf(timerShown, p.timers[i].key.height) / toY);
                if (lines.name)
                {
                    const float raise = static_cast<float>(settings.nameRaise) * scale;
                    anchor.minY -= raise;
                    anchor.maxY -= raise;
                }
                const NameplateLayout layout = LayoutNameplate(anchor, sizes, lines.name);
                const float depth            = gameName.depth;
                auto addText = [&](const PlateTexture& t, float x, float y, float fit) {
                    addQuad(m_Quads, t.texture, x - t.halo * fit / toX, y - t.halo * fit / toY, t.width * fit, t.height * fit, t.u, t.v,
                        depth, kWhite);
                };
                if (lines.name) addText(p.name, layout.nameX, layout.nameY, nameFit);
                if (style.ornament) addText(p.ornament, layout.ornamentX, layout.ornamentY, ornamentFit);
                if (lines.label) addText(p.label, layout.labelX, layout.labelY, labelFit);
                int drawn = 0;
                for (; drawn < lines.mobIconCount; ++drawn)
                {
                    const IconTexture* icon = IconTextureFor(info->mobIcons.icons[drawn]);
                    if (icon == nullptr) break;
                    addQuad(m_Quads, icon->texture, layout.iconsX + static_cast<float>(drawn) * layout.iconStep, layout.iconsY,
                        iconSize * toX, iconSize * toY, icon->u, icon->v, depth, iconTint);
                }
                auto addPlayerIcons = [&](const IconSet& row, int count, float x) {
                    for (int i = 0; i < count; ++i)
                    {
                        const Icon kind         = row.icons[i];
                        const IconTexture* icon = IconTextureFor(kind);
                        if (icon == nullptr) return;
                        addQuad(m_Quads, icon->texture, x + static_cast<float>(i) * layout.playerIconStep, layout.playerIconsY,
                            playerIconSize * toX, playerIconSize * toY, icon->u, icon->v, depth,
                            kind == Icon::Linkshell ? info->linkshellArgb : kWhite);
                    }
                };
                addPlayerIcons(info->playerIcons.left, lines.leftIconCount, layout.leftIconsX);
                addPlayerIcons(info->playerIcons.right, lines.rightIconCount, layout.rightIconsX);
                if (showCursor)
                {
                    addQuad(cursorQuads, p.cursor.texture, layout.cursor.x, layout.cursor.y + CursorBob(now, sizes.cursorHeight),
                        p.cursor.width * cursorFit, p.cursor.height * cursorFit, p.cursor.u, p.cursor.v, depth, kWhite);
                    m_CursorNames.push_back(CursorName{info->index, gameName.box});
                }
                for (int i = 0; i < sizes.timerCount; ++i)
                {
                    const PlateTexture& t = p.timers[i];
                    const float timerFit  = FitOf(timerShown, t.key.height);
                    addText(t, layout.centerX - (t.width - 2.0f * t.halo) * timerFit / toX / 2.0f,
                        layout.timersY + static_cast<float>(i) * layout.timerStep, timerFit);
                }
                auto shownHeight = [&](bool shown, float pixels) { return shown ? static_cast<int>(std::lround(pixels / toY)) : 0; };
                m_Shown.push_back(Shown{info->index, layout.nameX, layout.nameY, layout.labelX, layout.labelY, layout.iconsX,
                    layout.iconsY, shownHeight(lines.name, nameShown), shownHeight(lines.label, labelShown),
                    static_cast<int>(std::lround(iconSize)), drawn, nameColor});
            }
        }

        // A lone cursor is drawn at exactly its own size and on top: there is no name to scale by or to sit behind walls with.
        if (NameplatesOn(settings) && !m_NamesFailed && m_Device != nullptr)
        {
            std::vector<uint16_t> withNameplateCursor;
            for (const CursorName& c : m_CursorNames)
                withNameplateCursor.push_back(c.index);
            for (const LoneCursor& lone : LoneCursors(targets, settings, withNameplateCursor))
            {
                Plate& p                = m_Plates[lone.index];
                p.frame                 = m_Frame;
                const float cursorShown = static_cast<float>(settings.cursorSize) * toY;
                p.cursorRaster          = ExactHeight(cursorShown);
                if (!PrepareCursor(p.cursor, CursorColor(lone.kind, settings), p.cursorRaster, settings)) break;
                const float fit     = FitOf(cursorShown, p.cursor.key.height);
                const float height  = p.cursor.height * fit / toY;
                const CursorSpot at = CursorAtAnchor(lone.x, lone.y, p.cursor.width * fit / toX, height, p.cursor.tip);
                addQuad(cursorQuads, p.cursor.texture, at.x, at.y + CursorBob(now, height), p.cursor.width * fit,
                    p.cursor.height * fit, p.cursor.u, p.cursor.v, 0.0f, kWhite);
                m_CursorNames.push_back(CursorName{lone.index, ScreenBox{}});
            }
        }

        std::stable_sort(m_Quads.begin(), m_Quads.end(),
            [](const Quad& a, const Quad& b) { return DrawnBefore(a.rank, a.depth, b.rank, b.depth); });
        m_Quads.insert(m_Quads.end(), cursorQuads.begin(), cursorQuads.end());

        for (auto it = m_Plates.begin(); it != m_Plates.end();)
        {
            if (it->second.frame == m_Frame)
            {
                ++it;
                continue;
            }
            ReleasePlate(it->second);
            it = m_Plates.erase(it);
        }
    }

    void NameplateRenderer::ReleasePlate(Plate& plate)
    {
        ReleaseTexture(plate.name.texture);
        ReleaseTexture(plate.label.texture);
        ReleaseTexture(plate.cursor.texture);
        ReleaseTexture(plate.ornament.texture);
        for (PlateTexture& timer : plate.timers)
            ReleaseTexture(timer.texture);
        plate.timers.clear();
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
            ReleasePlate(plate);
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

    std::string NameplateRenderer::TakeNameFailure() { return std::exchange(m_NameFailure, std::string()); }

    std::string NameplateRenderer::TakeIconFailure() { return std::exchange(m_IconFailure, std::string()); }
}
