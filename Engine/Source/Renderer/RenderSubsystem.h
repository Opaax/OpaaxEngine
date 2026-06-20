#pragma once

#include "Core/Systems/EngineSubsystem.h"
#include "Core/OpaaxTypes.h"
#include "Renderer/Pass/RenderPipeline.h"
#include "Renderer/Camera/ICamera.h"
#include "World/IOverlayRenderSystem.h"

namespace Opaax
{
    class WindowResizeEvent;

    /**
     * @class RenderSubsystem
     *
     * Owns the render API lifetime, the render pass pipeline, and drives Renderer2D
     * init/shutdown. Registered as an engine subsystem so it participates in the standard
     * Startup / Shutdown / Render lifecycle. Registers the built-in passes at Startup.
     */
    class OPAAX_API RenderSubsystem final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(RenderSubsystem)
        
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        RenderSubsystem() = default;
        explicit RenderSubsystem(CoreEngineApp* InEngineApp) : EngineSubsystemBase(InEngineApp) {}
        ~RenderSubsystem() override = default;

        // =============================================================================
        // Copy - delete
        // =============================================================================
        RenderSubsystem(const RenderSubsystem&)            = delete;
        RenderSubsystem& operator=(const RenderSubsystem&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
        RenderSubsystem(RenderSubsystem&&)                 = default;
        RenderSubsystem& operator=(RenderSubsystem&&)      = default;

        // =============================================================================
        // Function
        // =============================================================================
    private:
        bool OnWindowResize(WindowResizeEvent& Event);

    public:
        // The frame's ordered pass list. CoreEngineApp::OnRender drives Execute on it.
        RenderPipeline& GetPipeline() noexcept { return m_Pipeline; }

        // Screen-space overlay (HUD) systems, owned here and dispatched by OverlayRenderPass between
        // Renderer2D::Begin/End. Replaces the DLL-broken static TPolymorphicList<IOverlayRenderSystem>.
        // The engine registers RenderStatsOverlaySystem at Startup (when render.stats); games append their
        // own HUD systems from CoreEngineApp::OnStartup. Registration order == draw order.
        void RegisterOverlaySystem(UniquePtr<IOverlayRenderSystem> InSystem);
        const TDynArray<UniquePtr<IOverlayRenderSystem>>& GetOverlaySystems() const noexcept { return m_OverlaySystems; }

        // Active 2D camera (folded in from the former CameraSubsystem). Dual-slot ownership (Lesson 19):
        // m_OwnedCamera holds the engine default / a runtime SetActiveCamera; m_ActivePtr points at
        // whichever is live — the owned camera or an externally-owned one installed via
        // SetActiveCameraNonOwning (the editor's EditorCamera). Passes fetch GetActiveCamera() at Execute.
        void     SetActiveCamera(UniquePtr<ICamera> InCamera);
        void     SetActiveCameraNonOwning(ICamera* InCamera);
        ICamera& GetActiveCamera();
        void     SetViewportSize(Uint32 InWidth, Uint32 InHeight);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup()  override;
        void Shutdown() override;
        Uint32 GetEventCategoryFilter() const noexcept override { return EEventCategory_Application; }
        bool OnEvent(OpaaxEvent& Event) override;
        //~End EngineSubsystemBase interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        RenderPipeline                             m_Pipeline;
        TDynArray<UniquePtr<IOverlayRenderSystem>> m_OverlaySystems;

        // Camera (folded in from CameraSubsystem) — dual-slot ownership (Lesson 19).
        UniquePtr<ICamera> m_OwnedCamera;                  // engine default / runtime owned camera
        ICamera*           m_ActivePtr          = nullptr; // live camera (owned, or external non-owning)
        Uint32             m_LastViewportWidth  = 0;       // re-applied to a freshly installed camera
        Uint32             m_LastViewportHeight = 0;
    };
} // namespace Opaax