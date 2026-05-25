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
     * Engine subsystem that exposes the active 2D camera. Internally tracks ownership via
     * a dual-slot model:
     *   - m_OwnedCamera: UniquePtr that owns the default OrthographicCamera created at
     *     Startup, and any runtime camera installed via SetActiveCamera(UniquePtr).
     *   - m_ActivePtr: raw pointer to whichever camera is currently active. Either equal
     *     to m_OwnedCamera.get() (owning swap), or to an external camera installed via
     *     SetActiveCameraNonOwning (used by the editor — EditorSubsystem owns the
     *     EditorCamera by UniquePtr and only hands a raw observer pointer here).
     *
     * Subscribes to WindowResizeEvent (release builds only — editor's ViewportPanel
     * callback is authoritative there). Consumers fetch the active camera by reference
     * at the use site and never cache ICamera* across a function call that could
     * trigger a swap (Lesson 17).
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
         * Transfer ownership of a new active camera. The previous owned camera is destroyed;
         * any non-owning active camera (e.g. EditorCamera) is structurally evicted — its
         * lifetime is the responsibility of the external owner.
         */
        void SetActiveCamera(UniquePtr<ICamera> InCamera);

        /**
         * Install an externally-owned camera as the active one. Pass nullptr to clear the
         * active pointer (used by EditorSubsystem::Shutdown to prevent a dangling pointer
         * during reverse-of-startup subsystem teardown). The owned slot is untouched.
         */
        void SetActiveCameraNonOwning(ICamera* InCamera);

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
        // Owned slot — holds the engine's default OrthographicCamera, or whatever the
        // game's SetActiveCamera installs at PIE Start. Reset on Shutdown.
        UniquePtr<ICamera> m_OwnedCamera;

        // Active pointer — either m_OwnedCamera.get() (owning swap) or an externally
        // owned camera (e.g. EditorSubsystem's EditorCamera). Cleared explicitly by the
        // external owner before its destruction (see EditorSubsystem::Shutdown).
        ICamera* m_ActivePtr = nullptr;

        // Last viewport size pushed in via SetViewportSize. Re-applied to every newly
        // installed camera so fresh runtime cameras at PIE Start (and any non-owning
        // editor camera swap) don't render with a degenerate 0x0 projection until the
        // user happens to resize the viewport panel.
        Uint32 m_LastViewportWidth  = 0;
        Uint32 m_LastViewportHeight = 0;
    };

} // namespace Opaax
