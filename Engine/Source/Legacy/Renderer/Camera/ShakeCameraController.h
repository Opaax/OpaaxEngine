#pragma once

#include "ICameraController.h"
#include "ShakeParams.h"

#include "Core/EngineAPI.h"

namespace Opaax
{
    class ICamera;

    /**
     * @class ShakeCameraController
     *
     * One-shot modifier controller. Drives a transient 2D offset on the active camera
     * via AddPositionOffset (which lands in the camera's transient slot — does not
     * mutate the base position, so it composes cleanly over a Follow controller).
     *
     * Auto-prunes via IsFinished once m_Elapsed reaches Duration. Tick order convention:
     * register modifiers AFTER positioners so this frame's shake decorates this frame's
     * Follow result.
     */
    class OPAAX_API ShakeCameraController final : public ICameraController
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        explicit ShakeCameraController(const ShakeParams& InParams);
        ~ShakeCameraController() override = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        FORCEINLINE ShakeParams&       GetParams() noexcept       { return m_Params; }
        FORCEINLINE const ShakeParams& GetParams() const noexcept { return m_Params; }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin ICameraController Interface
    public:
        void Tick(ICamera& InCamera, double InDeltaSeconds) override;
        bool IsFinished() const override;
        //~End ICameraController Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ShakeParams m_Params;
        float       m_Elapsed = 0.f;
    };

} // namespace Opaax
