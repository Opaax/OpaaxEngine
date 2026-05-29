#pragma once

#include "Core/Systems/EngineSubsystem.h"
#include "Renderer/Pass/RenderPipeline.h"

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
        RenderPipeline m_Pipeline;
    };
} // namespace Opaax