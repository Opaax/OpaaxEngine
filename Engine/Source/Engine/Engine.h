#pragma once

#include <Application/Services/IEngine.h>
#include <Engine/Subsystems/EngineSubsystem.h>
#include "Application/Services/IJobSystem.h"
#include "Subsystems/Renderer/RendererManager.h"
#include "FrameInfo.hpp"
#include "Core/Events/EventBus.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"   // ResolveStartupLevel returns one by value

namespace Opaax
{
    class ResourceManager;
    class EngineEventBus;
    class WorldManager;
    class World;
    class IFramebuffer;
    class IRenderTarget;
    struct FramebufferSpec;
    struct LevelData;
    struct LevelResource;

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
        bool CanFinishStartup();
        
        // =============================================================================
        // Native Engine
    private:
        /**  */
        void RegisterNativeComponents();
        
        /** Register Default engine subsystems*/
        void RegisterNativeSubsystems();
        
        /** Cache convenient subsystems */
        void CacheSubsystems();
        /** Cache convenient app services*/
        void CacheAppServices();
        // End Native Engine
        // =============================================================================
        
        // =============================================================================
        // Delta Time
    private:
        double GetDeltaTime();
        double GetFixedDeltaTime();
        // End Delta Time
        // =============================================================================

        // =============================================================================
        // Startup content
    private:
        /**
         * Load the level InSpec names, BEFORE the world exists — the world takes its name from
         * the level's own data, so the level has to be read first.
         *
         * @return A null ref for an empty path (silent — a supported answer) and for one that
         *   does not resolve (a warning). Either way the caller boots the NullLevel world.
         */
        ResourceRef<LevelResource> ResolveStartupLevel(const OpaaxString& InAssetRelPath) const;

        /**
         * Instantiate InLevel's maps into the freshly-created startup world.
         *
         * Runs AFTER the world's subsystems have started, which is the same order a PIE clone
         * gets. That uniformity is deliberate — see WS7.
         */
        void OpenStartupLevel(const LevelData& InLevel, World& InWorld);
        // End Startup content
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
        World* FinishStartup(const WorldSpec& InSpec) override;
        void Loop() override;
        void PresentBackbuffer() override;
        void SetPrimaryRenderTarget(IRenderTarget* InTarget) override;
        UniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec& InSpec) override;
        void Update(double InDeltaTime) override;
        void FixedUpdate(double InFixedDeltaTime) override;
        void Render(double InAlphaPhysicStep) override;
        void TearDown() override;
        void Shutdown() override;

        EngineRegistries&   GetRegistries() override { return m_Registries; }
        ResourceManager&    GetResources() override;
        EngineEventBus&     GetEngineEventBus() override;
        WorldManager&       GetWorldManager() override;
        DebugDraw&          GetDebugDraw() override;
        InputManager&       GetInput() override;
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
         * The engine's type registries. Owned here because they are boot-order state, not the
         * state of any one subsystem (see EngineRegistries.h). Constructed with the Engine, so
         * they exist before any subsystem does.
         */
        EngineRegistries m_Registries;
        
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

        /**
         * Convenient ptr, lifetime not managed by engine itself but through subsystem.
         * Read every frame by Loop (EndFrame), so it is cached in CacheSubsystems like the rest.
         */
        InputManager*      m_InputManager = nullptr;

        // Per-frame delta-time source (steady clock). Stored as nanoseconds so the header
        // stays <chrono>-free; the clock read + conversion live in Engine::Loop.
        Uint64             m_LastTickNs = 0;
        bool               m_bHasTick   = false;
        bool               m_bStarted  = false;
    };
}
