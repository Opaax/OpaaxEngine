#pragma once

#include "Application/Services/ILogger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Operation/EditorGizmo.hpp"    // GizmoDrag — this surface's live drag
#include "Editor/Operation/EntityOps.h"        // TransformDelta — what the banked delta becomes
#include "Editor/Undo/EntityUndoables.h"       // EntityTransform — the ONE step a whole drag records
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
    // GizmoGesture — the transform gizmo on ONE surface: drawn and manipulated by ImGuizmo in the
    //   ImGui pass, its delta banked, spent by the caller in OnPreRender, and the whole drag
    //   recorded as one undo step on the stack the caller names (⑦-C P8 V3).
    //
    //   Owned by every panel that shows an editable world, beside its selection and camera — the
    //   ViewportPanel's ③/⑤ code moved here as a block so the prefab panel could not drift from it.
    //   The SETTINGS (mode, pivot, space, snap) stay on the context's EditorGizmo and are read; the
    //   DRAG (matrix, delta) is this object's own, because ImGuizmo drives the matrix it was handed
    //   and two surfaces reseating one would fight a live drag (**GIZ5**).
    //
    //   Every ImGuizmo call is scoped by PushID(this): IsUsing/IsOver answer for THIS gizmo, so two
    //   can be on screen and only the grabbed one moves.
    // =============================================================================
    class GizmoGesture
    {
        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /**
         * Draw the handles and manipulate, banking whatever the drag produced. Call while the
         * panel's window is current and after the image, since it takes the image rect as its
         * viewport. Opens the undo step on the grab.
         *
         * @param InTranslateSnapStep What a translate drag snaps to — the grid's spacing while one
         *   is shown, else the authored step; rotate and scale always use the authored one.
         * @param bInSuppress Refuse to manipulate this frame (a toolbar sits under the cursor) —
         *   never mid-drag, because Enable(false) CANCELS the interaction it is editing.
         * @return True while the gizmo owns the mouse, in which case the caller must not run its
         *   pick gesture — one button, two consumers, and the order is stated at the call site.
         */
        bool Measure(const EditorGizmo& InSettings, World& InWorld, const EditorSelection& InSelection,
                     const CameraView& InView, const Vector2F& InViewportPx, const Vector2F& InOrigin,
                     const Vector2F& InSizePx, float InTranslateSnapStep, bool bInSuppress,
                     EUndoWorld InScope);

        /**
         * The banked delta as the verb's input, cleared. The caller applies it through its own
         * route — the level dispatches the command that carries the PIE guard, the prefab panel
         * calls EntityOps::TransformEntities on its world (**PF12**).
         * @return False when nothing was banked.
         */
        bool TakeDelta(const EditorGizmo& InSettings, EntityOps::TransformDelta& OutDelta);

        /**
         * The drag's FALLING EDGE, taken AFTER TakeDelta's last delta has been spent: closes the
         * step and records it on InStack when something moved. Call every OnPreRender — it is a
         * no-op on idle frames and needs no flag of its own.
         */
        void Close(const EditorContext& InContext, EditorUndo& InStack);

        // =========================================================================
        // Members
        // =========================================================================
    private:
        GizmoDrag m_Drag;

        // Infinite drag: how far the cursor has been TELEPORTED back into the image during the
        // current drag, accumulated in screen pixels. Added to io.MousePos for the length of the
        // Manipulate call, because ImGuizmo reads the ABSOLUTE position and would otherwise see the
        // wrap as a leap across the viewport. Reset whenever no drag is live.
        Vector2F m_WrapOffset = { 0.f, 0.f };

        // ⑤ — the drag's two edges. WasUsing is ImGuizmo's grab state as of the last pass; Measured
        // says that pass happened at all, so a panel that stops drawing mid-drag cannot leave the
        // step open (see Close).
        bool m_bWasUsing = false;
        bool m_bMeasured = false;

        // ⑤ — the drag's undo step, held ACROSS FRAMES: opened with the transforms as they were at
        // the grab, closed with them as they are at the release. That is what makes a sixty-frame
        // drag one entry, with nothing in the stack having to know a drag happened.
        EntityTransform m_Step;

        // One bit per EGizmoMode, not one flag: "does the gizmo write?" is a separate question per
        // mode (L15 — the instrument has to discriminate).
        Uint8 m_LoggedModes = 0;
        bool  m_bWrapLogged = false;
    };

    /**
     * WHERE a gizmo sits and HOW it is turned for InSelection in InWorld — one query, because both
     * answers come from the PRIMARY entity and handles that pointed one way while turning about
     * another would be a lie.
     *
     * The pivot is the selection's combined bounds centre (through the same EntityQuery the outline
     * and focus use, **SEL1**) or the primary's own position, per EGizmoPivot. The rotation is the
     * primary's when the EFFECTIVE space is Local, and zero otherwise.
     *
     * @return False when there is nothing to draw a gizmo for — no selection, no bounds, or a PLAY
     *   world, which must look like the game rather than like the editor.
     */
    bool TryGetGizmoPose(const EditorGizmo& InSettings, World& InWorld, const EditorSelection& InSelection,
                         float InAnchorHalfExtent, Vector2F& OutPivot, float& OutRotationRad);
}
