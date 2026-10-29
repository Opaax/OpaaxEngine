#pragma once

#include "Core/OpaaxTypes.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    /** How a camera's view moves against the fixed steps. */
    enum class ECameraRhythm : Uint8
    {
        Unknown,  // fewer steps seen than a window
        Still,    // not moved
        Steps,    // moved in fixed steps: framed between its last two
        Update,   // moved between the steps (in Update): framed where it was put
        Mixed     // one entity moved both ways: it jitters on screens faster than the steps
    };

    constexpr const char* ToString(const ECameraRhythm InRhythm) noexcept
    {
        switch (InRhythm)
        {
            case ECameraRhythm::Steps:  return "steps";
            case ECameraRhythm::Update: return "update";
            case ECameraRhythm::Mixed:  return "mixed";
            case ECameraRhythm::Still:  return "still";
            default:                    return "unknown";
        }
    }

    // =============================================================================
    // CameraStepMotion — what a camera's view did around one fixed step, seen at the start of the
    //   next: through its size and the entities of its chain drawn between steps (the camera itself
    //   always is).
    // =============================================================================
    struct CameraStepMotion
    {
        /** The last step moved the view, and it was moved since (in Update). */
        bool bInStep    = false;
        bool bSinceStep = false;

        /** One entity did both (the camera itself when bSizeBoth: its OrthoSize did). */
        EntityID MovedBoth = ENTITY_NONE;
        bool     bSizeBoth = false;
    };

    // =============================================================================
    // CameraRhythm — a camera's motions counted over windows of fixed steps: what camera.info
    //   answers, and the warning for a camera moved both in the steps and between them.
    // =============================================================================
    struct CameraRhythm
    {
        static constexpr Uint32 WINDOW_STEPS = 60;

        /** Steps of a window where one entity moved both ways that make it Mixed: a respawn now and then does not. */
        static constexpr Uint32 MIXED_STEPS = 10;

        /** The window being counted. */
        Uint32 Steps     = 0;
        Uint32 InStep    = 0;
        Uint32 SinceStep = 0;
        Uint32 Both      = 0;

        /** The last entity seen moving both ways, for the warning. */
        EntityID MovedBoth = ENTITY_NONE;
        bool     bSizeBoth = false;

        /** What the last full window said (Unknown before the first), and whether a Mixed one was warned about. */
        ECameraRhythm Last    = ECameraRhythm::Unknown;
        bool          bWarned = false;
    };

    /**
     * Counts InMotion into InOutRhythm; at the end of a window, sets Last from it and starts the next.
     * @return True on the first window found Mixed: the one time to warn.
     */
    inline bool ObserveCameraRhythm(CameraRhythm& InOutRhythm, const CameraStepMotion& InMotion) noexcept
    {
        ++InOutRhythm.Steps;
        InOutRhythm.InStep    += InMotion.bInStep ? 1 : 0;
        InOutRhythm.SinceStep += InMotion.bSinceStep ? 1 : 0;
        if (InMotion.MovedBoth != ENTITY_NONE)
        {
            ++InOutRhythm.Both;
            InOutRhythm.MovedBoth = InMotion.MovedBoth;
            InOutRhythm.bSizeBoth = InMotion.bSizeBoth;
        }

        if (InOutRhythm.Steps < CameraRhythm::WINDOW_STEPS)
        {
            return false;
        }

        if (InOutRhythm.Both >= CameraRhythm::MIXED_STEPS)
        {
            InOutRhythm.Last = ECameraRhythm::Mixed;
        }
        else if (InOutRhythm.InStep == 0 && InOutRhythm.SinceStep == 0)
        {
            InOutRhythm.Last = ECameraRhythm::Still;
        }
        else
        {
            InOutRhythm.Last = InOutRhythm.InStep >= InOutRhythm.SinceStep ? ECameraRhythm::Steps : ECameraRhythm::Update;
        }

        InOutRhythm.Steps     = 0;
        InOutRhythm.InStep    = 0;
        InOutRhythm.SinceStep = 0;
        InOutRhythm.Both      = 0;

        if (InOutRhythm.Last != ECameraRhythm::Mixed || InOutRhythm.bWarned)
        {
            return false;
        }
        InOutRhythm.bWarned = true;
        return true;
    }
}
