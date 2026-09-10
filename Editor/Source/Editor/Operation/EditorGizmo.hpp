#pragma once

#include <cmath>            // cos/sin — the pose Local adopts from the primary entity
#include <glm/matrix.hpp>   // inverse — this frame's delta is M * inverse(M last frame)

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

    /**
     * WHAT a rotation or a scale turns about.
     *
     * Center and Origin are ONE shared point for the whole selection — the difference is only which
     * point, so N entities swing around it and keep their formation. Individual is a different KIND
     * of answer: every entity turns about itself and nothing orbits anything (Blender calls it
     * Individual Origins). Three modes rather than two because "rotate them as a group" and "rotate
     * each of them" are both things an author wants, and neither substitutes for the other.
     *
     * For a single entity all three coincide — bounds are centred on the transform — so the choice
     * only speaks on a multi-selection, and Center/Origin only diverge for a single entity the day
     * a per-sprite Offset lands.
     */
    enum class EGizmoPivot : Uint8
    {
        Center,
        Origin,
        Individual
    };

    /** WHICH AXES the handles run along: the world's, or the primary entity's own. */
    enum class EGizmoSpace : Uint8
    {
        World,
        Local
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

    inline const char* ToString(const EGizmoPivot InPivot) noexcept
    {
        switch (InPivot)
        {
        case EGizmoPivot::Center:     return "Center";
        case EGizmoPivot::Origin:     return "Origin";
        case EGizmoPivot::Individual: return "Individual";
        }

        return "Center";
    }

    inline const char* ToString(const EGizmoSpace InSpace) noexcept
    {
        return InSpace == EGizmoSpace::Local ? "Local" : "World";
    }

    // =============================================================================
    // EditorGizmo — the transform gizmo's SETTINGS: which mode is active, the pivot, the space,
    //   whether snapping is on and by how much.
    //
    //   Owned by EditorService and reached through EditorContext, the EditorSelection /
    //   EditorCamera shape — not a ViewportPanel member. The mode is set by an editor-wide shortcut
    //   and the toolbar, and every surface that draws a gizmo follows the same choice.
    //
    //   THE DRAG ITSELF IS NOT HERE (⑦-C P8 V3). It was, until a second surface drew a gizmo: the
    //   matrix ImGuizmo drives is per-drag state, and two panels reseating one matrix would fight a
    //   live drag. That half is GizmoDrag below, owned per surface.
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
        // Pivot and space
        // =============================================================================
    public:
        void        SetPivot(const EGizmoPivot InPivot) noexcept { m_Pivot = InPivot; }
        EGizmoPivot GetPivot() const noexcept                    { return m_Pivot; }

        /**
         * Whether each entity should turn about ITSELF this frame.
         *
         * FALSE FOR TRANSLATE whatever the pivot says, and that is not a special case being papered
         * over — a translation moves everything by the same offset, so "about its own origin" has
         * no meaning there. Stated once here so the toolbar's label and the mutation agree.
         */
        bool UsesIndividualOrigins() const noexcept
        {
            return m_Pivot == EGizmoPivot::Individual && m_Mode != EGizmoMode::Translate;
        }

        void        SetSpace(const EGizmoSpace InSpace) noexcept { m_Space = InSpace; }
        EGizmoSpace GetSpace() const noexcept                    { return m_Space; }

        /**
         * The space actually used this frame. **SCALE IS ALWAYS LOCAL**, whatever the toggle says.
         *
         * Not a simplification — the alternative is unrepresentable. Scaling along WORLD axes an
         * entity that is rotated by R is a SHEAR, and `{Position, Rotation, Scale}` has nowhere to
         * put one; the result would either skew visibly or come back as a bogus rotation. Unity
         * forces local scale for the same reason. The toolbar shows the toggle disabled in Scale
         * mode rather than letting it lie.
         *
         * ONE answer, read by both the toolbar and the panel, so the button and the behaviour
         * cannot disagree.
         */
        EGizmoSpace GetEffectiveSpace() const noexcept
        {
            return m_Mode == EGizmoMode::Scale ? EGizmoSpace::Local : m_Space;
        }

        // =============================================================================
        // Snapping
        // =============================================================================
    public:
        /** The toolbar's persistent toggle — "snap every drag", until turned off. */
        void SetSnapEnabled(const bool bInEnabled) noexcept { m_bSnapEnabled = bInEnabled; }
        bool IsSnapEnabled() const noexcept                 { return m_bSnapEnabled; }

        /**
         * Ctrl, read per frame. It INVERTS the toggle rather than setting it (Unity's behaviour):
         * holding it snaps while the toggle is off, and suppresses snapping while it is on.
         *
         * That asymmetry is the point — the key keeps working for an author who never opens the
         * toolbar, and stays useful for one who leaves the toggle on.
         */
        void SetSnapInverted(const bool bInInverted) noexcept { m_bSnapInverted = bInInverted; }

        /** What the drag should actually do this frame. */
        bool IsSnappingNow() const noexcept { return m_bSnapEnabled != m_bSnapInverted; }

        /**
         * The step the active mode snaps to: world units, degrees, or a scale fraction.
         *
         * Per-mode because one number cannot mean all three — 15 units of translation is arbitrary
         * where 15 degrees is the useful rotation step. Editable from the toolbar (③b); these were
         * hard-coded constants in ③.
         */
        float GetSnapStep() const noexcept
        {
            switch (m_Mode)
            {
            case EGizmoMode::Translate: return m_SnapTranslate;
            case EGizmoMode::Rotate:    return m_SnapRotate;
            case EGizmoMode::Scale:     return m_SnapScale;
            }

            return 1.f;
        }

        /**
         * The step for one NAMED mode, regardless of which is active — the grid asks for the
         * translate step while the author may be rotating.
         */
        float GetSnapStep(const EGizmoMode InMode) const noexcept
        {
            switch (InMode)
            {
            case EGizmoMode::Rotate: return m_SnapRotate;
            case EGizmoMode::Scale:  return m_SnapScale;
            default:                 return m_SnapTranslate;
            }
        }

        /** The step for one named mode — what a toolbar field edits. Clamped above zero. */
        float& SnapStepRef(const EGizmoMode InMode) noexcept
        {
            switch (InMode)
            {
            case EGizmoMode::Rotate: return m_SnapRotate;
            case EGizmoMode::Scale:  return m_SnapScale;
            default:                 return m_SnapTranslate;
            }
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EGizmoMode  m_Mode  = EGizmoMode::Translate;
        EGizmoPivot m_Pivot = EGizmoPivot::Center;
        EGizmoSpace m_Space = EGizmoSpace::World;

        // Snapping: a persistent toggle, plus this frame's Ctrl, which inverts it.
        bool  m_bSnapEnabled  = false;
        bool  m_bSnapInverted = false;

        // ③'s constants, now editable from the toolbar. Session-only, like EditorCamera's pan and
        // zoom — viewport state in this editor does not survive a restart.
        float m_SnapTranslate = 10.f;
        float m_SnapRotate    = 15.f;
        float m_SnapScale     = 0.1f;
    };

    // =============================================================================
    // GizmoDrag — ONE surface's live drag: the matrix ImGuizmo drives and the delta it produced.
    //   Owned by every panel that draws a gizmo, beside its selection and camera.
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
    class GizmoDrag
    {
        // =============================================================================
        // The matrix ImGuizmo drives
        // =============================================================================
    public:
        Matrix44F& Matrix() noexcept { return m_Matrix; }

        /**
         * Put the gizmo back on InPivot, turned by InRotationRad, with unit scale.
         *
         * Call this ONLY when no drag is live.
         *
         * THE ROTATION IS WHAT MAKES `Local` MEAN ANYTHING. ③ always built this matrix with an
         * identity rotation, so Local and World would have drawn identically; ③b feeds it the
         * primary entity's rotation when the effective space is Local, and zero otherwise. Scale is
         * always unit here — the matrix measures a DRAG, not the entity, and starting it anywhere
         * else would make the first frame's delta report a scale nobody applied.
         */
        void ReseatAt(const Vector2F& InPivot, const float InRotationRad) noexcept
        {
            const float lCos = std::cos(InRotationRad);
            const float lSin = std::sin(InRotationRad);

            m_Matrix = Matrix44F(1.f);

            // Column-major: column 0 is where X lands, column 1 where Y lands.
            m_Matrix[0][0] =  lCos; m_Matrix[0][1] = lSin;
            m_Matrix[1][0] = -lSin; m_Matrix[1][1] = lCos;

            m_Matrix[3][0] = InPivot.x;
            m_Matrix[3][1] = InPivot.y;

            m_PrevMatrix = m_Matrix;
            m_FrameRad   = InRotationRad;
        }

        /**
         * The pose the matrix was last seated with — the frame a banked delta's LINEAR part is
         * expressed in.
         *
         * Remembered rather than re-derived at apply time, because a drag does not reseat: the
         * frame is fixed for the whole gesture, and asking the selection again mid-drag could
         * answer differently. Without it a scale is unrecoverable — `R·S·R⁻¹` cannot be reduced to
         * S by anyone who does not know R.
         */
        float GetFrameRad() const noexcept { return m_FrameRad; }

        // =============================================================================
        // The banked delta
        // =============================================================================
    public:
        /**
         * Bank this frame's motion, derived from the gizmo's OWN matrix rather than from ImGuizmo's
         * `deltaMatrix` out-parameter.
         *
         * THAT PARAMETER CANNOT BE USED, and the reason is worth keeping: it means a different thing
         * per mode. Translation hands back a per-frame increment; rotation hands back
         * `modelInverse * rotation * model`, incremental AND conjugated about the pivot; but SCALE
         * hands back a pure origin-centred `Scale(...)` whose factor is measured **since the drag
         * started**. Used uniformly, that scales an entity's position about the WORLD ORIGIN and
         * compounds every frame — invisible at (0,0) and badly wrong anywhere else.
         *
         * `M * inverse(M last frame)` has none of that. It is per-frame by construction, and because
         * this matrix SITS ON THE PIVOT the conjugation falls out for free: the result maps each
         * entity's old placement to its new one, so translate, rotate-about-pivot and
         * scale-about-pivot are all just this one expression. One rule, no per-mode knowledge.
         */
        void BankFrameDelta() noexcept
        {
            const Matrix44F lDelta = m_Matrix * glm::inverse(m_PrevMatrix);

            m_PrevMatrix   = m_Matrix;
            m_PendingDelta = lDelta * m_PendingDelta;
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
        Matrix44F  m_Matrix       = Matrix44F(1.f);

        // What m_Matrix was last frame — the other half of the delta. Kept in step by ReseatAt while
        // idle, so the first frame of a drag differences against the pre-drag pose.
        Matrix44F  m_PrevMatrix   = Matrix44F(1.f);

        Matrix44F  m_PendingDelta = Matrix44F(1.f);
        bool       m_bHasPending  = false;

        // The pose ReseatAt last used. Fixed for the length of a drag, because a drag never reseats.
        float      m_FrameRad     = 0.f;
    };
}
