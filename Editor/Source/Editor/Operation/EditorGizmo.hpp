#pragma once

#include <cmath>            // cos/sin
#include <glm/matrix.hpp>   // inverse

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    /** What the gizmo manipulates (W / E / R, like Unreal, Unity and Godot). */
    enum class EGizmoMode : Uint8
    {
        Translate,
        Rotate,
        Scale
    };

    /**
     * What a rotation or scale turns about. Center and Origin are one shared point for the whole
     * selection (entities keep their formation); Individual turns each entity about itself.
     * For a single entity all three are the same.
     */
    enum class EGizmoPivot : Uint8
    {
        Center,
        Origin,
        Individual
    };

    /** Which axes the handles use: the world's, or the primary entity's own. */
    enum class EGizmoSpace : Uint8
    {
        World,
        Local
    };

    /** Enum to string (for logs). */
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
    // EditorGizmo — the transform gizmo settings: mode, pivot, space, snapping and steps.
    //   Owned by EditorService and reached through EditorContext, so every surface that draws a
    //   gizmo uses the same settings. The drag itself is GizmoDrag, owned per surface.
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
         * Whether each entity turns about itself this frame. Always false for translate.
         */
        bool UsesIndividualOrigins() const noexcept
        {
            return m_Pivot == EGizmoPivot::Individual && m_Mode != EGizmoMode::Translate;
        }

        void        SetSpace(const EGizmoSpace InSpace) noexcept { m_Space = InSpace; }
        EGizmoSpace GetSpace() const noexcept                    { return m_Space; }

        /**
         * The space actually used this frame. Scale is always local: scaling a rotated entity along
         * world axes is a shear, which Position/Rotation/Scale cannot hold. The toolbar shows the toggle
         * disabled in Scale mode.
         */
        EGizmoSpace GetEffectiveSpace() const noexcept
        {
            return m_Mode == EGizmoMode::Scale ? EGizmoSpace::Local : m_Space;
        }

        // =============================================================================
        // Snapping
        // =============================================================================
    public:
        /** The toolbar toggle: snap every drag. */
        void SetSnapEnabled(const bool bInEnabled) noexcept { m_bSnapEnabled = bInEnabled; }
        bool IsSnapEnabled() const noexcept                 { return m_bSnapEnabled; }

        /**
         * Ctrl, read every frame. It inverts the toggle (like Unity): snaps while the toggle is off,
         * suppresses snapping while it is on.
         */
        void SetSnapInverted(const bool bInInverted) noexcept { m_bSnapInverted = bInInverted; }

        /** Whether this frame's drag snaps. */
        bool IsSnappingNow() const noexcept { return m_bSnapEnabled != m_bSnapInverted; }

        /**
         * The active mode's snap step: world units, degrees, or a scale fraction. Editable in the toolbar.
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
         * The step of a given mode, whatever is active (the grid asks for the translate step).
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

        /** Sets a mode's step (from the toolbar). Clamped above zero. */
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

        // Snapping: a toggle, plus this frame's Ctrl, which inverts it.
        bool  m_bSnapEnabled  = false;
        bool  m_bSnapInverted = false;

        // Snap steps. Session-only (not saved), like the editor camera.
        float m_SnapTranslate = 10.f;
        float m_SnapRotate    = 15.f;
        float m_SnapScale     = 0.1f;
    };

    // =============================================================================
    // GizmoDrag — one surface's live drag: the matrix ImGuizmo drives and the delta it produced.
    //   Owned by every panel that draws a gizmo. The matrix must persist across frames (ImGuizmo drives
    //   the same matrix it was given last frame), so it only follows the selection while nothing is
    //   dragged (ReseatAt). The delta is stored and applied later in OnPreRender through EntityOps.
    // =============================================================================
    class GizmoDrag
    {
        // =============================================================================
        // The matrix ImGuizmo drives
        // =============================================================================
    public:
        Matrix44F& Matrix() noexcept { return m_Matrix; }

        /**
         * Places the gizmo on InPivot, rotated by InRotationRad, unit scale. Only while no drag is live.
         * The rotation is what makes Local space differ from World. Scale stays 1: the matrix measures
         * the drag, not the entity.
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
         * The pose the matrix was last placed with: the frame the delta's linear part is in. Fixed for
         * the whole drag (a scale R*S*R^-1 cannot be recovered without R).
         */
        float GetFrameRad() const noexcept { return m_FrameRad; }

        // =============================================================================
        // The banked delta
        // =============================================================================
    public:
        /**
         * Stores this frame's motion, computed as M * inverse(M last frame) from the gizmo's own matrix.
         * ImGuizmo's deltaMatrix is not used: for scale it is cumulative and origin-centred, which
         * scaled positions about the world origin. Because the matrix sits on the pivot, this one
         * expression covers translate, rotate and scale about the pivot.
         */
        void BankFrameDelta() noexcept
        {
            const Matrix44F lDelta = m_Matrix * glm::inverse(m_PrevMatrix);

            m_PrevMatrix   = m_Matrix;
            m_PendingDelta = lDelta * m_PendingDelta;
            m_bHasPending  = true;
        }

        bool HasPendingDelta() const noexcept { return m_bHasPending; }

        /** The stored delta, reset to identity. */
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

        // m_Matrix last frame (the other half of the delta). Kept in step by ReseatAt while idle.
        Matrix44F  m_PrevMatrix   = Matrix44F(1.f);

        Matrix44F  m_PendingDelta = Matrix44F(1.f);
        bool       m_bHasPending  = false;

        // The pose ReseatAt last used. Fixed during a drag.
        float      m_FrameRad     = 0.f;
    };
}
