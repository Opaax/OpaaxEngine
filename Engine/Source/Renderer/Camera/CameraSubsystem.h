#pragma once

#include "Core/Systems/EngineSubsystem.h"
#include "Core/ApplicationEvents.hpp"
#include "Core/OpaaxTypes.h"

#include "ICamera.h"

namespace Opaax
{
    /**
     * @class CameraSubsystem
     *
     * Engine subsystem that owns the active 2D camera as UniquePtr<ICamera>. Default at
     * Startup is a fresh OrthographicCamera sized from the Window. SetActiveCamera swaps
     * the slot; controllers and the editor camera reuse this entry point.
     *
     * Subscribes to WindowResizeEvent and forwards size into the active camera. Consumers
     * fetch the active camera by reference at the use site — never cache ICamera* across
     * a function call that could trigger a swap.
     */
    class OPAAX_API CameraSubsystem final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(CameraSubsystem)

        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        CameraSubsystem() = default;
        explicit CameraSubsystem(CoreEngineApp* InEngineApp) : EngineSubsystemBase(InEngineApp) {}
        ~CameraSubsystem() override = default;

        CameraSubsystem(const CameraSubsystem&)            = delete;
        CameraSubsystem& operator=(const CameraSubsystem&) = delete;
        CameraSubsystem(CameraSubsystem&&)                 = default;
        CameraSubsystem& operator=(CameraSubsystem&&)      = default;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        bool OnWindowResize(WindowResizeEvent& Event);

        //------------------------------------------------------------------------------
        //  Get - Set
    public:
        /**
         * Transfer ownership of a new active camera. The previous active camera is destroyed.
         */
        void SetActiveCamera(UniquePtr<ICamera> InCamera);

        /**
         * Returns the active camera by reference. Guaranteed non-null after Startup.
         */
        ICamera& GetActiveCamera();

        /**
         * Push a viewport size into the active camera. Called by the editor on ViewportPanel
         * resize and internally on WindowResizeEvent.
         */
        void SetViewportSize(Uint32 InWidth, Uint32 InHeight);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
    public:
        bool Startup()  override;
        void Shutdown() override;

        Uint32 GetEventCategoryFilter() const noexcept override
        {
        #if OPAAX_WITH_EDITOR
            // Editor: ViewportPanel is the render target and pushes its own size via
            // EditorSubsystem's OnResized callback — window dims would race the FBO
            // and leave the projection misaligned until the user drags the panel.
            return EEventCategory_None;
        #else
            return EEventCategory_Application;
        #endif
        }

        bool OnEvent(OpaaxEvent& Event) override;
        //~End EngineSubsystemBase Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        UniquePtr<ICamera> m_ActiveCamera;
    };

} // namespace Opaax
