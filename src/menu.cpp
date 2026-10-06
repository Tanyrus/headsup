#include "menu.h"

#include "Ashita.h"

#include <cstdio>

namespace aggroglow
{
    namespace
    {
        const char* const kCategoryLabels[kCategoryCount] = {"Will attack", "Won't attack", "Unknown (no mob data)",
            "NM will attack", "NM won't attack"};
        const char* const kShowIds[kCategoryCount]  = {"##showWillAttack", "##showWontAttack", "##showUnknown",
            "##showNmWillAttack", "##showNmWontAttack"};
        const char* const kColorIds[kCategoryCount] = {"##colorWillAttack", "##colorWontAttack", "##colorUnknown",
            "##colorNmWillAttack", "##colorNmWontAttack"};
        // Rows top to bottom: ordinary mobs, notorious monsters, then mobs with no data.
        const int kRowOrder[kCategoryCount] = {0, 1, 3, 4, 2};
        const char* const kShadeLabels[kLabelShadeCount] = {"Unknown level", "Too Weak", "Easy Prey", "Decent Challenge",
            "Even Match", "Tough", "Very Tough"};
        constexpr int kColorsPerRow                       = 3;
        constexpr float kColorColumnWidth                 = 170.0f;
        constexpr float kSliderWidth                      = 240.0f;
    }

    bool Menu::Draw(IGuiManager* gui, Settings& s, const MenuStatus& status)
    {
        if (!open || gui == nullptr) return false;
        bool save = false;

        if (gui->Begin("AggroGlow", &open, ImGuiWindowFlags_AlwaysAutoResize))
        {
            save |= gui->Checkbox("Enable outlines", &s.enabled);
            gui->Separator();

            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderFloat("Thickness", &s.thickness, kMinThickness, kMaxThickness, "%.1f px",
                ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderInt("Smoothness", &s.smoothness, kMinSmoothness, kMaxSmoothness, "%d copies",
                ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderFloat("Max distance", &s.maxDistance, kMinOutlineDistance, kMaxOutlineDistance, "%.0f yalms",
                ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->Separator();

            for (const int c : kRowOrder)
            {
                save |= gui->Checkbox(kShowIds[c], &s.show[c]);
                gui->SameLine();
                gui->ColorEdit3(kColorIds[c], s.color[c].v, ImGuiColorEditFlags_NoInputs);
                save |= gui->IsItemDeactivatedAfterEdit();
                gui->SameLine();
                gui->TextUnformatted(kCategoryLabels[c]);
            }
            gui->Separator();

            gui->TextUnformatted("Nameplates");
            save |= gui->Checkbox("Replace game nameplates", &s.replaceNameplates);
            save |= gui->Checkbox("Show icons", &s.showIcons);
            save |= gui->Checkbox("Show level and con above names", &s.showLabels);
            save |= gui->Checkbox("Scale with distance", &s.scaleWithDistance);
            // The slider's text is the font's name: ImGui's combo box and selectables are overloaded (tools/abi_check.py).
            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderInt("Font", &s.fontIndex, 0, kFontCount - 1, FontFamily(s.fontIndex),
                ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->SameLine();
            save |= gui->Checkbox("Bold", &s.fontBold);
            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderInt("Name size", &s.nameSize, kMinTextSize, kMaxTextSize, "%d px", ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderInt("Level and con size", &s.labelSize, kMinTextSize, kMaxTextSize, "%d px",
                ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->SetNextItemWidth(kSliderWidth);
            gui->SliderInt("Icon size", &s.iconSize, kMinTextSize, kMaxTextSize, "%d px", ImGuiSliderFlags_AlwaysClamp);
            save |= gui->IsItemDeactivatedAfterEdit();
            save |= gui->Checkbox("##ownNameColor", &s.ownNameColor);
            gui->SameLine();
            gui->ColorEdit3("Own name color (off: the game's)", s.nameColor.v, ImGuiColorEditFlags_NoInputs);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->ColorEdit3("Text outline", s.textOutline.v, ImGuiColorEditFlags_NoInputs);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->SameLine(kColorColumnWidth);
            gui->ColorEdit3("Icon tint", s.iconTint.v, ImGuiColorEditFlags_NoInputs);
            save |= gui->IsItemDeactivatedAfterEdit();
            gui->TextUnformatted("Level and con colors");
            for (int k = 0; k < kLabelShadeCount; ++k)
            {
                if (k % kColorsPerRow != 0) gui->SameLine(static_cast<float>(k % kColorsPerRow) * kColorColumnWidth);
                gui->ColorEdit3(kShadeLabels[k], s.labelColor[k].v, ImGuiColorEditFlags_NoInputs);
                save |= gui->IsItemDeactivatedAfterEdit();
            }
            gui->Separator();

            save |= gui->Checkbox("Auto-examine the mob you target", &s.autoExamine);
            gui->BeginDisabled(true);
            gui->TextUnformatted("Checks a targeted mob that gives exp, once per respawn time.");
            gui->TextUnformatted("The checker addon still prints its own line for these checks.");
            gui->EndDisabled();
            gui->Separator();

            char line[128];
            std::snprintf(line, sizeof(line), "%u mobs outlined, %u meshes, %u nameplates, frame %.2f ms",
                status.outlinedMobs, status.meshes, status.nameplates, status.frameMs);
            gui->BeginDisabled(true);
            gui->TextUnformatted(line);
            gui->EndDisabled();
            if (!status.stencilAvailable)
                gui->TextUnformatted("Outlines unavailable: the depth buffer has no stencil bits.");
        }
        gui->End();

        s = Clamp(s);
        return save;
    }
}
