#include "Editor/Panels/SpriteSheetPanel.h"

#include <cstdio>   // snprintf

#include "Editor/EditorContext.h"
#include "Editor/EditorSpriteSheetDocument.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/SheetOperations.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/UI/IEditorUIBackend.h"

#include "Resources/ResourceManager.h"
#include "Renderer/Textures/SpriteSheetData.h"
#include "Renderer/Textures/TextureResource.h"
#include "Application/Services/IPaths.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        /** Frame outline colours: normal, selected, and the frame under a click. */
        constexpr ImU32 k_FrameColor    = IM_COL32(255, 255, 255, 110);
        constexpr ImU32 k_SelectedColor = IM_COL32(255, 190,  60, 255);

        /** One frame's rect on screen, given where the image was drawn and its scale. */
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

            // Nothing open: nothing to keep loaded.
            m_Claim       = {};
            m_ClaimedPath = OpaaxString();
            m_Selected    = -1;
            return;
        }

        SpriteSheetData&       lData    = m_Context.SheetDocument.GetMutableData();
        const TextureResource* lTexture = ClaimTexture(lData);

        DrawHeader(lData);
        ImGui::Separator();

        DrawCanvas(lData, lTexture);
        ImGui::Separator();

        DrawSliceTools(lTexture);
        ImGui::Separator();

        DrawSelectedFrame(lData);
        ImGui::Separator();

        DrawFrameList(lData);
    }

    void SpriteSheetPanel::DrawHeader(const SpriteSheetData& InData)
    {
        // The dirty marker comes from the document, so it matches what Save writes.
        const OpaaxString lName  = m_Context.SheetDocument.FileName();
        const bool        bDirty = m_Context.SheetDocument.IsDirty();

        ImGui::Text("%s%s", lName.CStr(), bDirty ? " *" : "");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        // The button and Ctrl+S dispatch the same command tag.
        ImGui::BeginDisabled(!bDirty);
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_SHEET, m_Context);
        }
        ImGui::EndDisabled();

        ImGui::TextDisabled("Texture : %s", InData.Texture.IsEmpty() ? "(none)" : InData.Texture.Path.CStr());
        ImGui::TextDisabled("Frames  : %u    Default : %u", InData.FrameCount(), InData.DefaultFrame);
    }

    void SpriteSheetPanel::DrawSliceTools(const TextureResource* InTexture)
    {
        if (!ImGui::TreeNodeEx("Grid", ImGuiTreeNodeFlags_DefaultOpen)) { return; }

        // The grid's own fields, from its property list.
        DrawProperties(m_Context.Widgets, m_Context.SheetDocument.GetMutableData().Grid);

        // Slicing needs the texture size, which only the loaded image gives.
        ImGui::BeginDisabled(InTexture == nullptr);

        if (ImGui::Button("Slice") && InTexture != nullptr)
        {
            if (SheetOps::Slice(m_Context, InTexture->Width, InTexture->Height))
            {
                m_Selected = -1;   // the selected frame was removed
            }
        }

        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("replaces every frame");

        // Next to Slice: sliced frames are unnamed, and animations reference frames by name.
        if (ImGui::Button("Auto-Name"))
        {
            SheetOps::AutoNameFrames(m_Context);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("names the unnamed, keeps the rest");

        ImGui::TreePop();
    }

    void SpriteSheetPanel::DrawSelectedFrame(SpriteSheetData& InData)
    {
        if (m_Selected < 0 || static_cast<Uint32>(m_Selected) >= InData.FrameCount())
        {
            ImGui::TextDisabled("Select a frame to edit it.");
            return;
        }

        const Uint32 lIndex = static_cast<Uint32>(m_Selected);

        ImGui::Text("Frame %u", lIndex);
        ImGui::SameLine();

        ImGui::BeginDisabled(lIndex == InData.DefaultFrame);
        if (ImGui::SmallButton("Set as default")) { SheetOps::SetDefaultFrame(m_Context, lIndex); }
        ImGui::EndDisabled();

        ImGui::PushID(static_cast<int>(lIndex));
        DrawProperties(m_Context.Widgets, InData.Frames[lIndex]);
        ImGui::PopID();

        // The fields' undo edges, like the Inspector. A canvas drag is not an ImGui item, so the two
        // never overlap.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasItemActive && !m_bDraggingRect)
        {
            m_Gesture.Begin(m_Context, lIndex);
            m_bGestureOpen = true;
        }
        else if (!lItemActive && m_bWasItemActive && m_bGestureOpen && !m_bDraggingRect)
        {
            if (m_Gesture.End(m_Context)) { m_Context.Undo.Record(Move(m_Gesture)); }

            m_Gesture      = SheetFrameEdit{};
            m_bGestureOpen = false;
        }

        m_bWasItemActive = lItemActive;
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

        // One scale for both axes (AspectFit keeps the ratio).
        const float lScale = (InTexture->Width > 0u) ? lSize.x / static_cast<float>(InTexture->Width) : 1.f;

        const bool   bHovered = ImGui::IsItemHovered();
        const ImVec2 lMouse   = ImGui::GetIO().MousePos;

        ImDrawList* lDraw = ImGui::GetWindowDrawList();

        for (Uint32 i = 0; i < InData.FrameCount(); ++i)
        {
            const SpriteFrame& lFrame = InData.Frames[i];

            // Drawn from the frames (texture pixels scaled to the image), never from the grid: frames can
            // be moved by hand.
            const ImVec2 lMin = ToScreen(lImgMin, lScale, lFrame.Offset);
            const ImVec2 lMax = ToScreen(lImgMin, lScale, { lFrame.Offset.x + lFrame.Size.x,
                                                            lFrame.Offset.y + lFrame.Size.y });

            const bool bSelected = (static_cast<Int32>(i) == m_Selected);

            lDraw->AddRect(lMin, lMax, bSelected ? k_SelectedColor : k_FrameColor, 0.f, 0, bSelected ? 2.f : 1.f);

            // Click selection: the first hit wins (frame index order).
            if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                && lMouse.x >= lMin.x && lMouse.x <= lMax.x && lMouse.y >= lMin.y && lMouse.y <= lMax.y
                && !bSelected)
            {
                m_Selected = static_cast<Int32>(i);
            }
        }

        UpdateFrameDrag(m_Context.SheetDocument.GetMutableData(), *InTexture,
                        { lImgMin.x, lImgMin.y }, lScale);

        ImGui::TextDisabled("%u x %u", InTexture->Width, InTexture->Height);
    }

    void SpriteSheetPanel::UpdateFrameDrag(SpriteSheetData& InData, const TextureResource& InTexture,
                                           const Vector2F InImageMin, const float InScale)
    {
        if (m_Selected < 0 || static_cast<Uint32>(m_Selected) >= InData.FrameCount() || InScale <= 0.f)
        {
            return;
        }

        SpriteFrame& lFrame = InData.Frames[static_cast<Uint32>(m_Selected)];

        // Everything below is in texture pixels (what frames are stored in).
        const ImVec2 lMouse = ImGui::GetIO().MousePos;
        const float  lTexX  = (lMouse.x - InImageMin.x) / InScale;
        const float  lTexY  = (lMouse.y - InImageMin.y) / InScale;

        const TEditorRect<float> lRect{ lFrame.Offset.x, lFrame.Offset.y, lFrame.Size.x, lFrame.Size.y };
        const float              lGrab = GRAB_THICKNESS / InScale;

        if (!m_bDraggingRect)
        {
            if (!ImGui::IsItemHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) { return; }

            const ERectEdge lEdge = HitTestRect(lRect, lTexX, lTexY, lGrab);

            const bool lInside = lTexX >= lRect.X && lTexX < lRect.X + lRect.Width
                              && lTexY >= lRect.Y && lTexY < lRect.Y + lRect.Height;

            // An edge resizes, the middle moves: HitTestRect returns None for both inside and outside,
            // and only inside is a grab.
            if (lEdge == ERectEdge::None && !lInside) { return; }

            m_bDraggingRect = true;
            m_DragEdge      = lEdge;

            m_Gesture.Begin(m_Context, static_cast<Uint32>(m_Selected));
            m_bGestureOpen  = true;
            return;
        }

        const ImVec2 lDelta = ImGui::GetIO().MouseDelta;
        const float  lDX    = lDelta.x / InScale;
        const float  lDY    = lDelta.y / InScale;

        TEditorRect<float> lNext = lRect;

        if (m_DragEdge == ERectEdge::None)
        {
            lNext.X += lDX;
            lNext.Y += lDY;
        }
        else
        {
            lNext = ResizeRect(lRect, m_DragEdge, lDX, lDY, 1.f, 1.f);
        }

        // A frame must stay inside the texture.
        lNext = ClampRectInside(lNext, static_cast<float>(InTexture.Width), static_cast<float>(InTexture.Height));

        lFrame.Offset = { lNext.X, lNext.Y };
        lFrame.Size   = { lNext.Width, lNext.Height };

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            m_bDraggingRect = false;
            m_DragEdge      = ERectEdge::None;

            // A drag back to where it started is not a step. The gesture is dropped either way.
            if (m_bGestureOpen && m_Gesture.End(m_Context)) { m_Context.Undo.Record(Move(m_Gesture)); }

            m_Gesture      = SheetFrameEdit{};
            m_bGestureOpen = false;
        }
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

            // The name when there is one, else the index (IsValid, not ToString, which gives "None").
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

            // Once per sheet opened, not per frame.
            if (m_Claim.IsValid())
            {
                OPAAX_LOG(LogSpriteSheetPanel, Info, "Sheet image '{}' loaded", lAbsolute.CStr());
            }
            else
            {
                OPAAX_LOG(LogSpriteSheetPanel, Warn, "Sheet image '{}' did not load", lAbsolute.CStr());
            }
        }

        // IsValid, not Get() != nullptr: a failed claim returns the magenta placeholder.
        return m_Claim.IsValid() ? m_Claim.Get() : nullptr;
    }
}
