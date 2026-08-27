#include "Editor/Panels/ViewportPanel.h"

#include "Editor/Camera/EditorCamera.h"     // the Edit viewpoint this panel drives (①)
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Input/InputRoute.h"        // hover/focus is pushed, not read back out (D5 step 2)
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EditorViewport.hpp"
#include "Editor/UI/IEditorUIBackend.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory

#include "Renderer/DebugDraw.h"             // selection outline (M2c)
#include "Renderer/RenderTarget.hpp"        // OffscreenRenderTarget
#include "RHI/Framebuffer.h"                // IFramebuffer + FramebufferSpec (created by the device)

#include "Core/Maths/Bounds2D.h"
#include "Renderer/CameraView.h"            // ScreenToWorld — the one pixel->world rule (CAM2)

#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"        // the complete all-entities view, for the icon pass
#include "World/Entity/EntityQuery.h"       // the ONE entity-AABB rule — outline, pick and marquee
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

        // FIRST, deliberately. The click being spent was made against the frame RENDERED last time,
        // so it has to be hit-tested against that frame's viewport size and camera — both of which
        // the two calls below are about to change.
        ApplyPendingPick();

        ApplyPendingResize();
        ApplyCameraGesture();
        EnqueueSelectionOutline();
        EnqueueEntityIcons();
    }

    // =========================================================================
    // ViewportToWorld / AnchorHalfExtent — the two conversions everything selection-related needs.
    //
    // Both read the ACTIVE WORLD's view rather than the editor camera, which is what keeps picking
    // free of an Edit/Play fork: a PIE clone is framed by its CameraComponent, and asking the world
    // how it is framed gets the right answer in either mode with no branch.
    // =========================================================================
    Vector2F ViewportPanel::ViewportToWorld(const Vector2F& InLocalPx) const
    {
        World* const   lWorld = m_Context.Worlds.GetActiveWorld();
        const Vector2F lViewportPx{ static_cast<float>(m_viewportSize.x), static_cast<float>(m_viewportSize.y) };

        return ScreenToWorld(lWorld != nullptr ? lWorld->GetCameraView() : CameraView{}, lViewportPx, InLocalPx);
    }

    float ViewportPanel::AnchorHalfExtent() const
    {
        World* const lWorld = m_Context.Worlds.GetActiveWorld();
        if (lWorld == nullptr || m_viewportSize.y == 0)
        {
            return m_IconHalfPx;
        }

        // OrthoSize is the vertical HALF-extent, so one pixel is (2 * OrthoSize) / height world units.
        const float lWorldPerPixel = (lWorld->GetCameraView().OrthoSize * 2.f) / static_cast<float>(m_viewportSize.y);

        return m_IconHalfPx * lWorldPerPixel;
    }

    // =========================================================================
    // ApplyPendingPick — spend the banked click or marquee. A point asks EntityQuery for the
    // topmost entity; a box asks for everything it overlaps. Ctrl adds, otherwise it replaces —
    // and a plain click on empty space clears, which is how an author drops a selection.
    // =========================================================================
    void ViewportPanel::ApplyPendingPick()
    {
        const EPendingPick lPending = m_PendingPick;
        m_PendingPick = EPendingPick::None;   // cleared FIRST: a refused pick must not retry next frame

        if (lPending == EPendingPick::None)
        {
            return;
        }

        World* lWorld = m_Context.Worlds.GetActiveWorld();
        if (lWorld == nullptr)
        {
            return;
        }

        const float lAnchor = AnchorHalfExtent();

        if (lPending == EPendingPick::Point)
        {
            Entity lHit = EntityQuery::PickAt(*lWorld, ViewportToWorld(m_PickStartPx), lAnchor);

            if (m_bPickAdditive) { m_Context.Selection.Toggle(lHit); }   // ignores a miss
            else if (lHit.IsValid()) { m_Context.Selection.Select(lHit); }
            else { m_Context.Selection.Clear(); }

            OPAAX_LOG(LogViewportPanel, Info, "Viewport click at ({:.1f}, {:.1f}) -> {} ({} selected)",
                      m_PickStartPx.x, m_PickStartPx.y,
                      lHit.IsValid() ? lHit.Get<EntityMeta>().Name.CStr() : "nothing",
                      m_Context.Selection.Count());
            return;
        }

        const Bounds2D lRegion = Bounds2D::FromMinMax(ViewportToWorld(m_PickStartPx),
                                                      ViewportToWorld(m_PickEndPx));

        TDynArray<EntityID> lHits;
        EntityQuery::QueryOverlapping(*lWorld, lRegion, lHits, lAnchor);

        if (m_bPickAdditive)
        {
            for (const EntityID lId : lHits) { m_Context.Selection.Add(Entity{ lId, lWorld }); }
        }
        else
        {
            m_Context.Selection.Replace(lWorld, lHits);
        }

        OPAAX_LOG(LogViewportPanel, Info, "Viewport marquee took {} entity(ies) ({} selected)",
                  static_cast<Uint64>(lHits.size()), m_Context.Selection.Count());
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
        World* const lWorld = m_Context.Selection.GetWorld();
        if (lWorld == nullptr)
        {
            return;
        }

        // Through EntityQuery, which is what fixed the bug this used to have: it read DummyComponent
        // directly, so a SPRITE-only entity outlined nothing at all. Every selected entity now, and
        // an anchor-only one gets its icon-sized box.
        const float lAnchor = AnchorHalfExtent();
        Uint64      lDrawn  = 0;

        for (const EntityID lId : m_Context.Selection.Ids())
        {
            Bounds2D lBounds;
            if (!EntityQuery::TryGetBounds(Entity{ lId, lWorld }, lBounds, lAnchor))
            {
                continue;
            }

            m_Context.Engine.GetDebugDraw().DrawBox(lBounds.Center, lBounds.Size() + m_OutlinePadding,
                                                    m_OutlineColor, m_OutlineThickness);
            ++lDrawn;
        }

        // Log the SUCCESS branch (L15): every return above is silent, so "selected something with no
        // bounds" would otherwise look exactly like a broken DebugDraw pipe. One-shot — per frame.
        if (!m_bOutlineLogged && lDrawn > 0)
        {
            OPAAX_LOG(LogViewportPanel, Info, "Selection outline enqueued for {} entity(ies)", lDrawn);
            m_bOutlineLogged = true;
        }
    }

    // =========================================================================
    // EnqueueEntityIcons — an entity that draws nothing still has to be findable and clickable.
    // Every engine solves this the same way (Unreal's editor billboard, Unity's gizmo icon, Godot's
    // origin grab-area) and all of them hang it off a transform every object is guaranteed to have,
    // which is exactly what TransformComponent now is.
    //
    // The anchor size is the SAME value EntityQuery::PickAt is given, so the box drawn is the box
    // hit-tested — what you see is what you click, by construction rather than by two constants
    // being kept in step.
    // =========================================================================
    void ViewportPanel::EnqueueEntityIcons()
    {
        World* lWorld = m_Context.Worlds.GetActiveWorld();

        // Edit worlds only: an overlay is authoring furniture, and a running game must look like the
        // game. Same rule EditorCamera::Apply states for the view.
        if (lWorld == nullptr || lWorld->GetMode() != EWorldMode::Edit)
        {
            return;
        }

        const float lAnchor = AnchorHalfExtent();
        Uint64      lDrawn  = 0;

        lWorld->Each<EntityMeta>([&](EntityID InId, const EntityMeta&)
        {
            // An entity with an EXTENT is already visible — asking without an anchor is what
            // distinguishes the two, and it is one call rather than a list of component checks.
            Bounds2D lUnused;
            if (EntityQuery::TryGetBounds(Entity{ InId, lWorld }, lUnused))
            {
                return;
            }

            Bounds2D lIcon;
            if (!EntityQuery::TryGetBounds(Entity{ InId, lWorld }, lIcon, lAnchor))
            {
                return;   // no transform at all — not reachable through CreateEntity
            }

            m_Context.Engine.GetDebugDraw().DrawBox(lIcon.Center, lIcon.Size(), m_IconColor, m_IconThickness);
            ++lDrawn;
        });

        if (!m_bIconsLogged && lDrawn > 0)
        {
            OPAAX_LOG(LogViewportPanel, Info, "Drawing {} entity icon(s) — entities with nothing to render",
                      lDrawn);
            m_bIconsLogged = true;
        }
    }

    // =========================================================================
    // MeasureCameraGesture — read the pan drag and the wheel while this window is current, and
    // bank them for OnPreRender. Call it right after the image, so GetItemRect* names the image.
    //
    // ImGui IS the source here, not a workaround: an Edit world puts the input route in
    // ClosedEditMode, so InputManager is never fed and would report every button up forever (IN8).
    // The gate is the IMAGE's hover — never io.WantCaptureMouse, which is true the whole time the
    // pointer is over the viewport because the viewport is an ImGui window (L29), and never the
    // WINDOW's, which includes the title bar.
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

    // =========================================================================
    // MeasureViewportInput — ONE left-button gesture with two outcomes. The press banks a point;
    // crossing ImGui's own MouseDragThreshold promotes it to a box. Using ImGui's threshold rather
    // than a constant of my own is what keeps a click here feeling like a click everywhere else.
    //
    // m_bSelecting is the gate that matters: it is set only by a press that landed ON the image, so
    // a drag begun over the Hierarchy and released here selects nothing. Once it IS ours the drag
    // survives leaving the panel, exactly as the middle-button pan does — releasing ends a gesture,
    // wandering off does not.
    // =========================================================================
    void ViewportPanel::MeasureViewportInput(bool bInHovered, const Vector2F& InOrigin)
    {
        const ImGuiIO& lIO = ImGui::GetIO();

        if (bInHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_bSelecting    = true;
            m_bWasDrag      = false;
            m_bPickAdditive = lIO.KeyCtrl;
            m_PickStartPx   = { lIO.MousePos.x - InOrigin.x, lIO.MousePos.y - InOrigin.y };
        }

        if (!m_bSelecting)
        {
            return;
        }

        // REMEMBERED, never asked for after the fact: IsMouseDragging requires the button to still
        // be DOWN, so on the release frame it is false and every drag would bank as a click at the
        // pixel the drag STARTED from — which selects whatever is under the drag's origin, or
        // clears when that is empty space.
        m_PickEndPx = { lIO.MousePos.x - InOrigin.x, lIO.MousePos.y - InOrigin.y };

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, lIO.MouseDragThreshold))
            {
                m_bWasDrag = true;
                // PAINTED HERE, in screen pixels, on the foreground list. A marquee is UI, not world
                // geometry — DebugDraw would put it a frame behind and make it scale with the zoom.
                const ImVec2 lFrom{ InOrigin.x + m_PickStartPx.x, InOrigin.y + m_PickStartPx.y };
                const ImVec2 lTo = lIO.MousePos;

                const ImU32 lLine = ImGui::GetColorU32(ImVec4{m_MarqueeColor.r, m_MarqueeColor.g,
                                                              m_MarqueeColor.b, m_MarqueeColor.a});
                const ImU32 lFill = ImGui::GetColorU32(ImVec4{m_MarqueeColor.r, m_MarqueeColor.g,
                                                              m_MarqueeColor.b, m_MarqueeFillAlpha});

                ImDrawList* lDraw = ImGui::GetForegroundDrawList();
                lDraw->AddRectFilled(lFrom, lTo, lFill);
                lDraw->AddRect(lFrom, lTo, lLine);
            }

            return;   // still held — nothing to spend yet
        }

        // Released: bank exactly one outcome for OnPreRender.
        m_PendingPick = m_bWasDrag ? EPendingPick::Box : EPendingPick::Point;
        m_bSelecting  = false;
        m_bWasDrag    = false;
    }

    // =========================================================================
    // MeasureViewportKeys — F and Delete, dispatched BY TAG so a key and its menu entry reach one
    // verb rather than two copies of it (the reserved F-keys' rule, one layer up).
    //
    // Gated on THIS PANEL having the pointer or focus, and deliberately NOT put in
    // EditorService::HandleAuthoringShortcuts beside Ctrl+S: that uses ImGuiInputFlags_RouteGlobal,
    // and an unmodified F on a global route would fire while a text field elsewhere owns the
    // keyboard. A bare key needs a surface to belong to, and this is it.
    //
    // Executed immediately rather than banked: a command mutates the WORLD, not this panel's draw
    // state, and nothing below in this pass reads the entities it may destroy — the row walk that
    // made the Hierarchy defer is in another panel.
    // =========================================================================
    void ViewportPanel::MeasureViewportKeys(bool bInHovered)
    {
        if (!bInHovered && !ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
        {
            return;
        }

        // A text field anywhere takes precedence — otherwise renaming an entity to "Fred" would
        // frame the selection and delete it.
        if (ImGui::GetIO().WantCaptureKeyboard)
        {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F, /*repeat*/false))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_FOCUS_SELECTED, m_Context);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, /*repeat*/false))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_DELETE_ENTITY, m_Context);
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
        // Immediately after the image because GetItemRect*/IsItemHovered name the LAST submitted
        // item, and MeasureCameraGesture reads that rect again for the zoom anchor.
        const bool   lImageHovered = ImGui::IsItemHovered();
        const ImVec2 lOrigin       = ImGui::GetItemRectMin();

        MeasureCameraGesture(lImageHovered);
        MeasureViewportInput(lImageHovered, { lOrigin.x, lOrigin.y });
        MeasureViewportKeys(lHovered);

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
