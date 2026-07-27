#pragma once

#include <Application/Services/IEngine.h>
#include <Engine/Subsystems/EngineSubsystem.h>
#include "Application/Services/IJobSystem.h"
#include "Subsystems/Renderer/RendererManager.h"
#include "FrameInfo.hpp"
#include "Core/Events/EventBus.h"

namespace Opaax
{
    class ResourceManager;
    class EngineEventBus;
    class WorldManager;
    class World;
    class IRenderTarget;

    inline constexpr double MAX_FRAME_DELTA = 0.25;

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
        
    private:
        // =============================================================================
        // Gather App services
    private:
        void CacheAppServices();
        
        // End Gather App services 
        // =============================================================================
        
        // =============================================================================
        // Delta Time
    private:
        double GetDeltaTime();
        double GetFixedDeltaTime();
        // End Delta Time
        // =============================================================================

        // =============================================================================
        // World event bridge
    private:
        void BindToWorldMgrEvents();
        void UnbindFromWorldMgrEvents();
        void HandleWorldCreated(World* InWorld);
        void HandleWorldDestroyed(World* InWorld);
        void HandleActiveWorldChanged(World* InOldWorld, World* InNewWorld);
        // End World event bridge
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
        void PresentBackbuffer() override;
        void SetPrimaryRenderTarget(IRenderTarget* InTarget) override;
        void Update(double InDeltaTime) override;
        void FixedUpdate(double InFixedDeltaTime) override;
        void Render(double InAlphaPhysicStep) override;
        void TearDown() override;
        void Shutdown() override;

        ResourceManager& GetResources() override;
        EngineEventBus&  GetEngineEventBus() override;
        WorldManager&    GetWorldManager() override;
        DebugDraw&       GetDebugDraw() override;
        //~End IEngine interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // =============================================================================
        // App system
        IJobSystem* m_JobSystem = nullptr;
        IPlatform*  m_Platform  = nullptr;
        // End App system
        // =============================================================================
        
        // =============================================================================
        // Delta Time
    private:
        double LastTime = 0.0f;
        
        FrameInfo m_FrameInfo;

        // End Delta Time
        // =============================================================================

        /**
         * Handle Subsystem lifetime
         */
        EngineSubsystemMgr m_Subsystems;
        
        /**
         * Convenient ptr, lifetime not managed by engine itself but through subsystem
         */
        EngineEventBus* m_EngineEventBus = nullptr;

        /**
         * Convenient ptr, lifetime not managed by engine itself but through subsystem
         */
        ResourceManager*   m_Resources = nullptr;
        
        /**
         * Convenient ptr, lifetime not managed by engine itself but through subsystem
         */
        RendererManager*   m_RendererManager = nullptr;

        /**
         * Convenient ptr, lifetime not managed by engine itself but through subsystem
         */
        WorldManager*      m_WorldManager = nullptr;

        // Per-frame delta-time source (steady clock). Stored as nanoseconds so the header
        // stays <chrono>-free; the clock read + conversion live in Engine::Loop.
        Uint64             m_LastTickNs = 0;
        bool               m_bHasTick   = false;
        bool               m_bStarted  = false;
    };
}
