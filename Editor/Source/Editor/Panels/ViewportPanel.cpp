#include "Editor/Panels/ViewportPanel.h"

#include "Editor/Camera/EditorCamera.h"     // the Edit viewpoint this panel drives (①)
#include "Editor/EditorContext.h"
#include "Editor/Input/InputRoute.h"        // hover/focus is pushed, not read back out (D5 step 2)
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/UI/IEditorUIBackend.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory

#include "Renderer/DebugDraw.h"             // selection outline (M2c)
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "RHI/Framebuffer.h"                // IFramebuffer + FramebufferSpec (created by the device)

#include "World/Components/DummyComponent.h"
#include "World/Components/TransformComponent.h"
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

        m_Context.Engine.SetPrimaryRenderTarget(m_RenderTarget.get());

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

        ApplyPendingResize();
        ApplyCameraGesture();
        EnqueueSelectionOutline();
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
        const Vector2F lViewportPx{ static_cast<float>(m_viewportSize.x), static_cast<float>(m_viewportSize.y) };

        // Open on the framing the editor had before cameras existed — that view was one world unit
        // per pixel, so half the panel's height is the equivalent OrthoSize. One-shot, and it
        // ignores the 1x1 reported before the first measured resize.
        m_Context.Camera.SeedFromViewportHeight(lViewportPx.y);

        // Zoom BEFORE pan: the wheel is anchored at the cursor, so it must not be applied to a
        // position the pan has already moved out from under the pointer.
        if (m_PendingZoom != 0.f)
        {
            m_Context.Camera.ZoomAtCursor(m_PendingZoom, m_PendingZoomCursorPx, lViewportPx);
            m_PendingZoom = 0.f;
        }

        if (m_PendingPanPx.x != 0.f || m_PendingPanPx.y != 0.f)
        {
            m_Context.Camera.Pan(m_PendingPanPx, lViewportPx);
            m_PendingPanPx = { 0.f, 0.f };
        }

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
    // EnqueueSelectionOutline — mark the selected entity in the world itself, not just as a
    // highlighted Hierarchy row. Re-submitted every frame by design: DebugDraw is drained and
    // cleared by the renderer each frame, so "still selected" means "queue it again".
    //
    // DummyComponent is what carries position/size and the only thing RendererManager draws — the
    // same component the Inspector edits (M2b). A real TransformComponent is M3's work.
    // =========================================================================
    void ViewportPanel::EnqueueSelectionOutline()
    {
        // A local COPY of the handle (the InspectorPanel idiom) — Entity is a value type, and TryGet
        // is non-const.
        Entity lSelected = m_Context.Selection.Get();
        if (!lSelected.IsValid())
        {
            return;
        }

        const TransformComponent* lXf   = lSelected.TryGet<TransformComponent>();
        const DummyComponent*     lComp = lSelected.TryGet<DummyComponent>();
        if (lXf == nullptr || lComp == nullptr)
        {
            return;
        }

        m_Context.Engine.GetDebugDraw().DrawBox(lXf->Position, lComp->Size + m_OutlinePadding,
                                                m_OutlineColor, m_OutlineThickness);

        // Log the SUCCESS branch (L15): both returns above are silent, so "selected an entity with no
        // DummyComponent" would otherwise be indistinguishable from a broken DebugDraw pipe. One-shot
        // — this runs every frame.
        if (!m_bOutlineLogged)
        {
            OPAAX_LOG(LogViewportPanel, Info, "Selection outline enqueued (4 debug lines around {},{})",
                      lXf->Position.x, lXf->Position.y);
            m_bOutlineLogged = true;
        }
    }

    // =========================================================================
    // MeasureCameraGesture — read the pan drag and the wheel while this window is current, and
    // bank them for OnPreRender. Call it right after the image, so GetItemRect* names the image.
    //
    // ImGui IS the source here, not a workaround: an Edit world puts the input route in
    // ClosedEditMode, so InputManager is never fed and would report every button up forever (IN8).
    // The gate is this window's own hover — NEVER io.WantCaptureMouse, which is true the whole
    // time the pointer is over the viewport, because the viewport is an ImGui window (L29).
    // =========================================================================
    void ViewportPanel::MeasureCameraGesture(bool bInHovered)
    {
        const ImGuiIO& lIO = ImGui::GetIO();

        // The drag STARTS on the viewport and then belongs to the gesture: releasing is what ends
        // it, not leaving the panel. Dragging out of the window mid-pan is normal at the edges of
        // a level, and cutting it there would feel broken.
        if (bInHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
        {
            m_bPanning = true;
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            m_bPanning = false;
        }

        if (m_bPanning)
        {
            m_PendingPanPx.x += lIO.MouseDelta.x;
            m_PendingPanPx.y += lIO.MouseDelta.y;
        }

        // The wheel, unlike the drag, needs the pointer to be here — it is anchored at the cursor,
        // and a cursor somewhere else has no world point to anchor to.
        if (bInHovered && lIO.MouseWheel != 0.f)
        {
            const ImVec2 lOrigin = ImGui::GetItemRectMin();

            m_PendingZoom          = lIO.MouseWheel;
            m_PendingZoomCursorPx  = { lIO.MousePos.x - lOrigin.x, lIO.MousePos.y - lOrigin.y };
        }
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

        // D5 step 2's inputs. ImGui can only answer these while the window is current, so they are
        // measured HERE and pushed into the route, which reads them next frame — the same one-frame
        // lag the deferred resize above already lives with, and for the same reason.
        const bool lHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

        m_Context.Route.SetViewportFocus(lHovered, ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows));

        const EditorImage lImg = GetViewportImage();

        // Draws a Dummy of the same size when the handle is null, which is the reserve-space branch
        // this used to spell out below.
        ImguiWidgets::Image(lImg, lAvail);

        MeasureCameraGesture(lHovered);

        if (lImg.IsValid() && !m_bImageLogged)
        {
            OPAAX_LOG(LogViewportPanel, Info, "Viewport displaying world FBO (handle={}, {}x{})", lImg.Handle, m_viewportSize.x, m_viewportSize.y);
            m_bImageLogged = true;
        }
    }

    void ViewportPanel::Shutdown()
    {
        // Clear the engine's primary target FIRST (while the engine is alive — LC/TearDown), so no live
        // frame reads a dangling target, THEN free the FBO (GL context still current). Order matters.
        m_Context.Engine.SetPrimaryRenderTarget(nullptr);
        m_RenderTarget.reset();
        m_Framebuffer.reset();

        OPAAX_LOG(LogViewportPanel, Info, "ViewportPanel shutdown");
    }
}
