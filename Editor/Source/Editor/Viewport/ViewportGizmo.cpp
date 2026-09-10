#include "Editor/Viewport/ViewportGizmo.h"

#include "Editor/ImguiLibrary/ImguiCursor.h"   // the infinite drag — wrap the cursor at the edge
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Viewport/ViewportOverlays.h"   // AnchorHalfExtent — the icon's size is the hit test's (SEL4)

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/Maths.h"                  // DegreesToRadians — the transform authors degrees
#include "Renderer/CameraView.h"               // MakeView / MakeProjection — ImGuizmo takes them separately (GIZ2)
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityQuery.h"
#include "World/World.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>   // value_ptr — ImGuizmo takes raw float[16]

namespace Opaax::Editor
{
    namespace
    {
        /**
         * The 2D subset of ImGuizmo's operations. Z is masked off on every one of them: this engine
         * has no third axis to author, and an unmasked gizmo would offer handles that write a field
         * no component reads.
         */
        ImGuizmo::OPERATION ToGizmoOperation(const EGizmoMode InMode) noexcept
        {
            switch (InMode)
            {
            case EGizmoMode::Translate: return ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y;
            case EGizmoMode::Rotate:    return ImGuizmo::ROTATE_Z;   // face-on under an ortho view
            case EGizmoMode::Scale:     return ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y;
            }

            return ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y;
        }
    }

    bool TryGetGizmoPose(const EditorGizmo& InSettings, World& InWorld, const EditorSelection& InSelection,
                         const float InAnchorHalfExtent, Vector2F& OutPivot, float& OutRotationRad)
    {
        // Edit worlds only: an overlay is authoring furniture, and a running game must look like
        // the game.
        if (InWorld.GetMode() != EWorldMode::Edit || !InSelection.HasSelection())
        {
            return false;
        }

        Entity lPrimary = InSelection.Get();

        // BOTH answers come from the PRIMARY, which is why they are one query: the entity whose
        // axes Local follows must be the entity Origin sits on, or the handles would point one way
        // and turn about another.
        const TransformComponent* lPrimaryXf = lPrimary.IsValid() ? lPrimary.TryGet<TransformComponent>()
                                                                  : nullptr;

        OutRotationRad = (InSettings.GetEffectiveSpace() == EGizmoSpace::Local && lPrimaryXf != nullptr)
                             ? Maths::DegreesToRadians(lPrimaryXf->Rotation)
                             : 0.f;

        if (InSettings.GetPivot() == EGizmoPivot::Origin && lPrimaryXf != nullptr)
        {
            OutPivot = lPrimaryXf->Position;
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
    // Measure — ImGuizmo both draws the handles and manipulates the matrix, in one call.
    //
    // That is why this lives in the ImGui pass while every other overlay is enqueued in OnPreRender:
    // it needs the draw list and the mouse. What it must NOT do from here is write the world (MP7),
    // so the delta is banked and the caller spends it next frame — the same measure-then-apply
    // handshake the camera gesture and the pick already use.
    //
    // The matrix is GizmoDrag's, not a local: ImGuizmo captures its start pose when a drag begins
    // and then drives the matrix it was handed, so re-seating it mid-drag would fight that state.
    // It follows the selection only while nothing is being dragged.
    // =========================================================================
    bool GizmoGesture::Measure(const EditorGizmo& InSettings, World& InWorld, const EditorSelection& InSelection,
                               const CameraView& InView, const Vector2F& InViewportPx, const Vector2F& InOrigin,
                               const Vector2F& InSizePx, const float InTranslateSnapStep, const bool bInSuppress,
                               const EUndoWorld InScope)
    {
        Vector2F lPivot;
        float    lRotationRad = 0.f;

        // The anchor keeps a transform-only entity's gizmo where its icon is — SEL4's one value.
        const float lAnchor = ViewportOverlays::AnchorHalfExtent(InView, InViewportPx);

        if (InSelection.GetWorld() != &InWorld
            || !TryGetGizmoPose(InSettings, InWorld, InSelection, lAnchor, lPivot, lRotationRad)
            || InSizePx.x <= 0.f || InSizePx.y <= 0.f)
        {
            return false;
        }

        // THIS gizmo's identity for the rest of the call — IsUsing/IsOver answer for it alone.
        ImGuizmo::PushID(this);

        // INFINITE DRAG. Wrap the cursor back into the image once it leaves, so a drag never runs
        // out of screen — Blender, Unreal and Unity all do this for exactly this gesture.
        //
        // ImGuizmo reads the ABSOLUTE io.MousePos, so the teleport alone would fling the selection
        // to the far side. The accumulated correction is added back below, which is what makes the
        // gizmo see a cursor that walked off the edge and kept walking. Reset when idle so the
        // offset cannot leak into the next drag.
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
        ImGuizmo::SetDrawlist();   // this panel's list, so the gizmo clips to the viewport image
        ImGuizmo::SetRect(InOrigin.x, InOrigin.y, InSizePx.x, InSizePx.y);

        // The view and the projection SEPARATELY — the reason ③ split them out of
        // MakeViewProjection, which is still their product so the three cannot drift.
        const Matrix44F lViewMatrix = MakeView(InView);
        const Matrix44F lProjMatrix = MakeProjection(InView, static_cast<Uint32>(InViewportPx.x),
                                                     static_cast<Uint32>(InViewportPx.y));

        if (!ImGuizmo::IsUsing())
        {
            m_Drag.ReseatAt(lPivot, lRotationRad);
        }

        // Translate follows the caller's step (the grid when one is shown); rotate and scale have
        // no grid to match, so they use the authored step.
        const float lStep    = InSettings.GetMode() == EGizmoMode::Translate ? InTranslateSnapStep
                                                                             : InSettings.GetSnapStep();
        const float lSnap[3] = { lStep, lStep, lStep };

        // A handle under the toolbar must not be grabbable through it — but NEVER mid-drag, because
        // Enable(false) CANCELS the interaction it is editing, which would drop a drag the moment
        // the cursor crossed the strip. Disabled still DRAWS, it only refuses to manipulate.
        const bool bSuppress = bInSuppress && !ImGuizmo::IsUsing();
        ImGuizmo::Enable(!bSuppress);

        // The VIRTUAL cursor, for the length of the Manipulate call only. Everything else in the
        // frame — ImGui's own hover, the marquee, the pan — wants the real one, so it is restored
        // immediately rather than left shifted.
        ImGuiIO&     lIO        = ImGui::GetIO();
        const ImVec2 lRealMouse = lIO.MousePos;

        lIO.MousePos = ImVec2{ lRealMouse.x + m_WrapOffset.x, lRealMouse.y + m_WrapOffset.y };

        // NO deltaMatrix out-parameter, deliberately — see GizmoDrag::BankFrameDelta. ImGuizmo's is
        // per-frame for translate and rotate but cumulative-and-origin-centred for SCALE, which is
        // invisible at (0,0) and wrong everywhere else. The delta is taken from our own matrix.
        //
        // Manipulate returns whether it actually CHANGED the matrix, which is the guard that keeps a
        // click-without-motion from dirtying the map — IsUsing() alone stays true for the whole
        // gesture and would bank an identity delta every frame.
        const bool bChanged = ImGuizmo::Manipulate(glm::value_ptr(lViewMatrix), glm::value_ptr(lProjMatrix),
                                                   ToGizmoOperation(InSettings.GetMode()),
                                                   InSettings.GetEffectiveSpace() == EGizmoSpace::Local
                                                       ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
                                                   glm::value_ptr(m_Drag.Matrix()), nullptr,
                                                   InSettings.IsSnappingNow() ? lSnap : nullptr);

        lIO.MousePos = lRealMouse;
        ImGuizmo::Enable(true);   // restored immediately — the flag is global and persists otherwise

        if (bChanged)
        {
            m_Drag.BankFrameDelta();
        }

        // ⑤ — THE DRAG'S RISING EDGE, and the step opens HERE rather than at the first applied
        // delta because nothing has been written yet: the first delta banks this frame and lands
        // next, so the transforms read now are the pre-drag ones. The step is named by MODE, which
        // is what makes the menu read "Undo Rotate" rather than the verb's own "Transform".
        const bool lUsing = ImGuizmo::IsUsing();

        if (lUsing && !m_bWasUsing)
        {
            m_Step.Begin(InWorld, InSelection.Ids(), ToString(InSettings.GetMode()), InScope);
        }

        m_bWasUsing = lUsing;
        m_bMeasured = true;   // Close's guard against a panel that stops drawing

        // IsOver() as well as IsUsing(): hovering a handle must already suppress the marquee, or the
        // press that starts a drag also starts a rubber band underneath it.
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

        // ONE-SHOT PER MODE, because a drag lands one of these per frame. Without it a gizmo that
        // draws but never writes looks exactly like one that writes (L15).
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
    // Close — the drag's FALLING EDGE, taken AFTER the last banked delta was spent. Reading it at
    // the edge seen in the ImGui pass would see a world that predates the final motion, because a
    // delta measured in frame N is applied in frame N+1 (**SEL3**'s measure-then-apply lag).
    //
    // m_bMeasured is the other half: Measure stops being called when the panel is hidden or the
    // selection empties, and an edge never seen would merge the next unrelated edit in.
    // =========================================================================
    void GizmoGesture::Close(const EditorContext& InContext, EditorUndo& InStack)
    {
        const bool lStillDragging = m_bWasUsing && m_bMeasured;

        if (!lStillDragging && !m_Drag.HasPendingDelta())
        {
            // The whole drag, as ONE step. End answers false for a step nobody opened and for a
            // drag that ended where it started, so this runs on every idle frame and records
            // nothing — which is why it needs no flag of its own.
            if (m_Step.End(InContext))
            {
                InStack.Record(Move(m_Step));
            }

            // CLOSED EITHER WAY. A step left holding entries would keep re-reading them every idle
            // frame, and the next Inspector edit to one of those entities would land in the stack
            // as a phantom "Move".
            m_Step = EntityTransform{};

            m_bWasUsing = false;
        }

        m_bMeasured = false;   // set again by the next Measure
    }
}
