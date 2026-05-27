#pragma once

#include "Core/EngineAPI.h"

namespace Opaax
{
    class ICamera;

    /**
     * @class ICameraController
     *
     * Behavior layer on top of ICamera. Controllers do not own a camera — CameraControllerSystem
     * passes the currently active ICamera by reference into Tick each frame, so a controller
     * never holds a stale handle across a PIE camera swap (Lesson 17).
     *
     * Tick order in CameraControllerSystem is registration order. Convention: positioner
     * controllers (Follow) register first, modifiers (Shake) register after — so per-frame
     * the modifier sees the positioner's writes.
     */
    class OPAAX_API ICameraController
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        ICameraController()                                    = default;
        virtual ~ICameraController()                           = default;

        ICameraController(const ICameraController&)            = delete;
        ICameraController& operator=(const ICameraController&) = delete;
        ICameraController(ICameraController&&)                 = delete;
        ICameraController& operator=(ICameraController&&)      = delete;

        // =============================================================================
        // Interface
        // =============================================================================
    public:
        /**
         * Per-frame update. InCamera is the currently active camera at call time —
         * controllers must not cache it across calls.
         */
        virtual void Tick(ICamera& InCamera, double InDeltaSeconds) = 0;

        /**
         * Called when CameraControllerSystem takes ownership of the controller. Default no-op.
         * Useful for one-shot setup that depends on the initial camera.
         */
        virtual void OnAttach(ICamera& /*InCamera*/) {}

        /**
         * Called immediately before the controller is destroyed. Default no-op.
         */
        virtual void OnDetach(ICamera& /*InCamera*/) {}

        /**
         * Returns true when the controller has completed its work and should be auto-pruned
         * by CameraControllerSystem at the end of the next Update. Default false (runs forever);
         * one-shot modifiers like Shake override to return true once their Duration elapses.
         */
        virtual bool IsFinished() const { return false; }
    };

} // namespace Opaax
