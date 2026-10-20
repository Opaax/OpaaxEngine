#include "Editor/Assets/Editors/SpriteSheetEditor.h"

#include <imgui.h>

#include "Application/Services/IPaths.h"
#include "Core/String/CString.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/Assets/Editors/SpriteSheetEditing.h"
#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "Renderer/Textures/SpriteSheet.h"
#include "Renderer/Textures/TextureResource.h"
#include "Resources/ResourceManager.h"

namespace Opaax::Editor
{
    namespace
    {
        /** Frame outlines: every frame, and the selected one. */
        constexpr ImU32 k_FrameColor    = IM_COL32(255, 255, 255, 110);
        constexpr ImU32 k_SelectedColor = IM_COL32(255, 190,  60, 255);

        /** A texel's place on screen, given where the image was drawn and its scale. */
        ImVec2 ToScreen(const ImVec2 InImageMin, const float InScale, const Vector2F& InTexel)
        {
            return ImVec2(InImageMin.x + InTexel.x * InScale, InImageMin.y + InTexel.y * InScale);
        }
    }

    SpriteSheetEditor::~SpriteSheetEditor() = default;

    void SpriteSheetEditor::DrawContents(AssetDocument& InDocument)
    {
        SpriteSheet* lSheet = Cast<SpriteSheet>(InDocument.GetAsset());
        if (lSheet == nullptr)
        {
            return;
        }

        // An undo can shorten the list under the selection.
        if (m_Selected >= static_cast<Int32>(lSheet->FrameCount()))
        {
            m_Selected      = -1;
            m_bDraggingRect = false;
        }

        DrawSettings(*lSheet);
        ImGui::Separator();

        const TextureResource* lTexture = ClaimTexture(*lSheet);

        DrawCanvas(*lSheet, lTexture);
        ImGui::Separator();

        DrawSliceTools(*lSheet, lTexture);
        ImGui::Separator();

        DrawSelectedFrame(*lSheet);
        ImGui::Separator();

        DrawFrameList(*lSheet);
    }

    void SpriteSheetEditor::DrawSettings(SpriteSheet& InSheet)
    {
        // The frames and the grid have their own tools below.
        TupleApply([this, &InSheet](const auto&... lProperties)
                   {
                       ([&]
                        {
                            if (!CString::Equals(lProperties.Name, "Frames") && !CString::Equals(lProperties.Name, "Grid"))
                            {
                                DrawProperty(m_Context.Widgets, lProperties, InSheet);
                            }
                        }(), ...);
                   },
                   SpriteSheet::GetProperties());
    }

    // =============================================================================
    // The canvas
    // =============================================================================
    void SpriteSheetEditor::DrawCanvas(SpriteSheet& InSheet, const TextureResource* InTexture)
    {
        if (InTexture == nullptr)
        {
            ImGui::TextDisabled(InSheet.Texture.IsEmpty() ? "This sheet names no texture: pick one above."
                                                          : "Could not load this sheet's texture.");
            return;
        }

        const ImVec2 lSize   = ImguiLayout::AspectFit(InTexture->Width, InTexture->Height, MAX_CANVAS_SIZE);
        const ImVec2 lImgMin = ImGui::GetCursorScreenPos();
        const ImVec2 lImgMax(lImgMin.x + lSize.x, lImgMin.y + lSize.y);

        // The canvas is one item: held while a frame is dragged, so the whole drag is one undo step.
        ImGui::InvisibleButton("##canvas", lSize);
        const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool bHeld    = ImGui::IsItemActive();

        ImDrawList* const lDraw = ImGui::GetWindowDrawList();

        if (InTexture->GetTexture() != nullptr)
        {
            const EditorImage lImage = m_Context.UIBackend.GetTextureImage(*InTexture->GetTexture());
            if (lImage.IsValid())
            {
                lDraw->AddImage(static_cast<ImTextureID>(lImage.Handle), lImgMin, lImgMax,
                                ImVec2(lImage.UV0.x, lImage.UV0.y), ImVec2(lImage.UV1.x, lImage.UV1.y));
            }
        }

        // One scale for both axes (AspectFit keeps the ratio).
        const float lScale = (InTexture->Width > 0u) ? lSize.x / static_cast<float>(InTexture->Width) : 1.f;

        // Drawn from the frames, never from the grid: frames can be moved by hand.
        for (Uint32 lIndex = 0; lIndex < InSheet.FrameCount(); ++lIndex)
        {
            const SpriteFrame& lFrame    = InSheet.Frames[lIndex];
            const bool         bSelected = static_cast<Int32>(lIndex) == m_Selected;

            const ImVec2 lMin = ToScreen(lImgMin, lScale, lFrame.Offset);
            const ImVec2 lMax = ToScreen(lImgMin, lScale, { lFrame.Offset.x + lFrame.Size.x, lFrame.Offset.y + lFrame.Size.y });

            lDraw->AddRect(lMin, lMax, bSelected ? k_SelectedColor : k_FrameColor, 0.f, 0, bSelected ? 2.f : 1.f);
        }

        if (lScale > 0.f)
        {
            const ImVec2   lMouse = ImGui::GetIO().MousePos;
            const Vector2F lTexel = { (lMouse.x - lImgMin.x) / lScale, (lMouse.y - lImgMin.y) / lScale };

            UpdateFrameDrag(InSheet, *InTexture, lTexel, lScale, bClicked, bHeld);
        }

        ImGui::TextDisabled("%u x %u", InTexture->Width, InTexture->Height);
    }

    void SpriteSheetEditor::UpdateFrameDrag(SpriteSheet& InSheet, const TextureResource& InTexture, const Vector2F InTexel,
                                            const float InScale, const bool bInClicked, const bool bInHeld)
    {
        if (!m_bDraggingRect)
        {
            if (!bInClicked)
            {
                return;
            }

            // The selected frame first: its edge or corner resizes it, its middle moves it.
            if (m_Selected >= 0)
            {
                const SpriteFrame&       lFrame = InSheet.Frames[static_cast<Uint32>(m_Selected)];
                const TEditorRect<float> lRect{ lFrame.Offset.x, lFrame.Offset.y, lFrame.Size.x, lFrame.Size.y };
                const ERectEdge          lEdge  = HitTestRect(lRect, InTexel.x, InTexel.y, GRAB_THICKNESS / InScale);

                const bool bInside = InTexel.x >= lRect.X && InTexel.x < lRect.X + lRect.Width
                                  && InTexel.y >= lRect.Y && InTexel.y < lRect.Y + lRect.Height;

                if (lEdge != ERectEdge::None || bInside)
                {
                    m_bDraggingRect = true;
                    m_DragEdge      = lEdge;
                    return;
                }
            }

            // Another frame: the click selects it, and holding moves it. Empty space keeps the selection.
            const Int32 lUnder = SheetEditing::FrameUnder(InSheet.Frames, InTexel);
            if (lUnder >= 0)
            {
                m_Selected      = lUnder;
                m_bDraggingRect = true;
                m_DragEdge      = ERectEdge::None;
            }
            return;
        }

        // The drag ends with the button.
        if (!bInHeld || m_Selected < 0)
        {
            m_bDraggingRect = false;
            m_DragEdge      = ERectEdge::None;
            return;
        }

        SpriteFrame& lFrame = InSheet.Frames[static_cast<Uint32>(m_Selected)];

        const ImVec2 lDelta = ImGui::GetIO().MouseDelta;
        const float  lDX    = lDelta.x / InScale;
        const float  lDY    = lDelta.y / InScale;

        const TEditorRect<float> lRect{ lFrame.Offset.x, lFrame.Offset.y, lFrame.Size.x, lFrame.Size.y };
        TEditorRect<float>       lNext = lRect;

        if (m_DragEdge == ERectEdge::None)
        {
            lNext.X += lDX;
            lNext.Y += lDY;
        }
        else
        {
            lNext = ResizeRect(lRect, m_DragEdge, lDX, lDY, 1.f, 1.f);
        }

        // A frame stays inside the texture.
        lNext = ClampRectInside(lNext, static_cast<float>(InTexture.Width), static_cast<float>(InTexture.Height));

        lFrame.Offset = { lNext.X, lNext.Y };
        lFrame.Size   = { lNext.Width, lNext.Height };
    }

    // =============================================================================
    // Tools and fields
    // =============================================================================
    void SpriteSheetEditor::DrawSliceTools(SpriteSheet& InSheet, const TextureResource* InTexture)
    {
        if (!ImGui::TreeNodeEx("Grid", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        ImGui::PushID("Grid");
        DrawProperties(m_Context.Widgets, InSheet.Grid);
        ImGui::PopID();

        // Slicing needs the texture's size, which only the loaded image gives.
        ImGui::BeginDisabled(InTexture == nullptr);
        if (ImGui::Button("Slice") && InTexture != nullptr)
        {
            if (SheetEditing::Slice(InSheet, InTexture->Width, InTexture->Height))
            {
                m_Selected = -1;   // the selected frame was replaced
                OPAAX_LOG(LogSpriteSheetEditor, Info, "Sliced into {} frame(s)", InSheet.FrameCount());
            }
            else
            {
                OPAAX_LOG(LogSpriteSheetEditor, Warn, "The grid cuts no frame from a {}x{} texture — the frames are unchanged",
                          InTexture->Width, InTexture->Height);
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("replaces every frame");

        // Next to Slice: sliced frames are unnamed, and clips name frames.
        if (ImGui::Button("Auto-Name"))
        {
            const Uint32 lNamed = SheetEditing::AutoNameFrames(InSheet.Frames);
            OPAAX_LOG(LogSpriteSheetEditor, Info, "Named {} of {} frame(s)", lNamed, InSheet.FrameCount());
        }

        ImGui::SameLine();
        ImGui::TextDisabled("names the unnamed, keeps the rest");

        ImGui::TreePop();
    }

    void SpriteSheetEditor::DrawSelectedFrame(SpriteSheet& InSheet)
    {
        if (m_Selected < 0)
        {
            ImGui::TextDisabled("Select a frame to edit it: click it in the image or in the list.");
            return;
        }

        const Uint32 lIndex = static_cast<Uint32>(m_Selected);

        ImGui::Text("Frame %u", lIndex);
        ImGui::SameLine();

        ImGui::BeginDisabled(lIndex == InSheet.DefaultFrame);
        if (ImGui::SmallButton("Set as Default"))
        {
            InSheet.DefaultFrame = lIndex;
        }
        ImGui::EndDisabled();

        ImGui::PushID(static_cast<int>(lIndex));
        DrawProperties(m_Context.Widgets, InSheet.Frames[lIndex]);
        ImGui::PopID();
    }

    void SpriteSheetEditor::DrawFrameList(const SpriteSheet& InSheet)
    {
        if (InSheet.FrameCount() == 0u)
        {
            ImGui::TextDisabled("No frames yet: set the grid, then Slice.");
            return;
        }

        constexpr ImGuiTableFlags lFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;

        if (!ImGui::BeginTable("##frames", 3, lFlags))
        {
            return;
        }

        ImGui::TableSetupColumn("#",    ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("000").x);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Rect", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("0000, 0000   0000 x 0000").x);
        ImGui::TableHeadersRow();

        for (Uint32 lIndex = 0; lIndex < InSheet.FrameCount(); ++lIndex)
        {
            const SpriteFrame& lFrame = InSheet.Frames[lIndex];

            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(lIndex));

            char lNumber[16];
            CString::Format(lNumber, "{}", lIndex);

            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(lNumber, static_cast<Int32>(lIndex) == m_Selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                m_Selected = static_cast<Int32>(lIndex);
            }

            // IsValid, not the name itself: an empty name prints as "None".
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s%s", lFrame.Name.IsValid() ? lFrame.Name.CStr() : "(unnamed)",
                        lIndex == InSheet.DefaultFrame ? "  (default)" : "");

            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%.0f, %.0f   %.0f x %.0f", lFrame.Offset.x, lFrame.Offset.y, lFrame.Size.x, lFrame.Size.y);

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    // =============================================================================
    // Claim
    // =============================================================================
    const TextureResource* SpriteSheetEditor::ClaimTexture(const SpriteSheet& InSheet)
    {
        if (InSheet.Texture.IsEmpty())
        {
            m_Claim       = {};
            m_ClaimedPath = OpaaxString();
            return nullptr;
        }

        if (InSheet.Texture.Path != m_ClaimedPath)
        {
            const OpaaxString lAbsolute = m_Context.Paths.AssetToAbsolute(InSheet.Texture.Path);

            m_Claim       = m_Context.Resources.Load<TextureResource>(lAbsolute.CStr());
            m_ClaimedPath = InSheet.Texture.Path;

            if (!m_Claim.IsValid())
            {
                OPAAX_LOG(LogSpriteSheetEditor, Warn, "Sheet image '{}' did not load", lAbsolute.CStr());
            }
        }

        // IsValid, not Get() != nullptr: a failed claim gives the magenta placeholder.
        return m_Claim.IsValid() ? m_Claim.Get() : nullptr;
    }
}
