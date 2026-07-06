#pragma once

#include <Core/Application/Services/IEngine.h>
#include <Core/Engine/Subsystems/EngineSubsystem.h>

#include "Subsystems/Renderer/RendererManager.h"

namespace Opaax
{
    class ResourceManager;

    // =============================================================================
    // Engine — the concrete IEngine. Owns the EngineSubsystemMgr and the engine's
    // per-frame tick. Registers the ResourceManager (the first engine subsystem) and
    // exposes it via GetResources(). Provided into the AppServiceLocator last, so it
    // tears down FIRST (reverse-order) — before the window/GPU context dies.
    // =============================================================================
    class OPAAX_API Engine final : public IEngine
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        Engine();
        ~Engine() override;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        Engine(const Engine&)            = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&)                 = delete;
        Engine& operator=(Engine&&)      = delete;
        
        // =============================================================================
        // Function
        // =============================================================================
        
        // =============================================================================
        // Getters 
        public:
        
        bool HasStarted() const { return m_bStarted; }
        
        // End Getters 
        // =============================================================================
        
        // =============================================================================
        // Override
        // =============================================================================
        
        //~Begin IAppService interface
    public:
        void OnShutdown() override; // reverse-order locator teardown -> Shutdown()
        //~End IAppService interface

        
        //~Begin IEngine interface
    public:
        bool Startup() override;
        void Loop() override;
        void Update(double InDeltaTime) override;
        void FixedUpdate(double InFixedDeltaTime) override;
        void Render(double InAlphaPhysicStep) override;
        void Shutdown() override;

        ResourceManager& GetResources() override;
        //~End IEngine interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /**
         * Handle Subsystem lifetime
         */
        EngineSubsystemMgr m_Subsystems;

        /**
         * Convenient ptr, lifetime not managed by engine itself but through subsystem
         */
        ResourceManager*   m_Resources = nullptr;
        /***/
        RendererManager*   m_RendererManager = nullptr;
        bool               m_bStarted  = false;

        // Per-frame delta-time source (steady clock). Stored as nanoseconds so the header
        // stays <chrono>-free; the clock read + conversion live in Engine::Loop.
        Uint64             m_LastTickNs = 0;
        bool               m_bHasTick   = false;
    };
}
