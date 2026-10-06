#include "nameplate_render.h"

#include "nameplate.h"

#include <algorithm>
#include <cstdio>
#include <iterator>

namespace aggroglow
{
    namespace
    {
        void TextAlias(const char* kind, size_t slot, char (&out)[40])
        {
            std::snprintf(out, sizeof(out), "aggroglow_%s_%02u", kind, static_cast<unsigned>(slot));
        }

        void IconAlias(Icon icon, size_t n, char (&out)[40])
        {
            std::snprintf(out, sizeof(out), "aggroglow_icon_%02u_%02u", static_cast<unsigned>(icon), static_cast<unsigned>(n));
        }

        IFontObject* CreateText(IFontManager* fonts, const char* alias)
        {
            fonts->Delete(alias); // left over from an unclean unload
            IFontObject* font = fonts->Create(alias);
            if (font == nullptr) return nullptr;
            font->SetDrawFlags(Ashita::FontDrawFlags::Outlined);
            font->SetLocked(true);
            font->SetCanFocus(false);
            if (IPrimitiveObject* background = font->GetBackground()) background->SetVisible(false);
            font->SetVisible(false);
            return font;
        }
    }

    NameplateRenderer::Plate* NameplateRenderer::PlateFor(uint16_t index)
    {
        const auto it = m_SlotOf.find(index);
        if (it != m_SlotOf.end()) return &m_Plates[it->second];
        size_t slot = 0;
        if (!m_FreeSlots.empty())
        {
            slot = m_FreeSlots.back();
            m_FreeSlots.pop_back();
        }
        else
        {
            if (m_Plates.size() == kMaxPlates) return nullptr;
            slot = m_Plates.size();
            char nameAlias[40], labelAlias[40];
            TextAlias("name", slot, nameAlias);
            TextAlias("label", slot, labelAlias);
            Plate plate;
            plate.name.font  = CreateText(m_Fonts, nameAlias);
            plate.label.font = CreateText(m_Fonts, labelAlias);
            if (plate.name.font == nullptr || plate.label.font == nullptr)
            {
                m_Fonts->Delete(nameAlias);
                m_Fonts->Delete(labelAlias);
                m_Failed = m_FailurePending = true;
                return nullptr;
            }
            m_Plates.push_back(plate);
        }
        m_SlotOf[index] = slot;
        return &m_Plates[slot];
    }

    void NameplateRenderer::Show(Text& t, const char* text, uint32_t color, int height, const Settings& settings, SIZE& size)
    {
        const bool fresh = t.family < 0;
        if (fresh || t.family != settings.fontIndex)
        {
            t.font->SetFontFamily(FontFamily(settings.fontIndex));
            t.family = settings.fontIndex;
        }
        if (fresh || t.bold != settings.fontBold)
        {
            t.font->SetBold(settings.fontBold);
            t.bold = settings.fontBold;
        }
        const uint32_t outline = ToArgb(settings.textOutline);
        if (fresh || t.outline != outline)
        {
            t.font->SetColorOutline(outline);
            t.outline = outline;
        }
        if (t.height != height)
        {
            t.font->SetFontHeight(static_cast<uint32_t>(height));
            t.height = height;
        }
        if (t.text != text)
        {
            t.font->SetText(text);
            t.text = text;
        }
        if (t.color != color)
        {
            t.font->SetColor(color);
            t.color = color;
        }
        t.font->GetTextSize(&size);
    }

    void NameplateRenderer::Place(Text& t, float x, float y)
    {
        t.font->SetPositionX(x);
        t.font->SetPositionY(y);
        if (!t.visible) t.font->SetVisible(true);
        t.visible = true;
    }

    void NameplateRenderer::Hide(Text& t)
    {
        if (t.visible && t.font != nullptr) t.font->SetVisible(false);
        t.visible = false;
    }

    IPrimitiveObject* NameplateRenderer::IconObject(Icon icon, size_t n)
    {
        auto& pool = m_Icons[static_cast<size_t>(icon)];
        if (n < pool.size()) return pool[n];
        if (m_IconsFailed || m_Primitives == nullptr) return nullptr;
        char alias[40];
        IconAlias(icon, n, alias);
        m_Primitives->Delete(alias);
        IPrimitiveObject* object = m_Primitives->Create(alias);
        const IconPng& png       = IconImage(icon);
        if (object == nullptr || !object->SetTextureFromMemory(png.data, png.size, 0))
        {
            if (object != nullptr) m_Primitives->Delete(alias);
            m_IconsFailed = m_IconFailurePending = true;
            return nullptr;
        }
        // A primitive's size crops its texture; icons are resized by scale instead.
        object->SetWidth(static_cast<float>(png.width));
        object->SetHeight(static_cast<float>(png.height));
        object->SetLocked(true);
        object->SetCanFocus(false);
        object->SetVisible(false);
        pool.push_back(object);
        return object;
    }

    void NameplateRenderer::Update(const Tracker& tracker, const OutlineRenderer& outline, const Settings& settings)
    {
        ++m_Frame;
        m_Shown.clear();
        m_ReplacedLast = std::unordered_set<uint16_t>(m_Replacing.begin(), m_Replacing.end());
        m_Replacing.clear();
        const uint32_t iconTint = ToArgb(settings.iconTint);
        std::fill(std::begin(m_IconsUsed), std::end(m_IconsUsed), size_t{0});
        const float screenWidth  = outline.BackBufferWidth();
        const float screenHeight = outline.BackBufferHeight();
        if (NameplatesOn(settings) && !m_Failed && m_Fonts != nullptr)
        {
            for (const ActorPtr actor : tracker.Mobs())
            {
                const ActorInfo* info = tracker.Find(actor);
                if (info == nullptr) continue;
                const ScreenBox* plate = outline.NameplateBox(info->index);
                if (!LabelVisible(plate, outline.MeshDraws(info->index), outline.PlateFramesInRow(info->index), screenWidth,
                        screenHeight))
                    continue; // the camera cannot see this mob well enough
                const bool replace   = settings.replaceNameplates && info->name[0] != '\0';
                const bool showName  = replace && m_ReplacedLast.count(info->index) != 0;
                const bool showLabel = settings.showLabels && info->alive && info->label.text[0] != '\0';
                const int iconCount  = settings.showIcons && info->alive && !m_IconsFailed ? info->icons.count : 0;
                if (!replace && !showLabel && iconCount == 0) continue;
                Plate* p = PlateFor(info->index);
                if (p == nullptr) break; // 64 nameplates, or a font could not be created
                p->frame = m_Frame;
                if (replace) m_Replacing.push_back(info->index);

                // Scale like the game's names: by the height of its letters, in whole-pixel steps for the fonts.
                const float letter = plate->maxY - plate->minY;
                auto scaled        = [&](int base, int current) {
                    return settings.scaleWithDistance ? SteppedSize(current, ScaledSize(base, letter, screenHeight)) : base;
                };
                const int nameHeight  = scaled(settings.nameSize, p->name.height);
                const int labelHeight = scaled(settings.labelSize, p->label.height);
                const int iconSize    = settings.scaleWithDistance ? ScaledSize(settings.iconSize, letter, screenHeight) : settings.iconSize;
                const uint32_t nameColor =
                    settings.ownNameColor ? ToArgb(settings.nameColor) : outline.NameplateColor(info->index);
                const uint32_t labelColor = ToArgb(settings.labelColor[static_cast<int>(info->label.shade)]);

                SIZE nameSize{}, labelSize{};
                if (showName)
                    Show(p->name, info->name, nameColor, nameHeight, settings, nameSize);
                else
                    Hide(p->name);
                if (showLabel)
                    Show(p->label, info->label.text, labelColor, labelHeight, settings, labelSize);
                else
                    Hide(p->label);

                const LineSizes sizes{showName ? static_cast<float>(nameSize.cx) : 0.0f, showName ? static_cast<float>(nameSize.cy) : 0.0f,
                    showLabel ? static_cast<float>(labelSize.cx) : 0.0f, showLabel ? static_cast<float>(labelSize.cy) : 0.0f, iconCount,
                    static_cast<float>(iconSize)};
                const NameplateLayout layout = LayoutNameplate(*plate, sizes, showName);
                if (showName) Place(p->name, layout.nameX, layout.nameY);
                if (showLabel) Place(p->label, layout.labelX, layout.labelY);
                int drawn = 0;
                for (int i = 0; i < iconCount; ++i)
                {
                    const Icon kind        = info->icons.icons[i];
                    IPrimitiveObject* icon = IconObject(kind, m_IconsUsed[static_cast<size_t>(kind)]);
                    if (icon == nullptr) break;
                    ++m_IconsUsed[static_cast<size_t>(kind)];
                    icon->SetPositionX(layout.iconsX + static_cast<float>(i) * layout.iconStep);
                    icon->SetPositionY(layout.iconsY);
                    const IconPng& png = IconImage(kind);
                    icon->SetScaleX(static_cast<float>(iconSize) / static_cast<float>(png.width));
                    icon->SetScaleY(static_cast<float>(iconSize) / static_cast<float>(png.height));
                    icon->SetColor(iconTint);
                    icon->SetVisible(true);
                    ++drawn;
                }
                m_Shown.push_back(Shown{info->index, layout.nameX, layout.nameY, layout.labelX, layout.labelY, layout.iconsX,
                    layout.iconsY, showName ? nameHeight : 0, showLabel ? labelHeight : 0, iconSize, drawn, nameColor});
            }
        }

        // Mobs whose nameplate is not shown this frame give their font objects back.
        for (auto it = m_SlotOf.begin(); it != m_SlotOf.end();)
        {
            Plate& p = m_Plates[it->second];
            if (p.frame == m_Frame)
            {
                ++it;
                continue;
            }
            Hide(p.name);
            Hide(p.label);
            m_FreeSlots.push_back(it->second);
            it = m_SlotOf.erase(it);
        }
        for (int kind = 0; kind < kIconCount; ++kind)
            for (size_t n = m_IconsUsed[kind]; n < m_Icons[kind].size(); ++n)
                m_Icons[kind][n]->SetVisible(false);
    }

    void NameplateRenderer::Release()
    {
        char alias[40];
        if (m_Fonts != nullptr)
        {
            for (size_t slot = 0; slot < m_Plates.size(); ++slot)
            {
                TextAlias("name", slot, alias);
                m_Fonts->Delete(alias);
                TextAlias("label", slot, alias);
                m_Fonts->Delete(alias);
            }
        }
        if (m_Primitives != nullptr)
        {
            for (int kind = 0; kind < kIconCount; ++kind)
            {
                for (size_t n = 0; n < m_Icons[kind].size(); ++n)
                {
                    IconAlias(static_cast<Icon>(kind), n, alias);
                    m_Primitives->Delete(alias);
                }
            }
        }
        m_Plates.clear();
        m_SlotOf.clear();
        m_FreeSlots.clear();
        for (auto& pool : m_Icons)
            pool.clear();
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
