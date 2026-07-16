#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"


// =============================================================================
// RendererManager
// =============================================================================
namespace Opaax
{
    class RenderSystem;
    class WorldManager;
    struct WindowResize;

    inline constexpr LogCategory LogRendererManager{"RendererManager"};

    // =============================================================================
    // RendererManager — the ENGINE ADAPTER for the portable RenderSystem. This is the
    //   only render-side code allowed to reach host globals (services/config/paths): it
    //   resolves them, builds a RenderSystemDesc, owns one RenderSystem, and drives its
    //   frame each tick. All actual rendering lives in the RenderSystem module, which knows
    //   nothing of this engine — so the same core runs unchanged in any other host.
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
        // Out-of-line — the owned UniquePtr<RenderSystem> holds a forward-declared type.
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
        /**
         * 
         * @return The portable render core, or nullptr before Startup. For future render peers.
         */
        RenderSystem* GetRenderSystem() const noexcept { return m_RenderSystem.get(); }

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
        // Functions
        // =============================================================================
    private:
        // Bus handler — updates the cached viewport size and resizes the render core.
        void OnWindowResized(const WindowResize& InResize);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        UniquePtr<RenderSystem> m_RenderSystem;
        WorldManager*           m_WorldManager = nullptr; // non-owning; active world = draw source
        Uint32                  m_ViewWidth  = 0;
        Uint32                  m_ViewHeight = 0;
    };
}
