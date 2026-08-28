#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    /** What the gizmo manipulates. Unreal, Unity and Godot all bind these to W / E / R. */
    enum class EGizmoMode : Uint8
    {
        Translate,
        Rotate,
        Scale
    };

    /** I11 — an enum gets a free ToString found by ADL. Total and silent; a log label. */
    inline const char* ToString(const EGizmoMode InMode) noexcept
    {
        switch (InMode)
        {
        case EGizmoMode::Translate: return "Translate";
        case EGizmoMode::Rotate:    return "Rotate";
        case EGizmoMode::Scale:     return "Scale";
        }

        return "Translate";
    }

    // =============================================================================
    // EditorGizmo — the transform gizmo's STATE: which mode is active, whether snapping is held,
    //   the matrix ImGuizmo drives, and the delta that matrix produced.
    //
    //   Owned by EditorService and reached through EditorContext, the EditorSelection /
    //   EditorCamera shape — not a ViewportPanel member. Its subject is the SELECTION, which no
    //   panel owns (**SEL7**), and the mode is set by an editor-wide shortcut.
    //
    //   THE MATRIX IS STATE, AND THAT IS THE WHOLE TRICK. ImGuizmo captures its start pose on the
    //   frame a drag begins and then drives the SAME matrix it was handed last frame, so the matrix
    //   has to persist across frames rather than be rebuilt from the selection each time. Re-seating
    //   it every frame would fight that captured state and the drag would fold back on itself. So it
    //   follows the selection only while nothing is being dragged (ReseatAt) and is left strictly
    //   alone in between.
    //
    //   The delta is BANKED, not applied: the panel measures inside the ImGui pass and spends it in
    //   OnPreRender through EntityOps (**MP7**/**SEL3**), because a draw pass reads the world and
    //   anything that writes it runs outside the pass.
    // =============================================================================
    class EditorGizmo
    {
        // =============================================================================
        // Mode
        // =============================================================================
    public:
        void       SetMode(const EGizmoMode InMode) noexcept { m_Mode = InMode; }
        EGizmoMode GetMode() const noexcept                  { return m_Mode; }

        // =============================================================================
        // Snapping
        // =============================================================================
    public:
        /** Held per frame from the modifier, not toggled — Ctrl means "snap this drag". */
        void SetSnapping(const bool bInSnapping) noexcept { m_bSnapping = bInSnapping; }
        bool IsSnapping() const noexcept                  { return m_bSnapping; }

        /**
         * The step the active mode snaps to: world units, degrees, or a scale fraction.
         *
         * Per-mode because one number cannot mean all three — 15 units of translation is arbitrary
         * where 15 degrees is the useful rotation step.
         */
        float GetSnapStep() const noexcept
        {
            switch (m_Mode)
            {
            case EGizmoMode::Translate: return 10.f;
            case EGizmoMode::Rotate:    return 15.f;
            case EGizmoMode::Scale:     return 0.1f;
            }

            return 1.f;
        }

        // =============================================================================
        // The matrix ImGuizmo drives
        // =============================================================================
    public:
        Matrix44F& Matrix() noexcept { return m_Matrix; }

        /**
         * Put the gizmo back on InPivot — translation only, identity rotation and unit scale.
         *
         * Call this ONLY when no drag is live. The pose is deliberately neutral rather than the
         * selection's own rotation: with N entities there is no single rotation to adopt, and every
         * delta is world-space anyway, so a neutral frame is the honest one for both cases.
         */
        void ReseatAt(const Vector2F& InPivot) noexcept
        {
            m_Matrix = Matrix44F(1.f);
            m_Matrix[3][0] = InPivot.x;
            m_Matrix[3][1] = InPivot.y;
        }

        // =============================================================================
        // The banked delta
        // =============================================================================
    public:
        /** Compose InDelta onto whatever is already banked — deltas multiply. */
        void BankDelta(const Matrix44F& InDelta) noexcept
        {
            m_PendingDelta = InDelta * m_PendingDelta;
            m_bHasPending  = true;
        }

        bool HasPendingDelta() const noexcept { return m_bHasPending; }

        /** The banked transform, cleared back to identity. */
        Matrix44F ConsumeDelta() noexcept
        {
            const Matrix44F lDelta = m_PendingDelta;

            m_PendingDelta = Matrix44F(1.f);
            m_bHasPending  = false;

            return lDelta;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EGizmoMode m_Mode      = EGizmoMode::Translate;
        bool       m_bSnapping = false;

        Matrix44F  m_Matrix       = Matrix44F(1.f);
        Matrix44F  m_PendingDelta = Matrix44F(1.f);
        bool       m_bHasPending  = false;
    };
}
