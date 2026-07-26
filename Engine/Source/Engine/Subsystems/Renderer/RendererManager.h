#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Engine/Subsystems/EngineSubsystem.h"


// =============================================================================
// RendererManager
// =============================================================================
namespace Opaax
{
    class RenderSystem;
    class WorldManager;
    class IRenderTarget;
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
        // Functions
        // =============================================================================
    private:
        /**
         * Bus handler
         * forwards a window resize to the render core (which resizes the backbuffer).
         * @param InResize The Event
         */
        void HandleWindowResize(const WindowResize& InResize);

        // =============================================================================
        // Getters - Setter
    public:
        /**
         * @return The portable render core, or nullptr before Startup. For future render peers.
         */
        RenderSystem* GetRenderSystem() const noexcept { return m_RenderSystem.get(); }

        /**
         * Present the backbuffer — called by Engine::PresentBackbuffer (host-driven, after TickFrame).
         * Separate from Render so the editor can draw UI to the backbuffer before the swap (S7). No-op
         * if the render core failed to start.
         */
        void Present();

        /**
         * Redirect the world render into InTarget instead of the backbuffer; nullptr restores the
         * backbuffer. Non-owning — the caller (editor's ViewportPanel) owns the target. Stored, then
         * read by Render() each frame to pick the target and its size (D2: the target's size drives
         * the view, replacing the old window-size cache).
         */
        void SetPrimaryRenderTarget(IRenderTarget* InTarget);
        
        // End Getters - Setter
        // =============================================================================

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
        UniquePtr<RenderSystem> m_RenderSystem;
        WorldManager*           m_WorldManager  = nullptr; // non-owning; active world = draw source
        IRenderTarget*          m_PrimaryTarget = nullptr; // non-owning; nullptr = backbuffer (I5)
    };
}
