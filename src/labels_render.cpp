#include "labels_render.h"

#include "nameplate.h"

#include <cstdio>

namespace aggroglow
{
    namespace
    {
        constexpr float kGap = 2.0f; // pixels between the label and the nameplate

        void Alias(size_t i, char (&out)[32])
        {
            std::snprintf(out, sizeof(out), "aggroglow_label_%02u", static_cast<unsigned>(i));
        }
    }

    IFontObject* LabelRenderer::Object(size_t i)
    {
        if (i < m_Objects.size()) return m_Objects[i];
        char alias[32];
        Alias(i, alias);
        m_Fonts->Delete(alias); // left over from an unclean unload
        IFontObject* font = m_Fonts->Create(alias);
        if (font == nullptr)
        {
            m_Failed = m_FailurePending = true;
            return nullptr;
        }
        font->SetFontFamily("Arial");
        font->SetFontHeight(14);
        font->SetBold(true);
        font->SetDrawFlags(Ashita::FontDrawFlags::Outlined);
        font->SetColorOutline(0xFF000000);
        font->SetLocked(true);
        if (IPrimitiveObject* background = font->GetBackground()) background->SetVisible(false);
        font->SetVisible(false);
        m_Objects.push_back(font);
        m_Texts.emplace_back();
        m_Colors.push_back(0);
        return font;
    }

    void LabelRenderer::Update(const Tracker& tracker, const OutlineRenderer& outline, bool show)
    {
        m_Shown.clear();
        m_TextChanges = 0;
        size_t used   = 0;
        if (show && !m_Failed && m_Fonts != nullptr)
        {
            for (const ActorPtr actor : tracker.OutlinedActors())
            {
                if (used == kMaxLabels) break;
                const ActorInfo* info  = tracker.Find(actor);
                const ScreenBox* plate = info != nullptr ? outline.NameplateBox(info->index) : nullptr;
                if (info == nullptr || info->label.text[0] == '\0') continue;
                if (!LabelVisible(plate, outline.MeshDraws(info->index), outline.PlateFramesInRow(info->index),
                        outline.BackBufferWidth(), outline.BackBufferHeight()))
                    continue; // the camera cannot see this mob well enough
                IFontObject* font = Object(used);
                if (font == nullptr) break;
                if (m_Texts[used] != info->label.text)
                {
                    font->SetText(info->label.text);
                    m_Texts[used] = info->label.text;
                    ++m_TextChanges;
                }
                if (m_Colors[used] != info->label.argb)
                {
                    font->SetColor(info->label.argb);
                    m_Colors[used] = info->label.argb;
                }
                SIZE size{};
                font->GetTextSize(&size);
                float x = 0.0f, y = 0.0f;
                PlaceAbove(*plate, static_cast<float>(size.cx), static_cast<float>(size.cy), kGap, x, y);
                font->SetPositionX(x);
                font->SetPositionY(y);
                font->SetVisible(true);
                m_Shown.push_back(Shown{info->index, x, y, static_cast<float>(size.cx), static_cast<float>(size.cy)});
                ++used;
            }
        }
        for (size_t i = used; i < m_Objects.size(); ++i)
            m_Objects[i]->SetVisible(false);
    }

    void LabelRenderer::Release()
    {
        if (m_Fonts != nullptr)
        {
            for (size_t i = 0; i < m_Objects.size(); ++i)
            {
                char alias[32];
                Alias(i, alias);
                m_Fonts->Delete(alias);
            }
        }
        m_Objects.clear();
        m_Texts.clear();
        m_Colors.clear();
        m_Shown.clear();
    }

    bool LabelRenderer::TakeFailure()
    {
        const bool pending = m_FailurePending;
        m_FailurePending   = false;
        return pending;
    }
}
