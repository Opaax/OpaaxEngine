#pragma once

#include "Core/Maths/MathTypes.h"
#include "Editor/Operation/GizmoHandles.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorGizmo — the viewport transform handles' STATE: what is grabbed, what the cursor is over,
    //   and how far the grab has moved since the last frame.
    //
    //   Owned by EditorService and reached through EditorContext, the EditorSelection / EditorCamera
    //   shape — not a ViewportPanel member. Its subject is the SELECTION, which no panel owns
    //   (**SEL7**), and from ③'s rotate step the MODE is set by an editor-wide shortcut.
    //
    //   THE GRAB IS LATCHED, never interrogated after the fact. ImGui::IsMouseDragging needs the
    //   button still down, so it is false exactly on the frame a release wants an answer — the shape
    //   that cost ② three bugs (**SEL8**). BeginDrag on the press, EndDrag on "not down", and the
    //   frames between are the drag.
    //
    //   The delta is BANKED, not applied: the panel measures it in its draw pass and spends it in
    //   OnPreRender through EntityOps (**SEL3**), because a draw pass reads the world and anything
    //   that writes it runs outside the pass.
    // =============================================================================
    class EditorGizmo
    {
        // =============================================================================
        // Write
        // =============================================================================
    public:
        void BeginDrag(const EGizmoHandle InHandle, const Vector2F& InCursor) noexcept
        {
            m_Grabbed    = InHandle;
            m_LastCursor = InCursor;
        }

        /**
         * Accumulate the motion from the last cursor to InCursor, constrained to the grabbed axis.
         *
         * Measured between two WORLD cursor positions rather than from a pixel delta, so it is exact
         * at any zoom with no scale bookkeeping — and it stays correct across a frame the panel
         * skipped.
         */
        void DragTo(const Vector2F& InCursor) noexcept
        {
            if (m_Grabbed == EGizmoHandle::None) { return; }

            const Vector2F lStep = GizmoHandles::Constrain(m_Grabbed,
                                                           { InCursor.x - m_LastCursor.x,
                                                             InCursor.y - m_LastCursor.y });

            m_PendingDelta.x += lStep.x;
            m_PendingDelta.y += lStep.y;
            m_LastCursor      = InCursor;
        }

        void EndDrag() noexcept { m_Grabbed = EGizmoHandle::None; }

        void SetHovered(const EGizmoHandle InHandle) noexcept { m_Hovered = InHandle; }

        /** The banked motion, cleared. {0,0} when the grab has not moved. */
        Vector2F ConsumeDelta() noexcept
        {
            const Vector2F lDelta = m_PendingDelta;
            m_PendingDelta = { 0.f, 0.f };

            return lDelta;
        }

        // =============================================================================
        // Read
        // =============================================================================
    public:
        bool IsDragging() const noexcept { return m_Grabbed != EGizmoHandle::None; }

        /** What the draw highlights: the grabbed handle, or the hovered one when nothing is held. */
        EGizmoHandle GetActiveHandle() const noexcept
        {
            return m_Grabbed != EGizmoHandle::None ? m_Grabbed : m_Hovered;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EGizmoHandle m_Grabbed      = EGizmoHandle::None;
        EGizmoHandle m_Hovered      = EGizmoHandle::None;
        Vector2F     m_LastCursor   = { 0.f, 0.f };
        Vector2F     m_PendingDelta = { 0.f, 0.f };
    };
}
