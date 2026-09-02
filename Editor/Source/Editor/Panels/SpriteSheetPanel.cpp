#include "Editor/Panels/SpriteSheetPanel.h"

#include <cstdio>   // snprintf — the frame list's row labels

#include "Editor/EditorContext.h"
#include "Editor/EditorSpriteSheetDocument.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/UI/IEditorUIBackend.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheetData.h"
#include "Engine/Subsystems/Resources/Types/TextureResource.h"
#include "Application/Services/IPaths.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        /** The frame outline, the selected one, and the frame a click landed in. */
        constexpr ImU32 k_FrameColor    = IM_COL32(255, 255, 255, 110);
        constexpr ImU32 k_SelectedColor = IM_COL32(255, 190,  60, 255);

        /** One frame's rect on screen, given where the image was drawn and how much it was scaled. */
        ImVec2 ToScreen(const ImVec2 InImageMin, const float InScale, const Vector2F& InTexel)
        {
            return ImVec2(InImageMin.x + InTexel.x * InScale, InImageMin.y + InTexel.y * InScale);
        }
    }

    SpriteSheetPanel::SpriteSheetPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    SpriteSheetPanel::~SpriteSheetPanel() = default;

    void SpriteSheetPanel::Shutdown()
    {
        m_Claim       = {};
        m_ClaimedPath = OpaaxString();
    }

    // =============================================================================
    // Draw
    // =============================================================================
    void SpriteSheetPanel::DrawContents()
    {
        if (!m_Context.SheetDocument.IsOpen())
        {
            ImGui::TextDisabled("No sprite sheet open.");
            ImGui::TextDisabled("Double-click a .opaaxsheet in the Resource Browser.");

            // Nothing open means nothing to keep resident — the same rule the preview panel follows.
            m_Claim       = {};
            m_ClaimedPath = OpaaxString();
            m_Selected    = -1;
            return;
        }

        const SpriteSheetData& lData = m_Context.SheetDocument.GetData();

        DrawHeader(lData);
        ImGui::Separator();

        DrawCanvas(lData, ClaimTexture(lData));
        ImGui::Separator();

        DrawFrameList(lData);
    }

    void SpriteSheetPanel::DrawHeader(const SpriteSheetData& InData)
    {
        // The dirty marker is derived by the document, so it cannot disagree with what a Save writes.
        const OpaaxString lName = m_Context.SheetDocument.FileName();

        ImGui::Text("%s%s", lName.CStr(), m_Context.SheetDocument.IsDirty() ? " *" : "");

        ImGui::TextDisabled("Texture : %s", InData.Texture.IsEmpty() ? "(none)" : InData.Texture.Path.CStr());
        ImGui::TextDisabled("Frames  : %u    Default : %u", InData.FrameCount(), InData.DefaultFrame);
    }

    void SpriteSheetPanel::DrawCanvas(const SpriteSheetData& InData, const TextureResource* InTexture)
    {
        if (InTexture == nullptr)
        {
            ImGui::TextDisabled(InData.Texture.IsEmpty() ? "This sheet names no texture."
                                                         : "Could not load this sheet's texture.");
            return;
        }

        const ImVec2 lSize    = ImguiLayout::AspectFit(InTexture->Width, InTexture->Height, MAX_CANVAS_SIZE);
        const ImVec2 lImgMin  = ImGui::GetCursorScreenPos();

        ImguiWidgets::Image(InTexture->GetTexture() != nullptr
                                ? m_Context.UIBackend.GetTextureImage(*InTexture->GetTexture())
                                : EditorImage{},
                            lSize);

        // ONE scale for both axes — AspectFit preserves the ratio, so a per-axis scale would be the
        // same number twice and would silently stop being true the day it is not.
        const float lScale = (InTexture->Width > 0u) ? lSize.x / static_cast<float>(InTexture->Width) : 1.f;

        const bool   bHovered = ImGui::IsItemHovered();
        const ImVec2 lMouse   = ImGui::GetIO().MousePos;

        ImDrawList* lDraw = ImGui::GetWindowDrawList();

        for (Uint32 i = 0; i < InData.FrameCount(); ++i)
        {
            const SpriteFrame& lFrame = InData.Frames[i];

            // The rect is drawn in TEXTURE pixels scaled to the image, never from the grid: the
            // frames are the truth, and a grid-derived overlay would stop matching the moment one
            // rect is moved by hand.
            const ImVec2 lMin = ToScreen(lImgMin, lScale, lFrame.Offset);
            const ImVec2 lMax = ToScreen(lImgMin, lScale, { lFrame.Offset.x + lFrame.Size.x,
                                                            lFrame.Offset.y + lFrame.Size.y });

            const bool bSelected = (static_cast<Int32>(i) == m_Selected);

            lDraw->AddRect(lMin, lMax, bSelected ? k_SelectedColor : k_FrameColor, 0.f, 0, bSelected ? 2.f : 1.f);

            // Selection by click. Front-to-back would matter for overlapping frames; the first hit
            // wins here, which is the submission order and therefore the frame INDEX order.
            if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                && lMouse.x >= lMin.x && lMouse.x <= lMax.x && lMouse.y >= lMin.y && lMouse.y <= lMax.y
                && !bSelected)
            {
                m_Selected = static_cast<Int32>(i);
            }
        }

        ImGui::TextDisabled("%u x %u", InTexture->Width, InTexture->Height);
    }

    void SpriteSheetPanel::DrawFrameList(const SpriteSheetData& InData)
    {
        if (InData.FrameCount() == 0)
        {
            ImGui::TextDisabled("No frames. Slicing arrives with the grid controls.");
            return;
        }

        ImGui::BeginChild("Frames", ImVec2(0.f, 0.f), ImGuiChildFlags_Borders);

        for (Uint32 i = 0; i < InData.FrameCount(); ++i)
        {
            const SpriteFrame& lFrame = InData.Frames[i];

            // The NAME when there is one, the index when there is not — and the check is IsValid,
            // never ToString, because an invalid id answers "None" and would print it as a name.
            char lLabel[160];
            if (lFrame.Name.IsValid())
            {
                std::snprintf(lLabel, sizeof(lLabel), "%u  %s", i, lFrame.Name.CStr());
            }
            else
            {
                std::snprintf(lLabel, sizeof(lLabel), "%u", i);
            }

            ImGui::PushID(static_cast<int>(i));

            if (ImGui::Selectable(lLabel, static_cast<Int32>(i) == m_Selected))
            {
                m_Selected = static_cast<Int32>(i);
            }

            ImGui::SameLine(120.f);
            ImGui::TextDisabled("%.0f, %.0f   %.0f x %.0f",
                                lFrame.Offset.x, lFrame.Offset.y, lFrame.Size.x, lFrame.Size.y);

            if (i == InData.DefaultFrame)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(default)");
            }

            ImGui::PopID();
        }

        ImGui::EndChild();
    }

    // =============================================================================
    // Claim
    // =============================================================================
    const TextureResource* SpriteSheetPanel::ClaimTexture(const SpriteSheetData& InData)
    {
        if (InData.Texture.IsEmpty())
        {
            m_Claim       = {};
            m_ClaimedPath = OpaaxString();
            return nullptr;
        }

        if (InData.Texture.Path != m_ClaimedPath)
        {
            const OpaaxString lAbsolute = m_Context.Paths.AssetToAbsolute(InData.Texture.Path);

            m_Claim       = m_Context.Resources.Load<TextureResource>(lAbsolute.CStr());
            m_ClaimedPath = InData.Texture.Path;

            // Once per sheet opened, not per frame — the discrete event, and the only signal that
            // the double-click reached this panel at all.
            if (m_Claim.IsValid())
            {
                OPAAX_LOG(LogSpriteSheetPanel, Info, "Sheet image '{}' loaded", lAbsolute.CStr());
            }
            else
            {
                OPAAX_LOG(LogSpriteSheetPanel, Warn, "Sheet image '{}' did not load", lAbsolute.CStr());
            }
        }

        // IsValid, not Get() != nullptr: a failed claim answers the magenta PLACEHOLDER, and drawing
        // that as if it were the sheet would hide the failure behind a picture (I16).
        return m_Claim.IsValid() ? m_Claim.Get() : nullptr;
    }
}
