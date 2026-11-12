#include "Editor/Assets/Editors/SpriteSheetEditor.h"

#include <imgui.h>

#include "Application/Services/IPaths.h"
#include "Core/Maths/Maths.h"
#include "Core/String/CString.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/Assets/Editors/SpriteSheetEditing.h"
#include "Editor/EditorContext.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "Renderer/Textures/SpriteSheet.h"
#include "Renderer/Textures/Texture2D.h"
#include "Resources/ResourceManager.h"

namespace Opaax::Editor
{
    namespace
    {
        /** Frame outlines: every frame, and the selected one. */
        constexpr ImU32 k_FrameColor    = IM_COL32(255, 255, 255, 110);
        constexpr ImU32 k_SelectedColor = IM_COL32(255, 190,  60, 255);

        /** The slice border's lines, and a line under the cursor or held. */
        constexpr ImU32 k_BorderColor    = IM_COL32( 80, 220, 255, 220);
        constexpr ImU32 k_BorderHotColor = IM_COL32(255, 255, 255, 255);

        /** A texel's place on screen, given where the image was drawn and its scale. */
        ImVec2 ToScreen(const ImVec2 InImageMin, const float InScale, const Vector2F& InTexel)
        {
            return ImVec2(InImageMin.x + InTexel.x * InScale, InImageMin.y + InTexel.y * InScale);
        }

        bool TakesLeft(const ERectEdge InLines)
        {
            return InLines == ERectEdge::Left || InLines == ERectEdge::TopLeft || InLines == ERectEdge::BottomLeft;
        }

        bool TakesRight(const ERectEdge InLines)
        {
            return InLines == ERectEdge::Right || InLines == ERectEdge::TopRight || InLines == ERectEdge::BottomRight;
        }

        bool TakesTop(const ERectEdge InLines)
        {
            return InLines == ERectEdge::Top || InLines == ERectEdge::TopLeft || InLines == ERectEdge::TopRight;
        }

        bool TakesBottom(const ERectEdge InLines)
        {
            return InLines == ERectEdge::Bottom || InLines == ERectEdge::BottomLeft || InLines == ERectEdge::BottomRight;
        }

        /**
         * A frame's four border lines, drawn over the frame placed at InFrameMin (screen) at InScale.
         * InHot lines are drawn brighter.
         */
        void DrawBorderLines(ImDrawList& InDraw, const SpriteFrame& InFrame, const ImVec2 InFrameMin, const float InScale,
                             const ERectEdge InHot, const float InThickness)
        {
            const float lLeft   = InFrameMin.x + InFrame.Border.Left * InScale;
            const float lRight  = InFrameMin.x + (InFrame.Size.x - InFrame.Border.Right) * InScale;
            const float lTop    = InFrameMin.y + InFrame.Border.Top * InScale;
            const float lBottom = InFrameMin.y + (InFrame.Size.y - InFrame.Border.Bottom) * InScale;
            const float lMinX   = InFrameMin.x;
            const float lMaxX   = InFrameMin.x + InFrame.Size.x * InScale;
            const float lMinY   = InFrameMin.y;
            const float lMaxY   = InFrameMin.y + InFrame.Size.y * InScale;

            const auto lColor = [](const bool bInHot) { return bInHot ? k_BorderHotColor : k_BorderColor; };

            InDraw.AddLine(ImVec2(lLeft, lMinY),  ImVec2(lLeft, lMaxY),   lColor(TakesLeft(InHot)),   InThickness);
            InDraw.AddLine(ImVec2(lRight, lMinY), ImVec2(lRight, lMaxY),  lColor(TakesRight(InHot)),  InThickness);
            InDraw.AddLine(ImVec2(lMinX, lTop),   ImVec2(lMaxX, lTop),    lColor(TakesTop(InHot)),    InThickness);
            InDraw.AddLine(ImVec2(lMinX, lBottom), ImVec2(lMaxX, lBottom), lColor(TakesBottom(InHot)), InThickness);
        }

        /** The mouse cursor that says which way the lines move. */
        ImGuiMouseCursor CursorFor(const ERectEdge InLines)
        {
            const bool bX = TakesLeft(InLines) || TakesRight(InLines);
            const bool bY = TakesTop(InLines) || TakesBottom(InLines);
            if (bX && bY) { return ImGuiMouseCursor_ResizeAll; }
            if (bX)       { return ImGuiMouseCursor_ResizeEW; }
            if (bY)       { return ImGuiMouseCursor_ResizeNS; }
            return ImGuiMouseCursor_Arrow;
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
            m_BorderLines   = ERectEdge::None;
        }

        DrawSettings(*lSheet);
        ImGui::Separator();

        const Texture2D* lTexture = ClaimTexture(*lSheet);

        DrawCanvas(*lSheet, lTexture);
        ImGui::Separator();

        DrawSliceTools(*lSheet, lTexture);
        ImGui::Separator();

        DrawSelectedFrame(*lSheet);
        DrawFrameView(*lSheet, lTexture);
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
    void SpriteSheetEditor::DrawCanvas(SpriteSheet& InSheet, const Texture2D* InTexture)
    {
        if (InTexture == nullptr)
        {
            ImGui::TextDisabled(InSheet.Texture.IsEmpty() ? "This sheet names no texture: pick one above."
                                                          : "Could not load this sheet's texture.");
            return;
        }

        const ImVec2 lSize   = ImguiLayout::AspectFit(InTexture->GetWidth(), InTexture->GetHeight(), MAX_CANVAS_SIZE);
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
        const float lScale = (InTexture->GetWidth() > 0u) ? lSize.x / static_cast<float>(InTexture->GetWidth()) : 1.f;

        // Drawn from the frames, never from the grid: frames can be moved by hand.
        for (Uint32 lIndex = 0; lIndex < InSheet.FrameCount(); ++lIndex)
        {
            const SpriteFrame& lFrame    = InSheet.Frames[lIndex];
            const bool         bSelected = static_cast<Int32>(lIndex) == m_Selected;

            const ImVec2 lMin = ToScreen(lImgMin, lScale, lFrame.Offset);
            const ImVec2 lMax = ToScreen(lImgMin, lScale, { lFrame.Offset.x + lFrame.Size.x, lFrame.Offset.y + lFrame.Size.y });

            lDraw->AddRect(lMin, lMax, bSelected ? k_SelectedColor : k_FrameColor, 0.f, 0, bSelected ? 2.f : 1.f);
        }

        // The selected frame's border: shown here, dragged in the frame view below.
        if (m_Selected >= 0)
        {
            const SpriteFrame& lFrame = InSheet.Frames[static_cast<Uint32>(m_Selected)];
            if (!lFrame.Border.IsZero())
            {
                DrawBorderLines(*lDraw, lFrame, ToScreen(lImgMin, lScale, lFrame.Offset), lScale, ERectEdge::None, 1.f);
            }
        }

        if (lScale > 0.f)
        {
            const ImVec2   lMouse = ImGui::GetIO().MousePos;
            const Vector2F lTexel = { (lMouse.x - lImgMin.x) / lScale, (lMouse.y - lImgMin.y) / lScale };

            UpdateFrameDrag(InSheet, *InTexture, lTexel, lScale, bClicked, bHeld);
        }

        ImGui::TextDisabled("%u x %u", InTexture->GetWidth(), InTexture->GetHeight());
    }

    void SpriteSheetEditor::UpdateFrameDrag(SpriteSheet& InSheet, const Texture2D& InTexture, const Vector2F InTexel,
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
        lNext = ClampRectInside(lNext, static_cast<float>(InTexture.GetWidth()), static_cast<float>(InTexture.GetHeight()));

        lFrame.Offset = { lNext.X, lNext.Y };
        lFrame.Size   = { lNext.Width, lNext.Height };
    }

    // =============================================================================
    // Tools and fields
    // =============================================================================
    void SpriteSheetEditor::DrawSliceTools(SpriteSheet& InSheet, const Texture2D* InTexture)
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
            if (SheetEditing::Slice(InSheet, InTexture->GetWidth(), InTexture->GetHeight()))
            {
                m_Selected = -1;   // the selected frame was replaced
                OPAAX_LOG(LogSpriteSheetEditor, Info, "Sliced into {} frame(s)", InSheet.FrameCount());
            }
            else
            {
                OPAAX_LOG(LogSpriteSheetEditor, Warn, "The grid cuts no frame from a {}x{} texture — the frames are unchanged",
                          InTexture->GetWidth(), InTexture->GetHeight());
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

    void SpriteSheetEditor::DrawFrameView(SpriteSheet& InSheet, const Texture2D* InTexture)
    {
        if (m_Selected < 0 || InTexture == nullptr || InTexture->GetTexture() == nullptr
            || InTexture->GetWidth() == 0u || InTexture->GetHeight() == 0u)
        {
            m_BorderLines = ERectEdge::None;
            return;
        }

        SpriteFrame& lFrame = InSheet.Frames[static_cast<Uint32>(m_Selected)];
        if (lFrame.Size.x <= 0.f || lFrame.Size.y <= 0.f)
        {
            return;
        }

        ImGui::SeparatorText("Slice border");

        // The frame alone, as wide as the window, no taller than MAX_FRAME_VIEW_HEIGHT.
        const float  lAvailable = Maths::Max(ImGui::GetContentRegionAvail().x, 64.f);
        const float  lScale     = Maths::Min(lAvailable / lFrame.Size.x, MAX_FRAME_VIEW_HEIGHT / lFrame.Size.y);
        const ImVec2 lSize(lFrame.Size.x * lScale, lFrame.Size.y * lScale);
        const ImVec2 lMin = ImGui::GetCursorScreenPos();
        const ImVec2 lMax(lMin.x + lSize.x, lMin.y + lSize.y);

        // Held for the whole drag: the window records it as one undo step.
        ImGui::InvisibleButton("##frameview", lSize);
        const bool bClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool bHeld    = ImGui::IsItemActive();
        const bool bHovered = ImGui::IsItemHovered();

        ImDrawList* const lDraw = ImGui::GetWindowDrawList();

        const EditorImage lImage = m_Context.UIBackend.GetTextureImage(*InTexture->GetTexture());
        if (lImage.IsValid())
        {
            // The frame's part of the image, between the backend's own corner UVs.
            const float  lTexW = static_cast<float>(InTexture->GetWidth());
            const float  lTexH = static_cast<float>(InTexture->GetHeight());
            const auto   lU    = [&lImage](const float InT) { return lImage.UV0.x + (lImage.UV1.x - lImage.UV0.x) * InT; };
            const auto   lV    = [&lImage](const float InT) { return lImage.UV0.y + (lImage.UV1.y - lImage.UV0.y) * InT; };
            const ImVec2 lUV0(lU(lFrame.Offset.x / lTexW), lV(lFrame.Offset.y / lTexH));
            const ImVec2 lUV1(lU((lFrame.Offset.x + lFrame.Size.x) / lTexW), lV((lFrame.Offset.y + lFrame.Size.y) / lTexH));

            lDraw->AddRectFilled(lMin, lMax, IM_COL32(30, 30, 34, 255));
            lDraw->AddImage(static_cast<ImTextureID>(lImage.Handle), lMin, lMax, lUV0, lUV1);
        }
        lDraw->AddRect(lMin, lMax, k_SelectedColor);

        // The cursor in texture pixels from the frame's top-left corner.
        const ImVec2   lMouse  = ImGui::GetIO().MousePos;
        const Vector2F lCursor = { (lMouse.x - lMin.x) / lScale, (lMouse.y - lMin.y) / lScale };
        const float    lGrab   = GRAB_THICKNESS / lScale;

        if (bClicked)
        {
            m_BorderLines = SheetEditing::BorderLinesUnder(lFrame.Border, lFrame.Size, lCursor, lGrab);
        }
        else if (!bHeld)
        {
            m_BorderLines = ERectEdge::None;
        }

        if (m_BorderLines != ERectEdge::None && bHeld)
        {
            lFrame.Border = SheetEditing::DragBorder(lFrame.Border, lFrame.Size, m_BorderLines, lCursor);
        }

        // The lines the cursor would take, or holds, are lit, and the cursor says which way they move.
        const ERectEdge lHot = (m_BorderLines != ERectEdge::None)
                                   ? m_BorderLines
                                   : (bHovered ? SheetEditing::BorderLinesUnder(lFrame.Border, lFrame.Size, lCursor, lGrab)
                                               : ERectEdge::None);
        if (lHot != ERectEdge::None)
        {
            ImGui::SetMouseCursor(CursorFor(lHot));
        }

        DrawBorderLines(*lDraw, lFrame, lMin, lScale, lHot, 2.f);

        ImGui::TextDisabled("Drag a line: L %.0f  R %.0f  T %.0f  B %.0f px", lFrame.Border.Left, lFrame.Border.Right,
                            lFrame.Border.Top, lFrame.Border.Bottom);
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
    const Texture2D* SpriteSheetEditor::ClaimTexture(const SpriteSheet& InSheet)
    {
        if (InSheet.Texture.IsEmpty())
        {
            m_Claim       = {};
            m_ClaimedPath = OpaaxString();
            return nullptr;
        }

        if (InSheet.Texture.Path != m_ClaimedPath)
        {
            m_Claim       = LoadAsset(m_Context.Resources, m_Context.Paths, InSheet.Texture);
            m_ClaimedPath = InSheet.Texture.Path;

            if (m_Claim.Get() == nullptr)
            {
                OPAAX_LOG(LogSpriteSheetEditor, Warn, "Sheet image '{}' did not load", InSheet.Texture.Path.CStr());
            }
        }

        return m_Claim.Get();
    }
}
