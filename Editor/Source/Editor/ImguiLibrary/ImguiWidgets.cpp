#include "Editor/ImguiLibrary/ImguiWidgets.h"

#include <cstring>

namespace Opaax::Editor::ImguiWidgets
{
    namespace
    {
        constexpr ImVec4 ACTIVE_TINT = ImVec4(0.26f, 0.59f, 0.98f, 1.f);
        constexpr char   ELLIPSIS[]  = "..";
    }

    void Image(const EditorImage& InImage, const ImVec2 InSize)
    {
        // A Dummy rather than a null handle: drawing a null texture is a backend validation error,
        // and reserving the space keeps the panel from jumping when the upload lands.
        if (!InImage.IsValid())
        {
            ImGui::Dummy(InSize);
            return;
        }

        ImGui::Image(static_cast<ImTextureID>(InImage.Handle), InSize,
                     ImVec2(InImage.UV0.x, InImage.UV0.y),
                     ImVec2(InImage.UV1.x, InImage.UV1.y));
    }

    bool ToggleButton(const char* InLabel, const bool bActive)
    {
        if (bActive) { ImGui::PushStyleColor(ImGuiCol_Button, ACTIVE_TINT); }
        const bool bPressed = ImGui::SmallButton(InLabel);
        if (bActive) { ImGui::PopStyleColor(); }

        return bPressed;
    }

    void TextEllipsized(const char* InText, const float InWidth, const bool bInCentered)
    {
        const ImVec2 lFull = ImGui::CalcTextSize(InText);
        if (lFull.x <= InWidth)
        {
            if (bInCentered)
            {
                const float lOffset = (InWidth - lFull.x) * 0.5f;
                if (lOffset > 0.f) { ImGui::SetCursorPosX(ImGui::GetCursorPosX() + lOffset); }
            }

            ImGui::TextUnformatted(InText);
            return;
        }

        char         lBuffer[160];
        const float  lDotsWidth = ImGui::CalcTextSize(ELLIPSIS).x;
        const size_t lMax       = strlen(InText);
        size_t       lLength    = 0;
        float        lWidth     = 0.f;

        for (; lLength < lMax && lLength < sizeof(lBuffer) - 3; ++lLength)
        {
            const char lChar[2] = { InText[lLength], '\0' };
            lWidth += ImGui::CalcTextSize(lChar).x;
            if (lWidth + lDotsWidth > InWidth) { break; }
        }

        memcpy(lBuffer, InText, lLength);
        lBuffer[lLength]     = ELLIPSIS[0];
        lBuffer[lLength + 1] = ELLIPSIS[1];
        lBuffer[lLength + 2] = '\0';
        ImGui::TextUnformatted(lBuffer);
    }
}
