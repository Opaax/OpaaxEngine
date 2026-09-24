#include "Editor/Viewport/ViewportGestures.h"

#include "Editor/Camera/EditorCamera.h"
#include "Editor/ImguiLibrary/ImguiCursor.h"   // the infinite drag — wrap the cursor at the edge
#include "Editor/Operation/EditorSelection.hpp"

#include "Core/Maths/Bounds2D.h"
#include "Renderer/CameraView.h"               // ScreenToWorld (CAM2)
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Entity/EntityQuery.h"          // the ONE entity-AABB rule — pick and marquee
#include "World/World.h"

#include <imgui.h>

namespace Opaax::Editor
{
    namespace
    {
        constexpr Vector4F k_MarqueeColor     = { 1.f, 0.6f, 0.1f, 1.f };
        constexpr float    k_MarqueeFillAlpha = 0.12f;
    }

    // =========================================================================
    // CameraGesture
    // =========================================================================
    void CameraGesture::Measure(const bool bInHovered, const Vector2F& InOrigin, const Vector2F& InSizePx)
    {
        const ImGuiIO& lIO = ImGui::GetIO();

        // The drag STARTS on the image and then belongs to the gesture: releasing is what ends it,
        // not leaving the panel — panning to the edge of a level is exactly when you leave.
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
            // No correction to accumulate, unlike the gizmo: a pan reads MouseDelta, and the
            // teleport zeroes it on the frame it jumps. One frame of no motion, invisible.
            const Vector2F lWrapped = ImguiCursor::WrapInRect(ImVec2{ InOrigin.x, InOrigin.y },
                                                              ImVec2{ InOrigin.x + InSizePx.x, InOrigin.y + InSizePx.y });

            if (!m_bWrapLogged && (lWrapped.x != 0.f || lWrapped.y != 0.f))
            {
                OPAAX_LOG(LogViewportGestures, Info, "Cursor wrapped at the image edge during a pan — the gesture continues");
                m_bWrapLogged = true;
            }

            m_PendingPanPx.x += lIO.MouseDelta.x;
            m_PendingPanPx.y += lIO.MouseDelta.y;
        }

        // The wheel, unlike the drag, needs the pointer here — it is anchored at the cursor.
        if (bInHovered && lIO.MouseWheel != 0.f)
        {
            m_PendingZoom         = lIO.MouseWheel;
            m_PendingZoomCursorPx = { lIO.MousePos.x - InOrigin.x, lIO.MousePos.y - InOrigin.y };
        }
    }

    void CameraGesture::Spend(EditorCamera& InCamera, const Vector2F& InViewportPx)
    {
        InCamera.SeedFromViewportHeight(InViewportPx.y);

        if (m_PendingZoom != 0.f)
        {
            InCamera.ZoomAtCursor(m_PendingZoom, m_PendingZoomCursorPx, InViewportPx);
            m_PendingZoom = 0.f;
        }

        if (m_PendingPanPx.x != 0.f || m_PendingPanPx.y != 0.f)
        {
            InCamera.Pan(m_PendingPanPx, InViewportPx);
            m_PendingPanPx = { 0.f, 0.f };
        }
    }

    // =========================================================================
    // PickGesture
    // =========================================================================
    void PickGesture::Measure(const bool bInHovered, const Vector2F& InOrigin)
    {
        const ImGuiIO& lIO = ImGui::GetIO();

        if (bInHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_bSelecting         = true;
            m_bWasDrag           = false;
            m_Pending.bAdditive  = lIO.KeyCtrl;
            m_Pending.StartPx    = { lIO.MousePos.x - InOrigin.x, lIO.MousePos.y - InOrigin.y };
        }

        if (!m_bSelecting)
        {
            return;
        }

        m_Pending.EndPx = { lIO.MousePos.x - InOrigin.x, lIO.MousePos.y - InOrigin.y };

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, lIO.MouseDragThreshold))
            {
                m_bWasDrag = true;

                // PAINTED HERE, in screen pixels, on the foreground list. A marquee is UI, not world
                // geometry — DebugDraw would put it a frame behind and make it scale with the zoom.
                const ImVec2 lFrom{ InOrigin.x + m_Pending.StartPx.x, InOrigin.y + m_Pending.StartPx.y };
                const ImVec2 lTo = lIO.MousePos;

                const ImU32 lLine = ImGui::GetColorU32(ImVec4{ k_MarqueeColor.r, k_MarqueeColor.g,
                                                               k_MarqueeColor.b, k_MarqueeColor.a });
                const ImU32 lFill = ImGui::GetColorU32(ImVec4{ k_MarqueeColor.r, k_MarqueeColor.g,
                                                               k_MarqueeColor.b, k_MarqueeFillAlpha });

                ImDrawList* lDraw = ImGui::GetForegroundDrawList();
                lDraw->AddRectFilled(lFrom, lTo, lFill);
                lDraw->AddRect(lFrom, lTo, lLine);
            }

            return;   // still held — nothing to spend yet
        }

        // Released: bank exactly one outcome.
        m_Pending.Kind = m_bWasDrag ? EKind::Box : EKind::Point;
        m_bSelecting   = false;
        m_bWasDrag     = false;
    }

    PickGesture::Pick PickGesture::Take()
    {
        const Pick lPick = m_Pending;
        m_Pending.Kind = EKind::None;
        return lPick;
    }

    void PickGesture::Apply(const Pick& InPick, World& InWorld, EditorSelection& InSelection,
                            const Vector2F& InViewportPx, const float InAnchorHalfExtent)
    {
        if (InPick.Kind == EKind::None)
        {
            return;
        }

        const CameraView& lView = InWorld.GetCameraView();

        if (InPick.Kind == EKind::Point)
        {
            Entity lHit = EntityQuery::PickAt(InWorld, ScreenToWorld(lView, InViewportPx, InPick.StartPx),
                                              InAnchorHalfExtent);

            if (InPick.bAdditive) { InSelection.Toggle(lHit); }   // ignores a miss
            else if (lHit.IsValid()) { InSelection.Select(lHit); }
            else { InSelection.Clear(); }

            OPAAX_LOG(LogViewportGestures, Info, "Click at ({:.1f}, {:.1f}) in '{}' -> {} ({} selected)",
                      InPick.StartPx.x, InPick.StartPx.y, InWorld.GetName().CStr(),
                      lHit.IsValid() ? lHit.Get<EntityMeta>().Name.CStr() : "nothing",
                      InSelection.Count());
            return;
        }

        const Bounds2D lRegion = Bounds2D::FromMinMax(ScreenToWorld(lView, InViewportPx, InPick.StartPx),
                                                      ScreenToWorld(lView, InViewportPx, InPick.EndPx));

        TDynArray<EntityID> lHits;
        EntityQuery::QueryOverlapping(InWorld, lRegion, lHits, InAnchorHalfExtent);

        if (InPick.bAdditive)
        {
            for (const EntityID lId : lHits) { InSelection.Add(Entity{ lId, &InWorld }); }
        }
        else
        {
            InSelection.Replace(&InWorld, lHits);
        }

        OPAAX_LOG(LogViewportGestures, Info, "Marquee in '{}' took {} entity(ies) ({} selected)",
                  InWorld.GetName().CStr(), static_cast<Uint64>(lHits.size()), InSelection.Count());
    }
}
