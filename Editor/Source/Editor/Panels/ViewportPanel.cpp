#include "Editor/Panels/ViewportPanel.h"

#include "Editor/Camera/EditorCamera.h"     // the Edit camera
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Application/Services/IPaths.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/PIE/PlayInEditor.h"             // IsEdit
#include "Editor/Input/InputRoute.h"        // hover/focus is pushed to it
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Operation/EditorGizmo.hpp"      // gizmo settings
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Operation/EditorViewport.hpp"
#include "Editor/Operation/EntityOps.h"          // gizmo drags write through it
#include "Editor/EditorMapDocument.h"            // a drop goes into the focused map
#include "Editor/Resources/ResourceDragDrop.h"   // typed drag payloads
#include "Resources/ResourceManager.h"   // before PrefabResource (completes LoadContext)
#include "World/Prefab/PrefabResource.hpp"
#include "Editor/UI/IEditorUIBackend.h"
#include "Editor/Viewport/ViewportOverlays.h"    // outline and icons, shared with the prefab panel

#include "Application/Services/IEngine.h"
#include "Core/Log/Logger.h"

#include <cmath>                            // ceil/floor/log10/pow

#include "Renderer/DebugDraw.h"             // selection outline + grid
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "RHI/Framebuffer.h"                // IFramebuffer + FramebufferSpec

#include "Core/Maths/Bounds2D.h"
#include "Renderer/CameraView.h"            // ScreenToWorld

#include "World/Entity/Entity.h"
#include "World/World.h"
#include "World/WorldManager.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    ViewportPanel::ViewportPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    ViewportPanel::~ViewportPanel() = default;

    void ViewportPanel::Startup()
    {
        // The engine's device creates the FBO; the panel owns it.
        m_Framebuffer = m_Context.Engine.CreateFramebuffer(
            FramebufferSpec{ m_viewportSize.x, m_viewportSize.y, /*DepthStencil*/ true });

        if (m_Framebuffer == nullptr)
        {
            // Log it: otherwise the panel just stays blank.
            OPAAX_LOG(LogViewportPanel, Error, "ViewportPanel startup — the engine created no framebuffer; "
                                               "the viewport will stay blank.");
            return;
        }

        m_RenderTarget = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());
    }

    // =========================================================================
    // OnPreRender — runs in EditorService::BeginFrame, before Engine().Loop() renders the world,
    //   so everything here reaches this frame's render. The resize and the outline are separate
    //   steps (ApplyPendingResize returns early when nothing is pending).
    // =========================================================================
    void ViewportPanel::OnPreRender()
    {
        // Cleared here, set again in DrawContents: a hidden viewport reports "not hovered".
        m_Context.Route.SetViewportFocus(false, false);

        // First: the click was made on the last rendered frame, so it is tested against that frame's
        // size and camera (the calls below change them).
        ApplyPendingPick();
        ApplyGizmoDrag();

        ApplyPendingResize();
        ApplyCameraGesture();

        // First overlay: it uses the camera just set, and it is the only one on the Background layer.
        EnqueueGrid();

        EnqueueSelectionOutline();
        EnqueueEntityIcons();

        // Last: publish the view once everything that could move it has run.
        SubmitView();
    }

    // =========================================================================
    // SubmitView — this panel's view, submitted every frame, hidden or not.
    //   Uses the active world's view (a Play copy is framed by its CameraComponent).
    // =========================================================================
    void ViewportPanel::SubmitView()
    {
        if (m_RenderTarget == nullptr)
        {
            return;
        }

        World* const lWorld = m_Context.Worlds.GetActiveWorld();

        // The game's UI is drawn over this view (Play is watched here).
        m_Context.Engine.SubmitRenderView(*m_RenderTarget,
                                          lWorld != nullptr ? lWorld->GetCameraView() : CameraView{},
                                          /*bInDrawOverlays*/ true, /*InSource*/ nullptr, /*bInDrawUI*/ true);
    }

    // =========================================================================
    // ActiveView / ViewportToWorld / AnchorHalfExtent — conversions for selection. They use the
    //   active world's view, so picking works the same in Edit and Play.
    // =========================================================================
    CameraView ViewportPanel::ActiveView() const
    {
        World* const lWorld = m_Context.Worlds.GetActiveWorld();
        return lWorld != nullptr ? lWorld->GetCameraView() : CameraView{};
    }

    Vector2F ViewportPanel::ViewportPx() const
    {
        return { static_cast<float>(m_viewportSize.x), static_cast<float>(m_viewportSize.y) };
    }

    Vector2F ViewportPanel::ViewportToWorld(const Vector2F& InLocalPx) const
    {
        return ScreenToWorld(ActiveView(), ViewportPx(), InLocalPx);
    }

    float ViewportPanel::WorldPerPixel() const
    {
        return Opaax::WorldPerPixel(ActiveView(), ViewportPx().y);
    }

    float ViewportPanel::AnchorHalfExtent() const
    {
        return ViewportOverlays::AnchorHalfExtent(ActiveView(), ViewportPx());
    }

    void ViewportPanel::ApplyPendingPick()
    {
        const PickGesture::Pick lPick = m_PickGesture.Take();   // cleared first, used or not

        // A press that began in Edit and was released in Play still stores a pick: dropped here.
        if (!m_Context.PIE.IsEdit())
        {
            return;
        }

        if (World* lWorld = m_Context.Worlds.GetActiveWorld())
        {
            PickGesture::Apply(lPick, *lWorld, m_Context.Selection, ViewportPx(), AnchorHalfExtent());
        }
    }

    // =========================================================================
    // ApplyGizmoDrag — applies what the gesture stored, through the transform command (which has
    //   the Play guard), then closes the drag's undo step. Before the resize and camera change,
    //   like ApplyPendingPick.
    // =========================================================================
    void ViewportPanel::ApplyGizmoDrag()
    {
        EntityOps::TransformDelta lDelta;
        if (m_Gizmo.TakeDelta(m_Context.Gizmo, lDelta))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_TRANSFORM_SELECTED, m_Context, lDelta);
        }

        m_Gizmo.Close(m_Context, m_Context.Undo);
    }

    // =========================================================================
    // ApplyCameraGesture — applies what DrawContents measured last frame, then publishes the camera
    //   as the world's view. After ApplyPendingResize and before Engine().Loop().
    // =========================================================================
    void ViewportPanel::ApplyCameraGesture()
    {
        m_CameraGesture.Spend(m_Context.Camera, ViewportPx());

        // Refuses a Play world itself (the Play copy is framed by its CameraComponent).
        if (World* lWorld = m_Context.Worlds.GetActiveWorld())
        {
            m_Context.Camera.Apply(*lWorld);
        }
    }

    void ViewportPanel::ApplyPendingResize()
    {
        if (m_viewportPendingSize.x == 0 || m_viewportPendingSize.y == 0)
        {
            return;
        }
        if (m_viewportPendingSize.x == m_viewportSize.x && m_viewportPendingSize.y == m_viewportSize.y)
        {
            return;
        }

        m_viewportSize  = m_viewportPendingSize;
        
        if (m_Framebuffer != nullptr)
        {
            m_Framebuffer->Resize(m_viewportSize.x, m_viewportSize.y);
        }
    }

    // =========================================================================
    // EnqueueGrid — the snap grid, on the Background layer (under everything). Its spacing is the
    //   translate snap step. Edit worlds only. Only visible lines are emitted, and the spacing grows
    //   by decades when cells get too small.
    // =========================================================================
    float ViewportPanel::GridSpacing() const
    {
        const float lWorldPerPixel = WorldPerPixel();
        const float lStep          = m_Context.Gizmo.GetSnapStep(EGizmoMode::Translate);

        if (lStep <= 0.f || lWorldPerPixel <= 0.f)
        {
            return lStep;
        }

        // Decade step-up, computed (not looped).
        const float lMinWorld = m_GridMinCellPx * lWorldPerPixel;

        return lStep < lMinWorld
                   ? lStep * std::pow(10.f, std::ceil(std::log10(lMinWorld / lStep)))
                   : lStep;
    }

    float ViewportPanel::TranslateSnapStep() const
    {
        // While the grid is visible, snap to what is drawn (zoomed out, the grid is coarser than the
        // authored step). With the grid hidden, the authored step is used.
        return m_Context.Viewport.IsGridVisible() ? GridSpacing()
                                                  : m_Context.Gizmo.GetSnapStep(EGizmoMode::Translate);
    }

    void ViewportPanel::EnqueueGrid()
    {
        World* const lWorld = m_Context.Worlds.GetActiveWorld();

        if (!m_Context.Viewport.IsGridVisible() || lWorld == nullptr
            || lWorld->GetMode() != EWorldMode::Edit || m_viewportSize.y == 0)
        {
            return;
        }

        const float lWorldPerPixel = WorldPerPixel();
        const float lSpacing       = GridSpacing();

        if (lSpacing <= 0.f || lWorldPerPixel <= 0.f)
        {
            return;
        }

        // The visible rect, from the image corners (the same conversion the click uses).
        const Bounds2D lView = Bounds2D::FromMinMax(
            ViewportToWorld({ 0.f, 0.f }),
            ViewportToWorld({ static_cast<float>(m_viewportSize.x), static_cast<float>(m_viewportSize.y) }));

        const Vector2F lMin = lView.Min();
        const Vector2F lMax = lView.Max();

        const Int32 lFirstX = static_cast<Int32>(std::ceil(lMin.x / lSpacing));
        const Int32 lLastX  = static_cast<Int32>(std::floor(lMax.x / lSpacing));
        const Int32 lFirstY = static_cast<Int32>(std::ceil(lMin.y / lSpacing));
        const Int32 lLastY  = static_cast<Int32>(std::floor(lMax.y / lSpacing));

        const Int64 lCount = static_cast<Int64>(lLastX - lFirstX + 1) + static_cast<Int64>(lLastY - lFirstY + 1);
        if (lCount <= 0 || lCount > static_cast<Int64>(m_GridMaxLines))
        {
            return;
        }

        DebugDraw& lDraw = m_Context.Engine.GetDebugDraw();

        // Constant screen thickness (the selection outline is world-sized instead).
        const float lThin = m_GridThickness * lWorldPerPixel;
        const float lAxis = m_GridAxisThickness * lWorldPerPixel;

        for (Int32 lIndex = lFirstX; lIndex <= lLastX; ++lIndex)
        {
            const float lX = static_cast<float>(lIndex) * lSpacing;

            // x == 0 is the Y axis (the vertical line).
            const bool bAxis = lIndex == 0;

            lDraw.DrawLine({ lX, lMin.y }, { lX, lMax.y },
                           bAxis ? m_GridAxisYColor : m_GridColor,
                           bAxis ? lAxis : lThin, ERenderLayer::Background);
        }

        for (Int32 lIndex = lFirstY; lIndex <= lLastY; ++lIndex)
        {
            const float lY    = static_cast<float>(lIndex) * lSpacing;
            const bool  bAxis = lIndex == 0;

            lDraw.DrawLine({ lMin.x, lY }, { lMax.x, lY },
                           bAxis ? m_GridAxisXColor : m_GridColor,
                           bAxis ? lAxis : lThin, ERenderLayer::Background);
        }
    }

    // =========================================================================
    // EnqueueSelectionOutline / EnqueueEntityIcons — submitted every frame (DebugDraw is cleared
    //   each frame). Both log once when they draw something.
    // =========================================================================
    void ViewportPanel::EnqueueSelectionOutline()
    {
        World* const lWorld = m_Context.Selection.GetWorld();
        if (lWorld == nullptr)
        {
            return;
        }

        ViewportOverlays::EnqueueSelectionOutline(
            m_Context.Engine.GetDebugDraw(), *lWorld, m_Context.Selection.Ids(), AnchorHalfExtent());
        ViewportOverlays::EnqueueLightGizmos(
            m_Context.Engine.GetDebugDraw(), *lWorld, m_Context.Selection.Ids(), AnchorHalfExtent());
    }

    void ViewportPanel::EnqueueEntityIcons()
    {
        World* lWorld = m_Context.Worlds.GetActiveWorld();

        // Edit worlds only: a running game must look like the game.
        if (lWorld == nullptr || lWorld->GetMode() != EWorldMode::Edit)
        {
            return;
        }

        ViewportOverlays::EnqueueEntityIcons(m_Context.Engine.GetDebugDraw(), *lWorld, AnchorHalfExtent());
    }

    // =========================================================================
    // DrawToolbarOverlay — the tool strip over the viewport. Drawn before the gestures, and its rect
    //   is removed from the image hover, so clicking a button does not start a marquee.
    // =========================================================================
    bool ViewportPanel::DrawToolbarOverlay(const Vector2F& InOrigin)
    {
        const ViewportToolbarRegistry& lTools = m_Context.Extensions.ViewportTools();

        // Edit worlds only.
        if (lTools.IsEmpty() || !m_Context.PIE.IsEdit())
        {
            return false;
        }

        ImGui::SetCursorScreenPos(ImVec2{ InOrigin.x + m_ToolbarInset, InOrigin.y + m_ToolbarInset });

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, m_ToolbarRounding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{ m_ToolbarBg.r, m_ToolbarBg.g, m_ToolbarBg.b, m_ToolbarBg.a });

        // Auto-resize: the strip is exactly as wide as the registered tools.
        if (ImGui::BeginChild("##ViewportToolbar", ImVec2{ 0.f, 0.f },
                              ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders))
        {
            lTools.Draw(m_Context);
        }
        ImGui::EndChild();

        const bool lHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        return lHovered;
    }

    EditorImage ViewportPanel::GetViewportImage() const
    {
        return m_Framebuffer != nullptr ? m_Context.UIBackend.GetViewportImage(*m_Framebuffer) : EditorImage{};
    }

    void ViewportPanel::DrawContents()
    {
        // Measure and cache the content region (applied next frame in OnPreRender).
        const ImVec2 lAvail = ImGui::GetContentRegionAvail();
        m_viewportPendingSize.x     = static_cast<Uint32>(lAvail.x);
        m_viewportPendingSize.y     = static_cast<Uint32>(lAvail.y);

        // Pushed for others: focus-selected needs the aspect.
        m_Context.Viewport.SetSizePx({ lAvail.x, lAvail.y });

        // Hover/focus can only be read while the window is current: measured here and pushed into the
        // input route, which reads them next frame.
        const bool lHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        m_Context.Route.SetViewportFocus(lHovered, ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows));

        const EditorImage lImg = GetViewportImage();

        // Draws a Dummy of the same size when the handle is null.
        ImguiWidgets::Image(lImg, lAvail);

        // Gestures use the image's hover, not the window's (which includes the title bar). The image
        // rect is read once here and passed to every measure. They read ImGui directly: with an Edit
        // world the engine never sees a mouse button.
        const bool   lImageRawHovered = ImGui::IsItemHovered();
        const ImVec2 lOrigin          = ImGui::GetItemRectMin();

        // The game's pointer, in viewport-image pixels (only knowable here). The route feeds it to the engine.
        const ImVec2 lMouse = ImGui::GetMousePos();
        m_Context.Route.SetPointerLocalPx({ lMouse.x - lOrigin.x, lMouse.y - lOrigin.y });

        // Drop a prefab to place one (BeginDragDropTarget refers to the last item: the image).
        // Recorded, not run (it creates entities); the local pixel is stored with it.
        {
            OpaaxString lDropped;
            if (AcceptResourceDragPayload(ResourceTypeID::Get<PrefabResource>(), lDropped))
            {
                const ImVec2 lMouse = ImGui::GetMousePos();

                m_PendingDropPrefab = lDropped;
                m_PendingDropPx     = Vector2F{ lMouse.x - lOrigin.x, lMouse.y - lOrigin.y };
            }
        }

        // The toolbar is drawn first and removed from the image hover, so gestures do not fire under
        // its buttons.
        const bool lToolbarHovered = DrawToolbarOverlay({ lOrigin.x, lOrigin.y });
        const bool lImageHovered   = lImageRawHovered && !lToolbarHovered;

        m_CameraGesture.Measure(lImageHovered, { lOrigin.x, lOrigin.y }, { lAvail.x, lAvail.y });

        // One left button, two users: a press on a handle is the gizmo's, so the marquee never sees it.
        // The gizmo uses the selection's world; TryGetGizmoPose refuses a Play world.
        World* const lGizmoWorld = m_Context.Selection.GetWorld();
        const bool   lGizmoOwns  = lGizmoWorld != nullptr
            && m_Gizmo.Measure(m_Context.Gizmo, *lGizmoWorld, m_Context.Selection, lGizmoWorld->GetCameraView(),
                               ViewportPx(), { lOrigin.x, lOrigin.y }, { lAvail.x, lAvail.y },
                               TranslateSnapStep(), lToolbarHovered, EUndoWorld::Active);

        // Edit worlds only: in Play a click on a UI button is the game's, not a pick.
        if (!lGizmoOwns)
        {
            m_PickGesture.Measure(lImageHovered && m_Context.PIE.IsEdit(), { lOrigin.x, lOrigin.y });
        }

        RunPendingDrop();
    }

    void ViewportPanel::RunPendingDrop()
    {
        const OpaaxString lPrefab = m_PendingDropPrefab;
        m_PendingDropPrefab = OpaaxString();   // cleared first, so a refused drop does not retry

        if (lPrefab.IsEmpty()) { return; }

        // ViewportToWorld uses the active world's camera, so the drop lands where the cursor was seen.
        const Vector2F lWorldPos = ViewportToWorld(m_PendingDropPx);

        EntityOps::InstantiatePrefab(m_Context, m_Context.Paths.AssetToAbsolute(lPrefab),
                                     m_Context.MapDocument.GetMapId(), &lWorldPos);
    }

    void ViewportPanel::Shutdown()
    {
        // Nothing to unregister: the view is submitted per frame.
        m_RenderTarget.reset();
        m_Framebuffer.reset();
    }
}
