#include "Editor/Panels/UICanvasPanel.h"

#include <algorithm>
#include <cmath>       // std::fabs — the selection's pixel rect

#include <imgui.h>

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IProjectManager.h"   // the reference height the game draws at
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Operation/UICanvasOperations.h"
#include "Editor/Properties/PropertyDrawers.h"   // the specializations DrawProperties folds over
#include "Editor/UI/IEditorGui.h"                 // IsKeyboardOwnedByUI — the shortcut guard
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

        /** How far from the selection's edge a press still counts as a grip, in image pixels. */
        constexpr float GRIP_PX = 6.f;

        /** The registry every route here asks for the buildable type names. */
        const UIWidgetRegistry& Registry()
        {
            return OpaaxApplication::GetAppService<IEngine>().GetRegistries().UIWidgets();
        }

        ImGuiMouseCursor CursorFor(const ERectEdge InEdge) noexcept
        {
            switch (InEdge)
            {
                case ERectEdge::Left:        case ERectEdge::Right:       return ImGuiMouseCursor_ResizeEW;
                case ERectEdge::Top:         case ERectEdge::Bottom:      return ImGuiMouseCursor_ResizeNS;
                case ERectEdge::TopLeft:     case ERectEdge::BottomRight: return ImGuiMouseCursor_ResizeNWSE;
                case ERectEdge::TopRight:    case ERectEdge::BottomLeft:  return ImGuiMouseCursor_ResizeNESW;
                default:                                                  return ImGuiMouseCursor_Arrow;
            }
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

        UICanvas& lCanvas = m_Context.UICanvasDocument.GetCanvas();

        // THE LAYOUT TARGET is the chosen aspect, not the framebuffer (U13): the canvas lays out
        // as the game would show it, and the view below decides which part the image shows.
        const Vector2u32 lLayout = PreviewLayoutSize(m_Aspect, m_Size.x, m_Size.y);
        lCanvas.SetTargetSize(lLayout.x, lLayout.y);

        // The view: seeded at 1:1 once a real size exists (today's picture) — again for every
        // document opened, since each has its own reference height — then the gestures measured
        // during the draw are spent here, outside the ImGui pass (SEL3).
        if (m_ViewedPath != m_Context.UICanvasDocument.AbsPath())
        {
            m_ViewedPath  = m_Context.UICanvasDocument.AbsPath();
            m_bViewSeeded = false;
        }

        if (!m_bViewSeeded && m_Size.x > 1 && m_Size.y > 1)
        {
            ResetView();
            m_bViewSeeded = true;
        }

        if (m_bFitPending)
        {
            FitView();
            m_bFitPending = false;
        }

        m_ViewGesture.Spend(m_View, PreviewPx());

        // NAMING ITS OWN TARGET — the whole reason U4 gave a canvas submission one: the game's
        // canvases stay out of this framebuffer and this document stays out of the world (UI14) —
        // and, since U13, bringing its own VIEW, so the pass projects through the zoom and leaves
        // the layout target alone.
        const CameraView lView = PreviewView();
        m_Context.Engine.SubmitUICanvas(lCanvas, m_RenderTarget.get(), &lView);
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
        HandleShortcuts();
        ImGui::Separator();

        ImGui::BeginChild("UITree", ImVec2(m_TreeWidth, 0.f), ImGuiChildFlags_ResizeX);
        DrawTree();
        ImGui::Separator();
        DrawInspector();
        ImGui::EndChild();

        ImGui::SameLine();

        // NoScrollWithMouse: the wheel over the image is the zoom's, not a scroll's.
        ImGui::BeginChild("UIPreview", ImVec2(0.f, 0.f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollWithMouse);
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

    void UICanvasPanel::HandleShortcuts()
    {
        // THIS WINDOW's route (PrefabPanel's F): the chords fire with the tree or the preview focused,
        // never from another panel, and never while a name is being typed.
        if (m_Context.Gui.IsKeyboardOwnedByUI()) { return; }

        EditorUICanvasDocument& lDoc      = m_Context.UICanvasDocument;
        const UIWidgetPath      lSelected = lDoc.SelectedPath();

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D))
        {
            if (const UIWidgetPath lPath = UICanvasOps::DuplicateWidget(m_Context, lSelected); !lPath.empty())
            {
                lDoc.Select(lPath);
            }
        }

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C))
        {
            if (const OpaaxString lText = UICanvasOps::CopyWidget(m_Context, lSelected); !lText.IsEmpty())
            {
                ImGui::SetClipboardText(lText.CStr());
            }
        }

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_V))
        {
            // Under the selection when there is one, else at the top level — the Add menu's rule.
            const char* lClipboard = ImGui::GetClipboardText();
            if (const UIWidgetPath lPath = UICanvasOps::PasteWidget(m_Context, OpaaxString(lClipboard), lSelected);
                !lPath.empty())
            {
                lDoc.Select(lPath);
            }
        }

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_UpArrow))   { UICanvasOps::MoveWidget(m_Context, lSelected, -1); }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_DownArrow)) { UICanvasOps::MoveWidget(m_Context, lSelected, +1); }
    }

    // =============================================================================
    // The tree
    // =============================================================================

    void UICanvasPanel::DrawTree()
    {
        DrawAddMenu();

        ImGui::SameLine();

        const UIWidgetPath lSelected = m_Context.UICanvasDocument.SelectedPath();

        ImGui::BeginDisabled(lSelected.empty());
        if (ImGui::Button("Delete"))
        {
            UICanvasOps::RemoveWidget(m_Context, lSelected);
        }

        ImGui::SameLine();
        if (ImGui::Button("Duplicate"))
        {
            if (const UIWidgetPath lPath = UICanvasOps::DuplicateWidget(m_Context, lSelected); !lPath.empty())
            {
                m_Context.UICanvasDocument.Select(lPath);
            }
        }

        // Sibling order is draw order AND, under a layout container, the order on screen.
        ImGui::SameLine();
        if (ImGui::ArrowButton("MoveUp", ImGuiDir_Up))     { UICanvasOps::MoveWidget(m_Context, lSelected, -1); }
        ImGui::SameLine();
        if (ImGui::ArrowButton("MoveDown", ImGuiDir_Down)) { UICanvasOps::MoveWidget(m_Context, lSelected, +1); }
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
        UICanvas&       lCanvas = m_Context.UICanvasDocument.GetCanvas();
        UIWidget* const lWidget = m_Context.UICanvasDocument.SelectedPath().empty()
                                      ? nullptr
                                      : m_Context.UICanvasDocument.SelectedWidget();

        // THE GESTURE brackets the FIELDS below and nothing else. IsAnyItemActive is frame-global,
        // so a verb button pressed elsewhere this frame (Delete, Duplicate, a preset) would open a
        // gesture whose commit re-records the verb's own step as a phantom "Edit Widget". Sampled
        // before the fields: an item already active is not one of theirs.
        bool lActiveBefore = ImGui::IsAnyItemActive();

        if (lWidget == nullptr)
        {
            // Nothing selected is the CANVAS: its one field lives here, under the same gesture as a
            // widget's, and the step it records carries the height with the tree (UI15).
            ImGui::TextDisabled("Canvas");

            float lHeight = lCanvas.GetReferenceHeight();
            ImGui::SetNextItemWidth(120.f);
            if (ImGui::DragFloat("Reference height", &lHeight, 1.f, 16.f, 8192.f, "%.0f"))
            {
                lCanvas.SetReferenceHeight(lHeight);
            }

            // The game draws every asset at the PROJECT's height (UI2); this one is the preview's.
            // A difference is said here, where the author is, before the mount log says it again.
            const float lProjectHeight = OpaaxApplication::GetAppService<IProjectManager>().UIReferenceHeight();
            ImGui::SameLine();
            ImGui::TextDisabled("(project: %.0f)", lProjectHeight);
            if (lHeight != lProjectHeight)
            {
                ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f),
                                   "Authored at %.0f, the game draws at %.0f - it will not look like this preview.",
                                   lHeight, lProjectHeight);
            }
        }
        else
        {
            ImGui::TextDisabled("%s", lWidget->GetTypeName().CStr());

            // The RESOLVED rect, read-only: what the anchors actually produced, so an odd one reads
            // as numbers here rather than as "it went somewhere" in the preview.
            const Bounds2D& lBounds = lWidget->GetBounds();
            ImGui::TextDisabled("resolved  min (%.0f, %.0f)  size %.0f x %.0f%s",
                                lBounds.Min().x, lBounds.Min().y, lBounds.Size().x, lBounds.Size().y,
                                lWidget->GetParent() != nullptr && lWidget->GetParent()->ArrangesChildren()
                                    ? "  (placed by parent)" : "");

            // The anchors as an author sets them — a preset, not four vectors — above the Rect
            // that shows what it wrote. Meaningless under a container, which places the child.
            const bool lArranged = lWidget->GetParent() != nullptr && lWidget->GetParent()->ArrangesChildren();
            ImGui::BeginDisabled(lArranged);
            if (ImGui::Button("Anchors...")) { ImGui::OpenPopup("UIAnchorPresets"); }
            ImGui::EndDisabled();

            if (ImGui::BeginPopup("UIAnchorPresets"))
            {
                DrawAnchorPresets(*lWidget);
                ImGui::EndPopup();
            }

            // The preset is a verb with its own step; re-sampled so its press does not open the gesture.
            lActiveBefore = ImGui::IsAnyItemActive();

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
        }

        // THE GESTURE: a drag is many frames, and a step per frame would flood the history. Open on
        // the first active frame — one of THESE fields, not something active before them — and
        // close when nothing is active any more: FontFamilyPanel's shape, with the whole TREE as
        // the before-image because that is what a step carries (UI15).
        const bool lActive = ImGui::IsAnyItemActive();

        if (lActive && !lActiveBefore && !m_bWasItemActive && !m_bGestureOpen)
        {
            m_GestureBefore = UICanvasOps::Snapshot(m_Context);
            m_bGestureOpen  = true;
        }
        else if (!lActive && m_bWasItemActive && m_bGestureOpen)
        {
            UICanvasOps::CommitEdit(m_Context, m_GestureBefore, lWidget != nullptr ? "Edit Widget" : "Edit Canvas");
            m_bGestureOpen = false;
        }

        m_bWasItemActive = lActive;

        // The widget's own state may have changed under the drawer; it cannot know to re-layout.
        if (lWidget != nullptr)
        {
            lWidget->InvalidateLayout();
        }
    }

    void UICanvasPanel::DrawAnchorPresets(UIWidget& InWidget)
    {
        static constexpr const char* kColumns[4] = { "Left", "Center", "Right", "Stretch" };
        static constexpr const char* kRows[4]    = { "Top", "Middle", "Bottom", "Stretch" };

        const UIAnchorPreset lCurrent = CurrentAnchorPreset(InWidget.Rect);

        ImGui::TextDisabled("The widget stays where it is; only what it does on a resize changes.");

        if (!ImGui::BeginTable("UIAnchorGrid", 5, ImGuiTableFlags_SizingFixedFit))
        {
            return;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        for (const char* lColumn : kColumns)
        {
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", lColumn);
        }

        for (Uint8 lY = 0; lY < 4; ++lY)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", kRows[lY]);

            for (Uint8 lX = 0; lX < 4; ++lX)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(lY * 4 + lX);

                const bool lIsCurrent = lCurrent.bKnown
                                     && lCurrent.X == static_cast<EUIAnchorX>(lX)
                                     && lCurrent.Y == static_cast<EUIAnchorY>(lY);

                if (ImGui::Button(lIsCurrent ? "[*]" : " . ", ImVec2(36.f, 0.f)))
                {
                    const OpaaxString lBefore = UICanvasOps::Snapshot(m_Context);

                    ApplyAnchorPreset(InWidget.Rect, static_cast<EUIAnchorX>(lX), static_cast<EUIAnchorY>(lY),
                                      InWidget.GetBounds(), InWidget.GetParent()->GetBounds());
                    InWidget.InvalidateLayout();

                    UICanvasOps::CommitEdit(m_Context, lBefore, "Anchor Preset");
                    ImGui::CloseCurrentPopup();
                }

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    // =============================================================================
    // The preview
    // =============================================================================

    void UICanvasPanel::DrawPreview()
    {
        DrawPreviewToolbar();

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
        const bool     lHovered = ImGui::IsItemHovered();

        // The view's gestures — middle-drag and the wheel — beside the designer's left button.
        m_ViewGesture.Measure(lHovered, lOrigin, lSizePx);

        // F frames the canvas: THIS WINDOW's route, PrefabPanel's idiom, and never from a name field.
        if (lHovered && !m_Context.Gui.IsKeyboardOwnedByUI() && ImGui::Shortcut(ImGuiKey_F))
        {
            m_bFitPending = true;
        }

        MeasurePreviewGesture(lHovered, lOrigin);
        DrawPreviewOverlay(lOrigin);
    }

    void UICanvasPanel::DrawPreviewToolbar()
    {
        ImGui::SetNextItemWidth(80.f);
        if (ImGui::BeginCombo("##UIPreviewAspect", ToString(m_Aspect)))
        {
            for (const EUIPreviewAspect lAspect : kUIPreviewAspects)
            {
                if (ImGui::Selectable(ToString(lAspect), lAspect == m_Aspect))
                {
                    m_Aspect      = lAspect;
                    m_bFitPending = true;   // a new frame is worth seeing whole
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) { ImGui::SetTooltip("The aspect the canvas lays out at — what the game's window would be."); }

        ImGui::SameLine();
        if (ImGui::SmallButton("Fit")) { m_bFitPending = true; }
        ImGui::SameLine();
        if (ImGui::SmallButton("1:1")) { ResetView(); }

        // One canvas unit per image pixel is 100%: the zoom reads in the author's terms.
        const float lReference = m_Context.UICanvasDocument.GetCanvas().GetReferenceHeight();
        ImGui::SameLine();
        ImGui::TextDisabled("%.0f%%", m_View.GetOrthoSize() > 0.f ? 100.f * (lReference * 0.5f) / m_View.GetOrthoSize() : 0.f);
        ImGui::SameLine();
        ImGui::TextDisabled("(wheel: zoom, middle-drag: pan, F: fit)");
    }

    void UICanvasPanel::MeasurePreviewGesture(const bool bInHovered, const Vector2F& InOrigin)
    {
        EditorUICanvasDocument& lDoc    = m_Context.UICanvasDocument;
        const UICanvas&         lCanvas = lDoc.GetCanvas();
        const ImGuiIO&          lIO     = ImGui::GetIO();

        const Vector2F lLocalPx{ lIO.MousePos.x - InOrigin.x, lIO.MousePos.y - InOrigin.y };

        m_HoverPath = bInHovered ? lDoc.PickAt(PreviewToCanvas(lLocalPx)) : UIWidgetPath{};

        // A grip on the SELECTION wins over whatever is under the pointer: the outline is drawn over
        // the preview, so its handles are what the author sees there. The eight regions and their
        // corner priority are EditorRectGeometry's — the title bar's and the sheet editor's.
        ERectEdge lEdge = ERectEdge::None;
        if (bInHovered && !m_bPreviewDrag && !lDoc.SelectedPath().empty() && lDoc.SelectedWidget() != nullptr)
        {
            lEdge = HitTestRect(SelectionRectPx(), lLocalPx.x, lLocalPx.y, GRIP_PX);
        }

        if (const ERectEdge lShown = m_bPreviewDrag ? m_PreviewEdge : lEdge; lShown != ERectEdge::None)
        {
            ImGui::SetMouseCursor(CursorFor(lShown));
        }

        if (bInHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (lEdge != ERectEdge::None)
            {
                m_PressRectPx = SelectionRectPx();   // the resize is measured from here
            }
            else
            {
                lDoc.Select(m_HoverPath);   // empty is bare canvas, and that clears
            }

            m_PreviewEdge      = lEdge;
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
            // The TOTAL since the press: exact across frames, and the threshold ImGui applies before
            // calling it a drag keeps a plain click from nudging.
            UIWidget* const lWidget = lDoc.SelectedWidget();
            if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left) || lWidget == nullptr || lDoc.SelectedPath().empty())
            {
                return;
            }

            const ImVec2 lTotal = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.f);

            if (m_PreviewEdge != ERectEdge::None)
            {
                // Resized in PIXEL space, where the grip was hit, then the two corners back through
                // the one pixel→canvas rule; FitRect keeps the anchors and solves the rest.
                const TEditorRect<float> lRect = ResizeRect(m_PressRectPx, m_PreviewEdge, lTotal.x, lTotal.y, 1.f, 1.f);

                const Bounds2D lTarget = Bounds2D::FromMinMax(
                    PreviewToCanvas({ lRect.X, lRect.Y }),
                    PreviewToCanvas({ lRect.X + lRect.Width, lRect.Y + lRect.Height }));

                FitRect(lWidget->Rect, lTarget, lWidget->GetParent()->GetBounds());
                lWidget->InvalidateLayout();
                m_bPreviewMoved = true;
                return;
            }

            const Vector2F lStepPx{ lTotal.x - m_PreviewAppliedPx.x, lTotal.y - m_PreviewAppliedPx.y };
            m_PreviewAppliedPx = { lTotal.x, lTotal.y };

            // Under a layout container the position is the container's, so a drag moves nothing —
            // the resize above still counts, since SizeDelta is what the container reads (UI23).
            const bool lArranged = lWidget->GetParent() != nullptr && lWidget->GetParent()->ArrangesChildren();

            if (!lArranged && (lStepPx.x != 0.f || lStepPx.y != 0.f))
            {
                // Pixels → canvas units, Y flipped: the canvas is Y-up and the image is not.
                const float lUnits = UnitsPerPreviewPixel();
                lWidget->Rect.AnchoredPosition += Vector2F{ lStepPx.x * lUnits, -lStepPx.y * lUnits };
                lWidget->InvalidateLayout();
                m_bPreviewMoved = true;
            }
            return;
        }

        // Released: one step for the whole drag, none for a click that only selected.
        m_bPreviewDrag = false;
        if (m_bPreviewMoved)
        {
            UICanvasOps::CommitEdit(m_Context, m_PreviewBefore,
                                    m_PreviewEdge != ERectEdge::None ? "Resize Widget" : "Move Widget");
        }
        m_PreviewEdge = ERectEdge::None;
    }

    void UICanvasPanel::NudgeSelected(const Vector2F& InDelta, const char* InLabel)
    {
        UIWidget* const lWidget = m_Context.UICanvasDocument.SelectedWidget();
        if (lWidget == nullptr || m_Context.UICanvasDocument.SelectedPath().empty()
            || (lWidget->GetParent() != nullptr && lWidget->GetParent()->ArrangesChildren()))
        {
            return;   // nothing, or a widget whose position is its container's
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

        const auto lRect = [&](const Bounds2D& InBounds, const ImU32 InColor, const float InThickness)
        {
            // Two corners through the one canvas→pixel rule; Y flips, so min/max are re-sorted.
            const Vector2F lA = CanvasToPreview(InBounds.Min()) + InOrigin;
            const Vector2F lB = CanvasToPreview(InBounds.Max()) + InOrigin;

            lDraw->AddRect(ImVec2(std::min(lA.x, lB.x), std::min(lA.y, lB.y)),
                           ImVec2(std::max(lA.x, lB.x), std::max(lA.y, lB.y)),
                           InColor, 0.f, 0, InThickness);
        };

        const auto lOutline = [&](const UIWidgetPath& InPath, const ImU32 InColor, const float InThickness)
        {
            UIWidget* const lWidget = InPath.empty() ? nullptr : lDoc.Resolve(InPath);
            if (lWidget != nullptr) { lRect(lWidget->GetBounds(), InColor, InThickness); }
        };

        // The SCREEN's edge — the layout target — under everything: zoomed out, it is what says
        // where the game's window ends; zoomed in, it is off the image and costs nothing.
        lRect(lCanvas.GetVisibleBounds(), IM_COL32(200, 200, 200, 110), 1.f);

        if (m_HoverPath != lDoc.SelectedPath())
        {
            lOutline(m_HoverPath, IM_COL32(255, 255, 255, 90), 1.f);
        }
        lOutline(lDoc.SelectedPath(), IM_COL32(255, 170, 40, 255), 2.f);

        // The eight grips, on the same rect the hit-test reads.
        if (!lDoc.SelectedPath().empty() && lDoc.SelectedWidget() != nullptr)
        {
            const TEditorRect<float> lRect = SelectionRectPx();
            const float lXs[3] = { lRect.X, lRect.X + lRect.Width * 0.5f, lRect.X + lRect.Width };
            const float lYs[3] = { lRect.Y, lRect.Y + lRect.Height * 0.5f, lRect.Y + lRect.Height };

            for (const float lX : lXs)
            {
                for (const float lY : lYs)
                {
                    if (lX == lXs[1] && lY == lYs[1]) { continue; }   // the centre is not a grip

                    const ImVec2 lAt{ InOrigin.x + lX, InOrigin.y + lY };
                    lDraw->AddRectFilled({ lAt.x - GRIP_PX * 0.5f, lAt.y - GRIP_PX * 0.5f },
                                         { lAt.x + GRIP_PX * 0.5f, lAt.y + GRIP_PX * 0.5f },
                                         IM_COL32(255, 170, 40, 255));
                }
            }
        }

        lDraw->PopClipRect();
    }

    Vector2F UICanvasPanel::PreviewPx() const
    {
        return { static_cast<float>(m_Size.x), static_cast<float>(m_Size.y) };
    }

    TEditorRect<float> UICanvasPanel::SelectionRectPx() const
    {
        const EditorUICanvasDocument& lDoc    = m_Context.UICanvasDocument;
        const UIWidget* const         lWidget = lDoc.SelectedWidget();

        if (lWidget == nullptr) { return {}; }

        // Two corners through the one canvas→pixel rule; Y flips, so min/max are re-sorted.
        const Vector2F lA = CanvasToPreview(lWidget->GetBounds().Min());
        const Vector2F lB = CanvasToPreview(lWidget->GetBounds().Max());

        return { std::min(lA.x, lB.x), std::min(lA.y, lB.y), std::fabs(lB.x - lA.x), std::fabs(lB.y - lA.y) };
    }

    // =============================================================================
    // The view
    // =============================================================================

    CameraView UICanvasPanel::PreviewView() const noexcept
    {
        return CameraView{ m_View.GetPosition(), m_View.GetOrthoSize() };
    }

    Vector2F UICanvasPanel::PreviewToCanvas(const Vector2F& InLocalPx) const noexcept
    {
        return ScreenToWorld(PreviewView(), PreviewPx(), InLocalPx);
    }

    Vector2F UICanvasPanel::CanvasToPreview(const Vector2F& InCanvasPoint) const noexcept
    {
        return WorldToScreen(PreviewView(), PreviewPx(), InCanvasPoint);
    }

    float UICanvasPanel::UnitsPerPreviewPixel() const noexcept
    {
        return WorldPerPixel(PreviewView(), PreviewPx().y);
    }

    void UICanvasPanel::FitView()
    {
        m_View.FocusOn(m_Context.UICanvasDocument.GetCanvas().GetVisibleBounds(), PreviewPx());
    }

    void UICanvasPanel::ResetView()
    {
        // Half the reference height IS the canvas's own view (UI2): one canvas unit per image pixel.
        m_View.Set({ 0.f, 0.f }, m_Context.UICanvasDocument.GetCanvas().GetReferenceHeight() * 0.5f);
    }

    void UICanvasPanel::Shutdown()
    {
        m_RenderTarget.reset();
        m_Framebuffer.reset();
    }
}
