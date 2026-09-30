#include "Editor/Viewport/ViewportGizmo.h"

#include "Editor/ImguiLibrary/ImguiCursor.h"   // infinite drag (wraps the cursor at the edge)
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Viewport/ViewportOverlays.h"   // AnchorHalfExtent (the icon's size is the hit test's)

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/Maths.h"                  // DegreesToRadians
#include "Renderer/CameraView.h"               // MakeView / MakeProjection
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityQuery.h"
#include "World/World.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>   // value_ptr

namespace Opaax::Editor
{
    namespace
    {
        /**
         * The 2D subset of ImGuizmo's operations (Z masked off: there is no third axis).
         */
        ImGuizmo::OPERATION ToGizmoOperation(const EGizmoMode InMode) noexcept
        {
            switch (InMode)
            {
            case EGizmoMode::Translate: return ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y;
            case EGizmoMode::Rotate:    return ImGuizmo::ROTATE_Z;   // face-on in an ortho view
            case EGizmoMode::Scale:     return ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y;
            }

            return ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y;
        }
    }

    bool TryGetGizmoPose(const EditorGizmo& InSettings, World& InWorld, const EditorSelection& InSelection,
                         const float InAnchorHalfExtent, Vector2F& OutPivot, float& OutRotationRad)
    {
        // Edit worlds only: a running game must look like the game.
        if (InWorld.GetMode() != EWorldMode::Edit || !InSelection.HasSelection())
        {
            return false;
        }

        Entity lPrimary = InSelection.Get();

        // Both come from the primary entity, so the handles point and turn about the same entity. Uses its
        // world pose (where it draws).
        const bool               lHasPrimary = lPrimary.IsValid() && lPrimary.TryGet<TransformComponent>() != nullptr;
        const TransformComponent lPrimaryXf  = lHasPrimary ? EntityHierarchy::WorldTransform(lPrimary)
                                                           : TransformComponent{};

        OutRotationRad = (InSettings.GetEffectiveSpace() == EGizmoSpace::Local && lHasPrimary)
                             ? Maths::DegreesToRadians(lPrimaryXf.Rotation)
                             : 0.f;

        if (InSettings.GetPivot() == EGizmoPivot::Origin && lHasPrimary)
        {
            OutPivot = lPrimaryXf.Position;
            return true;
        }

        Bounds2D lBounds;
        if (!EntityQuery::TryGetBounds(InWorld, InSelection.Ids(), lBounds, InAnchorHalfExtent))
        {
            return false;
        }

        OutPivot = lBounds.Center;
        return true;
    }

    // =========================================================================
    // Measure — ImGuizmo draws the handles and edits the matrix in one call, so this runs in the ImGui
    // pass. It must not write the world here: the delta is stored and the caller applies it next frame.
    // The matrix belongs to GizmoDrag and only follows the selection while no drag is live.
    // =========================================================================
    bool GizmoGesture::Measure(const EditorGizmo& InSettings, World& InWorld, const EditorSelection& InSelection,
                               const CameraView& InView, const Vector2F& InViewportPx, const Vector2F& InOrigin,
                               const Vector2F& InSizePx, const float InTranslateSnapStep, const bool bInSuppress,
                               const EUndoWorld InScope)
    {
        Vector2F lPivot;
        float    lRotationRad = 0.f;

        // The anchor keeps a transform-only entity's gizmo where its icon is.
        const float lAnchor = ViewportOverlays::AnchorHalfExtent(InView, InViewportPx);

        if (InSelection.GetWorld() != &InWorld
            || !TryGetGizmoPose(InSettings, InWorld, InSelection, lAnchor, lPivot, lRotationRad)
            || InSizePx.x <= 0.f || InSizePx.y <= 0.f)
        {
            return false;
        }

        // This gizmo's ID for the rest of the call (IsUsing/IsOver answer for it alone).
        ImGuizmo::PushID(this);

        // Infinite drag: the cursor wraps back into the image when it leaves. ImGuizmo reads the absolute
        // io.MousePos, so the accumulated offset is added back below. Reset when idle.
        if (ImGuizmo::IsUsing())
        {
            const Vector2F lCorrection = ImguiCursor::WrapInRect(ImVec2{ InOrigin.x, InOrigin.y },
                                                                 ImVec2{ InOrigin.x + InSizePx.x, InOrigin.y + InSizePx.y });
            m_WrapOffset += lCorrection;

            if (!m_bWrapLogged && (lCorrection.x != 0.f || lCorrection.y != 0.f))
            {
                OPAAX_LOG(LogViewportGizmo, Info, "Cursor wrapped at the image edge during a gizmo drag — the gesture continues");
                m_bWrapLogged = true;
            }
        }
        else
        {
            m_WrapOffset = { 0.f, 0.f };
        }

        ImGuizmo::SetOrthographic(true);
        ImGuizmo::SetDrawlist();   // this panel's list, so the gizmo clips to the image
        ImGuizmo::SetRect(InOrigin.x, InOrigin.y, InSizePx.x, InSizePx.y);

        // View and projection separately (ImGuizmo takes them apart).
        const Matrix44F lViewMatrix = MakeView(InView);
        const Matrix44F lProjMatrix = MakeProjection(InView, static_cast<Uint32>(InViewportPx.x),
                                                     static_cast<Uint32>(InViewportPx.y));

        if (!ImGuizmo::IsUsing())
        {
            m_Drag.ReseatAt(lPivot, lRotationRad);
        }

        // Translate uses the caller's step (the grid when shown); rotate and scale use the set step.
        const float lStep    = InSettings.GetMode() == EGizmoMode::Translate ? InTranslateSnapStep
                                                                             : InSettings.GetSnapStep();
        const float lSnap[3] = { lStep, lStep, lStep };

        // A handle under the toolbar must not be grabbable through it, but never disable mid-drag:
        // Enable(false) cancels the drag. Disabled still draws.
        const bool bSuppress = bInSuppress && !ImGuizmo::IsUsing();
        ImGuizmo::Enable(!bSuppress);

        // The virtual cursor, only for the Manipulate call; restored right after.
        ImGuiIO&     lIO        = ImGui::GetIO();
        const ImVec2 lRealMouse = lIO.MousePos;

        lIO.MousePos = ImVec2{ lRealMouse.x + m_WrapOffset.x, lRealMouse.y + m_WrapOffset.y };

        // No deltaMatrix out-parameter: ImGuizmo's is cumulative and origin-centred for scale (see
        // GizmoDrag::BankFrameDelta). Manipulate returns whether the matrix changed, so a click without
        // motion does not store an identity delta.
        const bool bChanged = ImGuizmo::Manipulate(glm::value_ptr(lViewMatrix), glm::value_ptr(lProjMatrix),
                                                   ToGizmoOperation(InSettings.GetMode()),
                                                   InSettings.GetEffectiveSpace() == EGizmoSpace::Local
                                                       ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
                                                   glm::value_ptr(m_Drag.Matrix()), nullptr,
                                                   InSettings.IsSnappingNow() ? lSnap : nullptr);

        lIO.MousePos = lRealMouse;
        ImGuizmo::Enable(true);   // restored right away (the flag is global)

        if (bChanged)
        {
            m_Drag.BankFrameDelta();
        }

        // Drag start: open the undo step now, while the transforms are still the pre-drag ones. Named
        // after the mode ("Undo Rotate").
        const bool lUsing = ImGuizmo::IsUsing();

        if (lUsing && !m_bWasUsing)
        {
            m_Step.Begin(InWorld, InSelection.Ids(), ToString(InSettings.GetMode()), InScope);
        }

        m_bWasUsing = lUsing;
        m_bMeasured = true;   // lets Close notice a panel that stops drawing

        // IsOver as well as IsUsing: hovering a handle suppresses the marquee.
        const bool lOwnsMouse = ImGuizmo::IsUsing() || ImGuizmo::IsOver();

        ImGuizmo::PopID();

        return lOwnsMouse;
    }

    bool GizmoGesture::TakeDelta(const EditorGizmo& InSettings, EntityOps::TransformDelta& OutDelta)
    {
        if (!m_Drag.HasPendingDelta())
        {
            return false;
        }

        OutDelta.Matrix   = m_Drag.ConsumeDelta();
        OutDelta.FrameRad = m_Drag.GetFrameRad();
        OutDelta.Origin   = InSettings.UsesIndividualOrigins() ? EntityOps::ETransformOrigin::Individual
                                                               : EntityOps::ETransformOrigin::Shared;

        // Logged once per mode (a drag produces one per frame).
        const EGizmoMode lMode = InSettings.GetMode();
        const Uint8      lBit  = static_cast<Uint8>(1u << static_cast<Uint8>(lMode));

        if ((m_LoggedModes & lBit) == 0)
        {
            OPAAX_LOG(LogViewportGizmo, Info, "Gizmo {} delta taken for {} entity(ies)",
                      ToString(lMode), static_cast<Uint64>(m_Step.Entries.size()));
            m_LoggedModes |= lBit;
        }

        return true;
    }

    // =========================================================================
    // Close — drag end, taken after the last stored delta was applied (a delta measured in frame N is
    // applied in frame N+1). m_bMeasured catches a panel that stopped drawing, so a missed end does
    // not merge the next edit in.
    // =========================================================================
    void GizmoGesture::Close(const EditorContext& InContext, EditorUndo& InStack)
    {
        const bool lStillDragging = m_bWasUsing && m_bMeasured;

        if (!lStillDragging && !m_Drag.HasPendingDelta())
        {
            // The whole drag as one step. End returns false when no step is open or nothing moved, so this is
            // safe on idle frames.
            if (m_Step.End(InContext))
            {
                InStack.Record(Move(m_Step));
            }

            // Closed either way, or a later Inspector edit would be recorded as a phantom "Move".
            m_Step = EntityTransform{};

            m_bWasUsing = false;
        }

        m_bMeasured = false;   // set again by the next Measure
    }
}
