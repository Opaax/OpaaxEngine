#pragma once

#include "Core/Engine/Subsystems/EngineSubsystem.h"
#include "Renderer/Pass/RenderPipeline.h"
#include "Renderer/Renderer2D.h"

namespace Opaax
{
    class WindowResizeEventOld;

    /**
     * @class RenderSubsystem
     *
     * Owns the render API lifetime, the render pass pipeline, and drives Renderer2D
     * init/shutdown. Registered as an engine subsystem so it participates in the standard
     * Startup / Shutdown / Render lifecycle. Registers the built-in passes at Startup.
     */
    class OPAAX_API RenderSubsystem final : public EngineSubsystemBaseOld
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(RenderSubsystem)
        
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        RenderSubsystem() = default;
        explicit RenderSubsystem(CoreEngineApp* InEngineApp) : EngineSubsystemBaseOld(InEngineApp) {}
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
        bool OnWindowResize(WindowResizeEventOld& Event);

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
        Renderer2D     m_Renderer2D; // the batch renderer, threaded into the pipeline's passes
    };
} // namespace Opaax