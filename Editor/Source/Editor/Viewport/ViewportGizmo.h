#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Operation/EditorGizmo.hpp"    // GizmoDrag
#include "Editor/Operation/EntityOps.h"        // TransformDelta
#include "Editor/Undo/EntityUndoables.h"       // EntityTransform
#include "Editor/Undo/UndoWorld.h"

namespace Opaax
{
    class World;
    struct CameraView;

    OPAAX_LOG_CATEGORY(ViewportGizmo);
}

namespace Opaax::Editor
{
    class EditorSelection;
    class EditorUndo;
    struct EditorContext;

    // =============================================================================
    // GizmoGesture — the transform gizmo on one surface. Drawn and manipulated by ImGuizmo in the
    //   ImGui pass; the delta is stored, applied by the caller in OnPreRender, and the whole drag is
    //   recorded as one undo step on the caller's stack.
    //   Settings (mode, pivot, space, snap) come from the context's EditorGizmo; the drag state is
    //   per surface. Every ImGuizmo call is scoped by PushID(this), so only the grabbed gizmo moves.
    // =============================================================================
    class GizmoGesture
    {
        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /**
         * Draws the handles and manipulates, storing what the drag produced. Call while the panel's window
         * is current, after the image. Opens the undo step on grab.
         * @param InTranslateSnapStep Snap step for translate (grid spacing when shown)
         * @param bInSuppress Do not manipulate this frame (toolbar under the cursor); ignored mid-drag
         * @return True while the gizmo owns the mouse (the caller then skips its pick gesture)
         */
        bool Measure(const EditorGizmo& InSettings, World& InWorld, const EditorSelection& InSelection,
                     const CameraView& InView, const Vector2F& InViewportPx, const Vector2F& InOrigin,
                     const Vector2F& InSizePx, float InTranslateSnapStep, bool bInSuppress,
                     EUndoWorld InScope);

        /**
         * The stored delta, cleared. The level applies it through the command; the prefab panel through
         * EntityOps::TransformEntities.
         * @return False when nothing was stored
         */
        bool TakeDelta(const EditorGizmo& InSettings, EntityOps::TransformDelta& OutDelta);

        /**
         * Drag end, after the last delta was applied: closes the step and records it on InStack if
         * something moved. Call every OnPreRender (does nothing on idle frames).
         */
        void Close(const EditorContext& InContext, EditorUndo& InStack);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        GizmoDrag m_Drag;

        // Infinite drag: how far the cursor was wrapped back during the current drag, in screen pixels.
        // Added to io.MousePos during Manipulate. Reset when no drag is live.
        Vector2F m_WrapOffset = { 0.f, 0.f };

        // Drag edges. WasUsing is ImGuizmo's grab state last pass; Measured says that pass ran, so a panel
        // that stops drawing cannot leave the step open.
        bool m_bWasUsing = false;
        bool m_bMeasured = false;

        // The drag's undo step, kept across frames: opened at grab, closed at release.
        EntityTransform m_Step;

        // One bit per EGizmoMode: logs once per mode.
        Uint8 m_LoggedModes = 0;
        bool  m_bWrapLogged = false;
    };

    /**
     * Where the gizmo sits and how it is rotated for InSelection in InWorld. The pivot is the
     * selection's bounds centre or the primary's position (per EGizmoPivot); the rotation is the
     * primary's in Local space, zero otherwise.
     * @return False when there is nothing to show (no selection, no bounds, or a Play world)
     */
    bool TryGetGizmoPose(const EditorGizmo& InSettings, World& InWorld, const EditorSelection& InSelection,
                         float InAnchorHalfExtent, Vector2F& OutPivot, float& OutRotationRad);
}
