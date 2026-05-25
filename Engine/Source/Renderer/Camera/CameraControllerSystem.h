#pragma once

#include "Core/Systems/GameSubsystem.h"
#include "Core/OpaaxTypes.h"

#include "ICameraController.h"

#include <vector>

namespace Opaax
{

    /**
     * @class CameraControllerSystem
     *
     * Play-only game subsystem that drives camera controllers. Owns its controllers by
     * UniquePtr, ticks them in registration order each frame on whatever camera
     * CameraSubsystem::GetActiveCamera() returns. PIE-gated via the GameSubsystem layer —
     * controllers never tick in edit or paused mode.
     *
     * Auto-registered by CoreEngineApp::Initialize so games don't have to (M4 OD-4).
     */
    class OPAAX_API CameraControllerSystem final : public GameSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(CameraControllerSystem)

        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        CameraControllerSystem() = default;
        explicit CameraControllerSystem(CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}
        ~CameraControllerSystem() override = default;

        CameraControllerSystem(const CameraControllerSystem&)            = delete;
        CameraControllerSystem& operator=(const CameraControllerSystem&) = delete;
        CameraControllerSystem(CameraControllerSystem&&) noexcept        = default;
        CameraControllerSystem& operator=(CameraControllerSystem&&) noexcept = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Take ownership of a controller and call its OnAttach hook. Tick order is
         * registration order — positioners first, modifiers after.
         */
        void AttachController(UniquePtr<ICameraController> InController);

        /**
         * Call OnDetach on every owned controller, then destroy them all.
         */
        void DetachAll();

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IGameSubsystem Interface
    public:
        bool Startup()  override;
        void Shutdown() override;
        void Update(double DeltaTime) override;
        //~End IGameSubsystem Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        std::vector<UniquePtr<ICameraController>> m_Controllers;
    };

} // namespace Opaax
