#include "Editor/Panels/UICanvasPanel.h"

#include <algorithm>

#include <imgui.h>

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Operation/UICanvasOperations.h"
#include "Editor/Properties/PropertyDrawers.h"   // the specializations DrawProperties folds over
#include "Editor/UI/IEditorUIBackend.h"

#include "Engine/Registries/EngineRegistries.h"
#include "RHI/Framebuffer.h"
#include "Renderer/RenderTarget.hpp"             // OffscreenRenderTarget
#include "UI/UICanvasFile.h"
#include "UI/UIWidget.h"
#include "UI/UIWidgetRegistry.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UIText.h"

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        constexpr const char* DRAG_PAYLOAD = "OPAAX_UI_WIDGET";

        /** The registry every route here asks for the buildable type names. */
        const UIWidgetRegistry& Registry()
        {
            return OpaaxApplication::GetAppService<IEngine>().GetRegistries().UIWidgets();
        }

    }

    UICanvasPanel::UICanvasPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    UICanvasPanel::~UICanvasPanel() = default;

    // =============================================================================
    // Frame
    // =============================================================================

    void UICanvasPanel::OnPreRender()
    {
        if (!m_Context.UICanvasDocument.IsOpen()) { return; }

        if (m_Framebuffer == nullptr)
        {
            m_Framebuffer = m_Context.Engine.CreateFramebuffer(FramebufferSpec{ m_Size.x, m_Size.y });
            if (m_Framebuffer == nullptr) { return; }

            m_RenderTarget = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());
        }

        // The deferred resize every offscreen panel uses: measured during the draw, applied before
        // the next frame's pass.
        if (m_PendingSize.x > 0 && m_PendingSize.y > 0
            && (m_PendingSize.x != m_Size.x || m_PendingSize.y != m_Size.y))
        {
            m_Size = m_PendingSize;
            m_Framebuffer->Resize(m_Size.x, m_Size.y);
        }

        // NAMING ITS OWN TARGET — the whole reason U4 gave a canvas submission one: the game's
        // canvases stay out of this framebuffer and this document stays out of the world (UI14).
        m_Context.Engine.SubmitUICanvas(m_Context.UICanvasDocument.GetCanvas(), m_RenderTarget.get());
    }

    void UICanvasPanel::DrawContents()
    {
        if (!m_Context.UICanvasDocument.IsOpen())
        {
            ImGui::TextDisabled("No UI canvas open.");
            ImGui::TextDisabled("Double-click a .opaaxui in the Resource Browser, or File > New UI...");
            return;
        }

        DrawHeader();
        ImGui::Separator();

        ImGui::BeginChild("UITree", ImVec2(m_TreeWidth, 0.f), ImGuiChildFlags_ResizeX);
        DrawTree();
        ImGui::Separator();
        DrawInspector();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("UIPreview", ImVec2(0.f, 0.f));
        DrawPreview();
        ImGui::EndChild();
    }

    void UICanvasPanel::DrawHeader()
    {
        const OpaaxString lName  = m_Context.UICanvasDocument.FileName();
        const bool        bDirty = m_Context.UICanvasDocument.IsDirty();

        ImGui::Text("%s%s", lName.CStr(), bDirty ? " *" : "");

        ImGui::SameLine();
        ImGui::TextDisabled("(%llu widgets)",
                            static_cast<unsigned long long>(
                                UICanvasFile::CountWidgets(m_Context.UICanvasDocument.GetCanvas().Root())));

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        ImGui::BeginDisabled(!bDirty);
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_UI, m_Context);
        }
        ImGui::EndDisabled();
    }

    // =============================================================================
    // The tree
    // =============================================================================

    void UICanvasPanel::DrawTree()
    {
        DrawAddMenu();

        ImGui::SameLine();

        const UIWidgetPath& lSelected = m_Context.UICanvasDocument.SelectedPath();

        ImGui::BeginDisabled(lSelected.empty());
        if (ImGui::Button("Delete"))
        {
            UICanvasOps::RemoveWidget(m_Context, lSelected);
        }
        ImGui::EndDisabled();

        ImGui::Separator();

        if (ImGui::BeginChild("UITreeNodes", ImVec2(0.f, ImGui::GetContentRegionAvail().y * 0.5f)))
        {
            UIWidget& lRoot = m_Context.UICanvasDocument.GetCanvas().Root();

            // The ROOT is a drop target too — "move this back to the top level".
            ImGui::TextDisabled("Canvas");
            if (ImGui::BeginDragDropTarget())
            {
                if (ImGui::AcceptDragDropPayload(DRAG_PAYLOAD) != nullptr)
                {
                    UICanvasOps::ReparentWidget(m_Context, m_DragPath, UIWidgetPath{});
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::Indent();
            for (Uint32 lIndex = 0; lIndex < lRoot.GetChildren().size(); ++lIndex)
            {
                DrawNode(*lRoot.GetChildren()[lIndex], UIWidgetPath{ lIndex });
            }
            ImGui::Unindent();
        }
        ImGui::EndChild();
    }

    void UICanvasPanel::DrawNode(UIWidget& InWidget, const UIWidgetPath& InPath)
    {
        ImGui::PushID(static_cast<int>(reinterpret_cast<std::uintptr_t>(&InWidget)));

        const bool lLeaf     = InWidget.GetChildren().empty();
        const bool lSelected = m_Context.UICanvasDocument.SelectedPath() == InPath;

        ImGuiTreeNodeFlags lFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen
                                  | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (lLeaf)     { lFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; }
        if (lSelected) { lFlags |= ImGuiTreeNodeFlags_Selected; }

        const OpaaxString lLabel = InWidget.Name.IsEmpty() ? InWidget.GetTypeName().ToString() : InWidget.Name;

        const bool lOpen = ImGui::TreeNodeEx(lLabel.CStr(), lFlags, "%s  (%s)",
                                             lLabel.CStr(), InWidget.GetTypeName().CStr());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            m_Context.UICanvasDocument.Select(InPath);
        }

        // Drag to reparent. The PATH is banked at the source, the Hierarchy's rule — the payload
        // itself carries nothing, because a path outlives the frame and a pointer might not.
        if (ImGui::BeginDragDropSource())
        {
            m_DragPath = InPath;
            ImGui::SetDragDropPayload(DRAG_PAYLOAD, nullptr, 0);
            ImGui::TextUnformatted(lLabel.CStr());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (ImGui::AcceptDragDropPayload(DRAG_PAYLOAD) != nullptr)
            {
                UICanvasOps::ReparentWidget(m_Context, m_DragPath, InPath);
            }
            ImGui::EndDragDropTarget();
        }

        if (lOpen && !lLeaf)
        {
            for (Uint32 lIndex = 0; lIndex < InWidget.GetChildren().size(); ++lIndex)
            {
                UIWidgetPath lChildPath = InPath;
                lChildPath.emplace_back(lIndex);

                DrawNode(*InWidget.GetChildren()[lIndex], lChildPath);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void UICanvasPanel::DrawAddMenu()
    {
        if (!ImGui::Button("Add")) { /* the popup is opened below */ }

        if (ImGui::IsItemClicked()) { ImGui::OpenPopup("UIAddWidget"); }

        if (ImGui::BeginPopup("UIAddWidget"))
        {
            // Every REGISTERED type — a game module's own widget appears here with no edit (UI12).
            for (const OpaaxStringID& lName : Registry().GetNames())
            {
                if (ImGui::Selectable(lName.CStr()))
                {
                    // Under the selection when there is one, else at the top level.
                    const UIWidgetPath lPath = UICanvasOps::AddWidget(m_Context, lName,
                                                                      m_Context.UICanvasDocument.SelectedPath());
                    if (!lPath.empty())
                    {
                        m_Context.UICanvasDocument.Select(lPath);
                    }
                }
            }

            ImGui::EndPopup();
        }
    }

    // =============================================================================
    // The inspector
    // =============================================================================

    void UICanvasPanel::DrawInspector()
    {
        UIWidget* const lWidget = m_Context.UICanvasDocument.SelectedWidget();

        if (lWidget == nullptr || m_Context.UICanvasDocument.SelectedPath().empty())
        {
            ImGui::TextDisabled("Select a widget.");
            return;
        }

        ImGui::TextDisabled("%s", lWidget->GetTypeName().CStr());

        // The RESOLVED rect, read-only: what the anchors actually produced, so an odd one reads
        // as numbers here rather than as "it went somewhere" in the preview.
        const Bounds2D& lBounds = lWidget->GetBounds();
        ImGui::TextDisabled("resolved  min (%.0f, %.0f)  size %.0f x %.0f",
                            lBounds.Min().x, lBounds.Min().y, lBounds.Size().x, lBounds.Size().y);

        // TWO HALVES, and the ladder that used to be here drew only one of them (**UI18**):
        //   the BASE fields every widget has (Name, Rect, visibility) — which a leaf type's own
        //   property list does not repeat, so a UIText had no editable Rect at all;
        //   then the TYPE's own, through the drawer registry, so a widget type nobody added to a
        //   hand-written list cannot silently lose its fields.
        DrawProperties(m_Context.Widgets, static_cast<UIWidget&>(*lWidget));

        ImGui::Separator();

        if (!m_Context.Extensions.UIWidgetDrawers().DrawFirst(*lWidget, m_Context.Widgets, m_Context))
        {
            // A registered widget type with no drawer: say so where the author is looking, rather
            // than showing a short list that looks complete.
            ImGui::TextDisabled("No drawer registered for %s.", lWidget->GetTypeName().CStr());
        }

        // THE GESTURE: a drag is many frames, and a step per frame would flood the history. Open on
        // the first active frame, close when nothing is active any more — FontFamilyPanel's shape,
        // with the whole TREE as the before-image because that is what a step carries (UI15).
        const bool lActive = ImGui::IsAnyItemActive();

        if (lActive && !m_bWasItemActive && !m_bGestureOpen)
        {
            m_GestureBefore = UICanvasOps::Snapshot(m_Context);
            m_bGestureOpen  = true;
        }
        else if (!lActive && m_bWasItemActive && m_bGestureOpen)
        {
            UICanvasOps::CommitEdit(m_Context, m_GestureBefore, "Edit Widget");
            m_bGestureOpen = false;
        }

        m_bWasItemActive = lActive;

        // The widget's own state may have changed under the drawer; it cannot know to re-layout.
        lWidget->InvalidateLayout();
    }

    // =============================================================================
    // The preview
    // =============================================================================

    void UICanvasPanel::DrawPreview()
    {
        const ImVec2 lAvail = ImGui::GetContentRegionAvail();

        if (lAvail.x < 1.f || lAvail.y < 1.f) { return; }

        // The framebuffer is sized to the REGION, so the image draws 1:1 and never rescales.
        m_PendingSize = { static_cast<Uint32>(lAvail.x), static_cast<Uint32>(lAvail.y) };

        const EditorImage lImage = m_Framebuffer != nullptr
                                       ? m_Context.UIBackend.GetViewportImage(*m_Framebuffer)
                                       : EditorImage{};

        // Drawn at the FRAMEBUFFER's size rather than the region's: they agree from the frame after
        // a resize, and using the region on the frame they disagree is exactly a stretch.
        const Vector2F lSizePx = PreviewPx();
        ImguiWidgets::Image(lImage, ImVec2(lSizePx.x, lSizePx.y));

        // The image is the item, so its rect and hover are readable right here — the viewport's
        // rule for every gesture (the origin is only knowable at the item).
        const ImVec2   lItemMin = ImGui::GetItemRectMin();
        const Vector2F lOrigin{ lItemMin.x, lItemMin.y };

        MeasurePreviewGesture(ImGui::IsItemHovered(), lOrigin);
        DrawPreviewOverlay(lOrigin);
    }

    void UICanvasPanel::MeasurePreviewGesture(const bool bInHovered, const Vector2F& InOrigin)
    {
        EditorUICanvasDocument& lDoc    = m_Context.UICanvasDocument;
        const UICanvas&         lCanvas = lDoc.GetCanvas();
        const ImGuiIO&          lIO     = ImGui::GetIO();

        const Vector2F lLocalPx{ lIO.MousePos.x - InOrigin.x, lIO.MousePos.y - InOrigin.y };

        m_HoverPath = bInHovered ? lDoc.PickAt(lCanvas.ScreenToCanvas(lLocalPx)) : UIWidgetPath{};

        if (bInHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            lDoc.Select(m_HoverPath);   // empty is bare canvas, and that clears

            m_bPreviewDrag     = true;
            m_bPreviewMoved    = false;
            m_PreviewAppliedPx = { 0.f, 0.f };
            m_PreviewBefore    = UICanvasOps::Snapshot(m_Context);
        }

        if (!m_bPreviewDrag)
        {
            // Arrow keys while the pointer is over the preview: 1 unit, Shift for 10.
            if (bInHovered && !lDoc.SelectedPath().empty())
            {
                const float lStep = lIO.KeyShift ? 10.f : 1.f;
                Vector2F    lDelta{ 0.f, 0.f };

                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  { lDelta.x -= lStep; }
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) { lDelta.x += lStep; }
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))  { lDelta.y -= lStep; }
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))    { lDelta.y += lStep; }

                if (lDelta.x != 0.f || lDelta.y != 0.f)
                {
                    NudgeSelected(lDelta, "Nudge Widget");
                }
            }
            return;
        }

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // The TOTAL since the press, minus what is already applied: exact across frames, and
            // the threshold ImGui applies before calling it a drag keeps a plain click from nudging.
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                const ImVec2   lTotal = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.f);
                const Vector2F lStepPx{ lTotal.x - m_PreviewAppliedPx.x, lTotal.y - m_PreviewAppliedPx.y };
                m_PreviewAppliedPx = { lTotal.x, lTotal.y };

                UIWidget* const lWidget = lDoc.SelectedWidget();
                if (lWidget != nullptr && !lDoc.SelectedPath().empty() && (lStepPx.x != 0.f || lStepPx.y != 0.f))
                {
                    // Pixels → canvas units, Y flipped: the canvas is Y-up and the image is not.
                    const float lUnits = lCanvas.UnitsPerPixel();
                    lWidget->Rect.AnchoredPosition += Vector2F{ lStepPx.x * lUnits, -lStepPx.y * lUnits };
                    lWidget->InvalidateLayout();
                    m_bPreviewMoved = true;
                }
            }
            return;
        }

        // Released: one step for the whole drag, none for a click that only selected.
        m_bPreviewDrag = false;
        if (m_bPreviewMoved)
        {
            UICanvasOps::CommitEdit(m_Context, m_PreviewBefore, "Move Widget");
        }
    }

    void UICanvasPanel::NudgeSelected(const Vector2F& InDelta, const char* InLabel)
    {
        UIWidget* const lWidget = m_Context.UICanvasDocument.SelectedWidget();
        if (lWidget == nullptr || m_Context.UICanvasDocument.SelectedPath().empty())
        {
            return;
        }

        const OpaaxString lBefore = UICanvasOps::Snapshot(m_Context);

        lWidget->Rect.AnchoredPosition += InDelta;
        lWidget->InvalidateLayout();

        UICanvasOps::CommitEdit(m_Context, lBefore, InLabel);
    }

    void UICanvasPanel::DrawPreviewOverlay(const Vector2F& InOrigin)
    {
        EditorUICanvasDocument& lDoc    = m_Context.UICanvasDocument;
        const UICanvas&         lCanvas = lDoc.GetCanvas();
        const Vector2F          lSizePx = PreviewPx();

        ImDrawList* const lDraw = ImGui::GetWindowDrawList();
        const ImVec2 lClipMin{ InOrigin.x, InOrigin.y };
        const ImVec2 lClipMax{ InOrigin.x + lSizePx.x, InOrigin.y + lSizePx.y };

        // A widget may sit partly off the canvas; its outline stops at the image like it does.
        lDraw->PushClipRect(lClipMin, lClipMax, true);

        const auto lOutline = [&](const UIWidgetPath& InPath, const ImU32 InColor, const float InThickness)
        {
            UIWidget* const lWidget = InPath.empty() ? nullptr : lDoc.Resolve(InPath);
            if (lWidget == nullptr) { return; }

            // Two corners through the one canvas→pixel rule; Y flips, so min/max are re-sorted.
            const Vector2F lA = lCanvas.CanvasToScreen(lWidget->GetBounds().Min()) + InOrigin;
            const Vector2F lB = lCanvas.CanvasToScreen(lWidget->GetBounds().Max()) + InOrigin;

            lDraw->AddRect(ImVec2(std::min(lA.x, lB.x), std::min(lA.y, lB.y)),
                           ImVec2(std::max(lA.x, lB.x), std::max(lA.y, lB.y)),
                           InColor, 0.f, 0, InThickness);
        };

        if (m_HoverPath != lDoc.SelectedPath())
        {
            lOutline(m_HoverPath, IM_COL32(255, 255, 255, 90), 1.f);
        }
        lOutline(lDoc.SelectedPath(), IM_COL32(255, 170, 40, 255), 2.f);

        lDraw->PopClipRect();
    }

    Vector2F UICanvasPanel::PreviewPx() const
    {
        return { static_cast<float>(m_Size.x), static_cast<float>(m_Size.y) };
    }

    void UICanvasPanel::Shutdown()
    {
        m_RenderTarget.reset();
        m_Framebuffer.reset();

        OPAAX_LOG(LogUICanvasPanel, Info, "UICanvasPanel shutdown");
    }
}
