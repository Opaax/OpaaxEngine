#pragma once

#include "Core/Systems/EngineSubsystem.h"
 
namespace Opaax
{
    class WindowResizeEvent;
    
    // =============================================================================
    // RenderSubsystem
    //
    // Owns the render API lifetime and drives Renderer2D init/shutdown.
    // Registered as an engine subsystem so it participates in the standard
    // Startup / Shutdown / Render lifecycle.
    //
    // Render() is called from the game loop with the physics interpolation alpha.
    // The game layer overrides CoreEngineApp::OnRender to issue draw calls between
    // Renderer2D::Begin and Renderer2D::End.
    //
    // NOTE: RenderSubsystem does NOT call Renderer2D::Begin/End itself — that
    //   belongs to game code. It only manages the API context and viewport.
    // =============================================================================
    class OPAAX_API RenderSubsystem final : public IEngineSubsystem
    {
    public:
        RenderSubsystem() = default;
        explicit RenderSubsystem(CoreEngineApp* InEngineApp) : IEngineSubsystem(InEngineApp) {}
        ~RenderSubsystem() override = default;
 
        RenderSubsystem(const RenderSubsystem&)            = delete;
        RenderSubsystem& operator=(const RenderSubsystem&) = delete;
        RenderSubsystem(RenderSubsystem&&)                 = default;
        RenderSubsystem& operator=(RenderSubsystem&&)      = default;
 
        // =============================================================================
        // IEngineSubsystem
        // =============================================================================
    public:
        bool Startup()  override;
        void Shutdown() override;
 
        Uint32 GetEventCategoryFilter() const noexcept override
        {
            return EEventCategory_Application;
        }
 
        bool OnEvent(OpaaxEvent& Event) override;
 
    private:
        bool OnWindowResize(WindowResizeEvent& Event);
    };
 
} // namespace Opaax