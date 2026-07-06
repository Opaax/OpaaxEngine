#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"


// =============================================================================
// RendererManager
// =============================================================================
namespace Opaax
{
    class IRenderAPI;
    class ICommandBuffer;
    class IRenderTarget;

    inline constexpr LogCategory LogRendererManager{"RendererManager"};

    // =============================================================================
    // RendererManager — the render department subsystem. Owns the RHI render API
    //   (device + frame lifecycle) instead of the old static RenderCommand facade,
    //   and the backbuffer render target. Selects the backend from engine config and
    //   binds against the window's graphics context (both reached through the app
    //   service locator). This pass: clears the backbuffer every frame. Drawing
    //   (Renderer2D, passes) consumes GetCommandBuffer()/GetBackbufferTarget() later.
    // =============================================================================
    class OPAAX_API RendererManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(RendererManager)

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        // Both defined in the .cpp — the owned UniquePtr members hold forward-declared types,
        // so their construction/destruction must be instantiated where those types are complete.
        RendererManager();
        ~RendererManager() override;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        RendererManager(const RendererManager&)            = delete;
        RendererManager& operator=(const RendererManager&) = delete;
        RendererManager(RendererManager&&)                 = delete;
        RendererManager& operator=(RendererManager&&)      = delete;

        // =============================================================================
        // Getters
        // =============================================================================
    public:
        // The live render API (device + frame lifecycle), or nullptr before Startup.
        IRenderAPI*    GetRenderAPI() const noexcept { return m_RenderAPI.get(); }

        // The frame's recorder — valid between BeginFrame and EndFrame. Render peers
        // (passes, Renderer2D) record their draws here once they land.
        ICommandBuffer& GetCommandBuffer() const;

        // The backbuffer target (the window surface), or nullptr before Startup.
        IRenderTarget* GetBackbufferTarget() const noexcept { return m_Backbuffer.get(); }

        //------------------------------------------------------------------------------
        // Set

        void SetClearColor(const Vector4F& InColor) noexcept { m_ClearColor = InColor; }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
    public:
        bool Startup()             override;
        void Shutdown()            override;
        void Render(double Alpha)  override;
        //~End EngineSubsystemBase Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Owns the device + frame lifecycle (replaces the static RenderCommand::s_API).
        UniquePtr<IRenderAPI>    m_RenderAPI;
        // The window backbuffer (DefaultRenderTarget). Concrete type stays in the .cpp.
        UniquePtr<IRenderTarget> m_Backbuffer;
        // Backbuffer clear color — a named default (dark slate), tweakable via SetClearColor.
        Vector4F                 m_ClearColor{1.f, 0.10f, 0.12f, 1.0f};
    };
}
