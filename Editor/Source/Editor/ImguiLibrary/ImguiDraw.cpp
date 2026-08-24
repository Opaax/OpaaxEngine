#include "Editor/ImguiLibrary/ImguiDraw.h"

#include "Editor/ImguiLibrary/ImguiLayout.h"

namespace Opaax::Editor::ImguiDraw
{
    namespace
    {
        constexpr ImU32 FOLDER_COLOR = IM_COL32(232, 196, 104, 255);   // gold, Explorer-ish
        constexpr ImU32 CARD_FILL    = IM_COL32(80, 80, 92, 255);
        constexpr ImU32 CARD_BORDER  = IM_COL32(0, 0, 0, 120);
        constexpr ImU32 GLYPH_TEXT   = IM_COL32(232, 232, 232, 255);
        constexpr float CARD_INSET   = 0.16f;
        constexpr float CARD_ROUND   = 4.f;
    }

    ImU32 Darken(const ImU32 InColor, const float InFactor) noexcept
    {
        const int lR = static_cast<int>(((InColor >> IM_COL32_R_SHIFT) & 0xFF) * InFactor);
        const int lG = static_cast<int>(((InColor >> IM_COL32_G_SHIFT) & 0xFF) * InFactor);
        const int lB = static_cast<int>(((InColor >> IM_COL32_B_SHIFT) & 0xFF) * InFactor);
        const int lA =  (InColor >> IM_COL32_A_SHIFT) & 0xFF;

        return IM_COL32(lR, lG, lB, lA);
    }

    void Image(ImDrawList* InDrawList, const EditorImage& InImage, const ImVec2 InMin, const ImVec2 InMax)
    {
        InDrawList->AddImage(static_cast<ImTextureID>(InImage.Handle), InMin, InMax,
                             ImVec2(InImage.UV0.x, InImage.UV0.y),
                             ImVec2(InImage.UV1.x, InImage.UV1.y));
    }

    void HoverHighlight(ImDrawList* InDrawList, const ImVec2 InMin, const ImVec2 InMax)
    {
        InDrawList->AddRectFilled(InMin, InMax, ImGui::GetColorU32(ImGuiCol_HeaderHovered), CARD_ROUND);
    }

    void FolderGlyph(ImDrawList* InDrawList, const ImVec2 InMin, const ImVec2 InMax)
    {
        ImVec2 lA, lB;
        ImguiLayout::Inset(InMin, InMax, CARD_INSET, lA, lB);

        // The tab is a fraction of the BODY, not of the tile, so the folder keeps its shape at any size.
        const float lTabH = (lB.y - lA.y) * 0.26f;
        const float lTabW = (lB.x - lA.x) * 0.30f;

        InDrawList->AddRectFilled(lA, ImVec2(lA.x + lTabW, lA.y + lTabH + 3.f),
            Darken(FOLDER_COLOR, 0.84f), 3.f, ImDrawFlags_RoundCornersTop);
        InDrawList->AddRectFilled(ImVec2(lA.x, lA.y + lTabH), lB, FOLDER_COLOR, 3.f);
    }

    void IconBox(ImDrawList* InDrawList, const EditorImage& InImage, const char* InGlyph,
                 const ImVec2 InMin, const ImVec2 InMax, const float InInset)
    {
        ImVec2 lA, lB;
        ImguiLayout::Inset(InMin, InMax, InInset, lA, lB);

        if (InImage.IsValid())
        {
            Image(InDrawList, InImage, lA, lB);
            return;
        }

        InDrawList->AddRectFilled(lA, lB, CARD_FILL, CARD_ROUND);
        InDrawList->AddRect(lA, lB, CARD_BORDER, CARD_ROUND);

        const ImVec2 lTextSize = ImGui::CalcTextSize(InGlyph);
        InDrawList->AddText(ImVec2((lA.x + lB.x) * 0.5f - lTextSize.x * 0.5f,
                                   (lA.y + lB.y) * 0.5f - lTextSize.y * 0.5f),
            GLYPH_TEXT, InGlyph);
    }
}
