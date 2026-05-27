#pragma once

#include "ICameraController.h"
#include "FollowParams.h"

#include "Core/EngineAPI.h"

namespace Opaax
{
    class World;
    class ICamera;

    /**
     * @class FollowCameraController
     *
     * Tracks a target entity's TransformComponent position with optional offset, deadzone,
     * and frame-rate-independent exponential smoothing. Resolves the target through the
     * World passed at construction — never caches the TransformComponent pointer across
     * frames (entt can relocate components on insertion/destruction).
     *
     * Params can be hot-mutated via GetParams() — swap Target for a new entity, retune
     * smoothing without rebuilding the controller.
     */
    class OPAAX_API FollowCameraController : public ICameraController
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        FollowCameraController(const FollowParams& InParams, World& InWorld);
        ~FollowCameraController() override = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        FORCEINLINE FollowParams&       GetParams() noexcept       { return m_Params; }
        FORCEINLINE const FollowParams& GetParams() const noexcept { return m_Params; }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin ICameraController Interface
    public:
        void Tick(ICamera& InCamera, double InDeltaSeconds) override;
        //~End ICameraController Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        FollowParams m_Params;
        World&       m_World;
    };

} // namespace Opaax
