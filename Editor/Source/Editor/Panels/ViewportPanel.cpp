#include "Editor/Panels/ViewportPanel.h"

#include "Editor/Camera/EditorCamera.h"     // the Edit viewpoint this panel drives (①)
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/PIE/PlayInEditor.h"             // IsEdit — the toolbar is authoring furniture
#include "Editor/Input/InputRoute.h"        // hover/focus is pushed, not read back out (D5 step 2)
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Operation/EditorGizmo.hpp"      // the gizmo SETTINGS — mode, snap step (③)
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Undo/EditorUndo.h"              // the stack a drag's step lands on (⑤)
#include "Editor/Operation/EditorViewport.hpp"
#include "Editor/Operation/EntityOps.h"          // the choke point a gizmo drag writes through (SEL6)
#include "Editor/EditorMapDocument.h"            // ⑦-C — a drop authors into the focused map
#include "Editor/Resources/ResourceDragDrop.h"   // ⑦-C — the typed payload the browser drags
#include "Engine/Subsystems/Resources/ResourceManager.h"   // before PrefabResource — LoadContext
#include "World/Prefab/PrefabResource.hpp"       // ⑦-C — which type the drop target accepts
#include "Editor/UI/IEditorUIBackend.h"
#include "Editor/Viewport/ViewportOverlays.h"    // the outline and the icons, shared with the prefab panel (P8)

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory

#include <cmath>                            // ceil/floor/log10/pow — the grid's decade step-up

#include "Renderer/DebugDraw.h"             // selection outline (M2c) + the grid's Background band
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "RHI/Framebuffer.h"                // IFramebuffer + FramebufferSpec (created by the device)

#include "Core/Maths/Bounds2D.h"
#include "Renderer/CameraView.h"            // ScreenToWorld (CAM2)

#include "World/Entity/Entity.h"
#include "World/World.h"                    // Apply publishes the camera as the world's view
#include "World/WorldManager.h"             // the active world is what gets it

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
        // The engine's device builds the FBO (F2a) — the panel owns it, but never picks the backend.
        m_Framebuffer = m_Context.Engine.CreateFramebuffer(
            FramebufferSpec{ m_viewportSize.x, m_viewportSize.y, /*DepthStencil*/ true });

        if (m_Framebuffer == nullptr)
        {
            // Loud, not silent: without this the only symptom is Draw()'s Dummy fallback — a blank
            // panel and a clean log, which is exactly the failure L15 is about.
            OPAAX_LOG(LogViewportPanel, Error, "ViewportPanel startup — the engine created no framebuffer; "
                                               "the viewport will stay blank.");
            return;
        }

        m_RenderTarget = MakeUnique<OffscreenRenderTarget>(m_Framebuffer.get());

        OPAAX_LOG(LogViewportPanel, Info, "ViewportPanel startup — offscreen FBO {}x{}", m_viewportSize.x, m_viewportSize.y);
    }

    // =========================================================================
    // OnPreRender — runs in EditorService::BeginFrame, i.e. BEFORE Engine().Loop() renders the world.
    // Everything here reaches this frame's render; anything done in Draw() would be one frame late.
    //
    // Both steps must run independently: ApplyPendingResize early-outs on the steady state (no
    // pending resize), which is every frame that isn't a resize — so the outline cannot ride at the
    // end of that body or it would almost never be queued.
    // =========================================================================
    void ViewportPanel::OnPreRender()
    {
        // Cleared here, set again in DrawContents. OnPreRender runs whether the panel is visible or
        // not, so a HIDDEN viewport reports "not hovered" instead of holding the last value it
        // measured — which would otherwise keep the input route open with no viewport on screen.
        m_Context.Route.SetViewportFocus(false, false);

        // FIRST, deliberately. The click being spent was made against the frame RENDERED last time,
        // so it has to be hit-tested against that frame's viewport size and camera — both of which
        // the two calls below are about to change.
        ApplyPendingPick();
        ApplyGizmoDrag();

        ApplyPendingResize();
        ApplyCameraGesture();

        // FIRST of the overlays — it reads the camera the two calls above just settled, and it is
        // the only one on the Background band, so everything else still draws over it.
        EnqueueGrid();

        EnqueueSelectionOutline();
        EnqueueEntityIcons();

        // LAST: the view is published once everything that could move it this frame has run.
        SubmitView();
    }

    // =========================================================================
    // SubmitView — this panel's standing claim on the frame, renewed every frame.
    //
    // UNCONDITIONAL, hidden or not: OnPreRender runs either way, and a hidden viewport that stopped
    // submitting would stop rendering the world it is about to be shown again with.
    //
    // The ACTIVE WORLD's view, not the editor camera's, for ViewportToWorld's reason — a PIE clone
    // is framed by its CameraComponent, and asking the world how it is framed is right in either
    // mode with no branch.
    // =========================================================================
    void ViewportPanel::SubmitView()
    {
        if (m_RenderTarget == nullptr)
        {
            return;
        }

        World* const lWorld = m_Context.Worlds.GetActiveWorld();

        m_Context.Engine.SubmitRenderView(*m_RenderTarget,
                                          lWorld != nullptr ? lWorld->GetCameraView() : CameraView{},
                                          /*bInDrawOverlays*/ true);
    }

    // =========================================================================
    // ActiveView / ViewportToWorld / AnchorHalfExtent — the conversions everything selection-related
    // needs. All read the ACTIVE WORLD's view rather than the editor camera, which is what keeps
    // picking free of an Edit/Play fork: a PIE clone is framed by its CameraComponent, and asking
    // the world how it is framed gets the right answer in either mode with no branch.
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
        const PickGesture::Pick lPick = m_PickGesture.Take();   // cleared FIRST, whether spent or not

        if (World* lWorld = m_Context.Worlds.GetActiveWorld())
        {
            PickGesture::Apply(lPick, *lWorld, m_Context.Selection, ViewportPx(), AnchorHalfExtent());
        }
    }

    // =========================================================================
    // ApplyGizmoDrag — spend what the gesture banked, through the transform COMMAND (SEL6): that
    // dispatch carries the PIE guard, which is the level's policy and not the gesture's. Then the
    // drag's falling edge, onto the level's stack.
    //
    // Beside ApplyPendingPick and for the same reason — the motion was measured against the frame
    // that was already RENDERED, so it is spent before the resize and the camera change that frame.
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
    // ApplyCameraGesture — spend what DrawContents measured last frame, then publish the camera
    // as the world's view. AFTER ApplyPendingResize so the pixel sizes below are this frame's,
    // and BEFORE Engine().Loop() so the result reaches this frame's render.
    //
    // Measure-then-apply rather than acting inside DrawContents: a panel's draw pass READS the
    // world, and anything that writes it runs outside the pass. Costs the one frame of lag the
    // deferred resize above already lives with.
    // =========================================================================
    void ViewportPanel::ApplyCameraGesture()
    {
        m_CameraGesture.Spend(m_Context.Camera, ViewportPx());

        // Refuses a Play world on its own (the clone is framed by its CameraComponent), so there is
        // no mode check here — one statement of that rule, and it lives with the camera.
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

        OPAAX_LOG(LogViewportPanel, Trace, "ViewportPanel resized to {}x{}", m_viewportSize.x, m_viewportSize.y);
    }

    // =========================================================================
    // EnqueueGrid — the snap grid, on the BACKGROUND band so it sits under everything it measures.
    //
    // Its spacing IS the translate snap step, which is the whole point: a grid that does not match
    // what a drag lands on is decoration, and worse than none. Edit worlds only.
    //
    // BOUNDED TWICE, because the world is infinite and the batch is not. The visible rect comes from
    // the same ScreenToWorld picking uses, so only lines actually on screen are emitted; and the
    // spacing climbs by DECADES once a cell would be finer than a few pixels, so zooming out turns
    // the grid coarse instead of emitting ten thousand invisible lines. The line cap behind both is
    // a guard against a step nobody anticipated, not the mechanism.
    // =========================================================================
    float ViewportPanel::GridSpacing() const
    {
        const float lWorldPerPixel = WorldPerPixel();
        const float lStep          = m_Context.Gizmo.GetSnapStep(EGizmoMode::Translate);

        if (lStep <= 0.f || lWorldPerPixel <= 0.f)
        {
            return lStep;
        }

        // DECADE STEP-UP, solved rather than looped so a pathological step cannot spin here.
        const float lMinWorld = m_GridMinCellPx * lWorldPerPixel;

        return lStep < lMinWorld
                   ? lStep * std::pow(10.f, std::ceil(std::log10(lMinWorld / lStep)))
                   : lStep;
    }

    float ViewportPanel::TranslateSnapStep() const
    {
        // WHILE THE GRID IS VISIBLE, SNAP TO WHAT IS DRAWN. Zoomed out, the grid coarsens by decades
        // and a drag snapping to the authored step would land between two visible lines — the author
        // sees 100-unit cells and gets 10-unit jumps. With the grid hidden there is nothing to
        // match, so the number they typed is honoured literally.
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

        // The visible rect, from the corners of the image — the same conversion the click uses, so
        // the grid cannot disagree with what a drag snaps to.
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

        // Screen-CONSTANT thickness: a grid is a hairline at every zoom, unlike the selection
        // outline, which is deliberately world-sized so it hugs the entity.
        const float lThin = m_GridThickness * lWorldPerPixel;
        const float lAxis = m_GridAxisThickness * lWorldPerPixel;

        for (Int32 lIndex = lFirstX; lIndex <= lLastX; ++lIndex)
        {
            const float lX = static_cast<float>(lIndex) * lSpacing;

            // x == 0 is the Y AXIS — the vertical line. Naming it the other way round is the easy
            // mistake here, and it would put the colours on the wrong lines.
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

        if (!m_bGridLogged)
        {
            OPAAX_LOG(LogViewportPanel, Info, "Snap grid enqueued — {} line(s) at {:.1f} world units",
                      lCount, lSpacing);
            m_bGridLogged = true;
        }
    }

    // =========================================================================
    // EnqueueSelectionOutline / EnqueueEntityIcons — re-submitted every frame by design: DebugDraw
    // is drained and cleared by the renderer each frame, so "still selected" means "queue it again".
    // Both log the SUCCESS branch once (L15): "selected something with no bounds" would otherwise
    // look exactly like a broken DebugDraw pipe.
    // =========================================================================
    void ViewportPanel::EnqueueSelectionOutline()
    {
        World* const lWorld = m_Context.Selection.GetWorld();
        if (lWorld == nullptr)
        {
            return;
        }

        const Uint64 lDrawn = ViewportOverlays::EnqueueSelectionOutline(
            m_Context.Engine.GetDebugDraw(), *lWorld, m_Context.Selection.Ids(), AnchorHalfExtent());

        if (!m_bOutlineLogged && lDrawn > 0)
        {
            OPAAX_LOG(LogViewportPanel, Info, "Selection outline enqueued for {} entity(ies)", lDrawn);
            m_bOutlineLogged = true;
        }
    }

    void ViewportPanel::EnqueueEntityIcons()
    {
        World* lWorld = m_Context.Worlds.GetActiveWorld();

        // Edit worlds only: an overlay is authoring furniture, and a running game must look like the
        // game. Same rule EditorCamera::Apply states for the view.
        if (lWorld == nullptr || lWorld->GetMode() != EWorldMode::Edit)
        {
            return;
        }

        const Uint64 lDrawn = ViewportOverlays::EnqueueEntityIcons(
            m_Context.Engine.GetDebugDraw(), *lWorld, AnchorHalfExtent());

        if (!m_bIconsLogged && lDrawn > 0)
        {
            OPAAX_LOG(LogViewportPanel, Info, "Drawing {} entity icon(s) — entities with nothing to render",
                      lDrawn);
            m_bIconsLogged = true;
        }
    }

    // =========================================================================
    // DrawToolbarOverlay — the strip over the viewport (③b), from whatever the registry holds.
    //
    // Drawn BEFORE the gesture measures and its hover returned to them, which is the whole reason
    // this is a separate step rather than three lines at the end of DrawContents. The measures gate
    // on the IMAGE's hover (SEL8), and a toolbar sitting ON the image is still "over the image" as
    // far as that test is concerned — so without subtracting this rect, clicking a toolbar button
    // would also start a marquee underneath it.
    // =========================================================================
    bool ViewportPanel::DrawToolbarOverlay(const Vector2F& InOrigin)
    {
        const ViewportToolbarRegistry& lTools = m_Context.Extensions.ViewportTools();

        // Edit worlds only — authoring furniture, the rule the icons and the gizmo already state.
        if (lTools.IsEmpty() || !m_Context.PIE.IsEdit())
        {
            return false;
        }

        ImGui::SetCursorScreenPos(ImVec2{ InOrigin.x + m_ToolbarInset, InOrigin.y + m_ToolbarInset });

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, m_ToolbarRounding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{ m_ToolbarBg.r, m_ToolbarBg.g, m_ToolbarBg.b, m_ToolbarBg.a });

        // Auto-resize on BOTH axes: the strip is exactly as wide as what is registered, so adding a
        // tool needs no size to be kept in step anywhere.
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
        // Measure the content region and cache it — OnPreRender applies it next frame (deferred resize).
        const ImVec2 lAvail = ImGui::GetContentRegionAvail();
        m_viewportPendingSize.x     = static_cast<Uint32>(lAvail.x);
        m_viewportPendingSize.y     = static_cast<Uint32>(lAvail.y);

        // Pushed for whoever is not this panel — focus-selected needs the aspect and cannot reach a
        // panel (the InputRoute::SetViewportFocus shape, one line down, for the same reason).
        m_Context.Viewport.SetSizePx({ lAvail.x, lAvail.y });

        // D5 step 2's inputs. ImGui can only answer these while the window is current, so they are
        // measured HERE and pushed into the route, which reads them next frame — the same one-frame
        // lag the deferred resize above already lives with, and for the same reason.
        const bool lHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        m_Context.Route.SetViewportFocus(lHovered, ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows));

        const EditorImage lImg = GetViewportImage();

        // Draws a Dummy of the same size when the handle is null, which is the reserve-space branch
        // this used to spell out below.
        ImguiWidgets::Image(lImg, lAvail);

        // BOTH GESTURES GATE ON THE IMAGE, not on the window. IsWindowHovered() is true over the
        // TITLE BAR too, so a title-bar press started a marquee that then painted itself while ImGui
        // moved the panel — the window-move and the selection box running at once. The image is the
        // only surface either gesture means anything on.
        //
        // THE ONLY PLACE THAT READS GetItemRect*, and it is taken here because these calls name the
        // LAST SUBMITTED ITEM — the image, right now, and nothing else afterwards. All three
        // measures below take the rect as an ARGUMENT instead of re-reading it. They used to
        // re-read, which was correct until ③b drew a toolbar (a child window IS an item) before
        // them, at which point the pan silently began wrapping inside the strip.
        //
        // The gestures read ImGui directly, and that is forced (IN8): an Edit world leaves the input
        // route closed, so InputManager never sees a button. The gate is the IMAGE's hover — never
        // io.WantCaptureMouse (L29), and never the WINDOW's, which is true over the title bar.
        const bool   lImageRawHovered = ImGui::IsItemHovered();
        const ImVec2 lOrigin          = ImGui::GetItemRectMin();

        // ⑦-C. DROP A PREFAB TO PLACE ONE. Taken here because BeginDragDropTarget names the LAST
        // SUBMITTED ITEM, which is the image — nothing has been submitted since, and the three
        // measures above only read its rect.
        //
        // RECORDED, not run: it creates entities, and doing that mid-pass is the same hazard the
        // Hierarchy's queue exists for (**MP7**). The local pixel is banked with it because the
        // origin is only knowable here.
        {
            OpaaxString lDropped;
            if (AcceptResourceDragPayload(ResourceTypeID::Get<PrefabResource>(), lDropped))
            {
                const ImVec2 lMouse = ImGui::GetMousePos();

                m_PendingDropPrefab = lDropped;
                m_PendingDropPx     = Vector2F{ lMouse.x - lOrigin.x, lMouse.y - lOrigin.y };
            }
        }

        // The toolbar is drawn FIRST and SUBTRACTED from the image's hover. It sits on top of the
        // image, so every gesture below would otherwise fire underneath its buttons — a click on
        // "Snap" would start a marquee, and a drag off a button would pan the camera.
        const bool lToolbarHovered = DrawToolbarOverlay({ lOrigin.x, lOrigin.y });
        const bool lImageHovered   = lImageRawHovered && !lToolbarHovered;

        m_CameraGesture.Measure(lImageHovered, { lOrigin.x, lOrigin.y }, { lAvail.x, lAvail.y });

        // ONE left button, TWO consumers, and the order is stated here once: a press that lands on a
        // handle belongs to the gizmo, so the marquee never sees it. Without this a drag on a handle
        // would move the entity AND rubber-band a selection over it.
        //
        // The gizmo measures against the world the SELECTION is in, which after a PIE start is the
        // clone — and TryGetGizmoPose refuses a Play world, so nothing draws there.
        World* const lGizmoWorld = m_Context.Selection.GetWorld();
        const bool   lGizmoOwns  = lGizmoWorld != nullptr
            && m_Gizmo.Measure(m_Context.Gizmo, *lGizmoWorld, m_Context.Selection, lGizmoWorld->GetCameraView(),
                               ViewportPx(), { lOrigin.x, lOrigin.y }, { lAvail.x, lAvail.y },
                               TranslateSnapStep(), lToolbarHovered, EUndoWorld::Active);

        if (!lGizmoOwns)
        {
            m_PickGesture.Measure(lImageHovered, { lOrigin.x, lOrigin.y });
        }

        if (lImg.IsValid() && !m_bImageLogged)
        {
            OPAAX_LOG(LogViewportPanel, Info, "Viewport displaying world FBO (handle={}, {}x{})", lImg.Handle, m_viewportSize.x, m_viewportSize.y);
            m_bImageLogged = true;
        }

        RunPendingDrop();
    }

    void ViewportPanel::RunPendingDrop()
    {
        const OpaaxString lPrefab = m_PendingDropPrefab;
        m_PendingDropPrefab = OpaaxString();   // cleared FIRST — a refused drop must not retry

        if (lPrefab.IsEmpty()) { return; }

        // ViewportToWorld reads the ACTIVE world's camera, so a drop lands where the author saw the
        // cursor rather than where the editor camera happens to be.
        const Vector2F lWorldPos = ViewportToWorld(m_PendingDropPx);

        EntityOps::InstantiatePrefab(m_Context, m_Context.Paths.AssetToAbsolute(lPrefab),
                                     m_Context.MapDocument.GetMapId(), &lWorldPos);
    }

    void ViewportPanel::Shutdown()
    {
        // NOTHING to unregister: the view is submitted per frame, so a panel that has stopped
        // running has already stopped being drawn. The dangling-target window this used to have to
        // order around does not exist.
        m_RenderTarget.reset();
        m_Framebuffer.reset();

        OPAAX_LOG(LogViewportPanel, Info, "ViewportPanel shutdown");
    }
}
