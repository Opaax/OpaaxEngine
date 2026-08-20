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
        // =============================================================================
        // Native Engine
    private:
        /** @return true if not started yet and world is not null */
        bool CanFinishStartup();
    
        /**  */
        void RegisterNativeComponents();

        /** Register the resource types the engine itself loads, and the extensions they claim. */
        void RegisterNativeResourceFormats();

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
        // Startup
    private:
        /**
         * Load the level InSpec names, BEFORE the world exists.
         * The world takes its name from the level's own data, so the level has to be read first.
         *
         * @return A null ref for an empty path (silent — a supported answer) and for one that
         *   does not resolve (a warning). Either way the caller boots the NullLevel world.
         */
        ResourceRef<LevelResource> ResolveLevel(const OpaaxString& InAssetRelPath) const;
        // End Startup
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
        //Life cycle
        bool    Startup() override;
        World*  FinishStartup(const WorldSpec& InSpec) override;
        World*  OpenLevel(const WorldSpec& InSpec) override;
        void    Loop() override;
        void    TearDown() override;
        void    Shutdown() override;
        
        //Tick
        void Update(double InDeltaTime) override;
        void FixedUpdate(double InFixedDeltaTime) override;
        void Render(double InAlphaPhysicStep) override;
        
        //Render
        void                        PresentBackbuffer() override;
        void                        SetPrimaryRenderTarget(IRenderTarget* InTarget) override;
        TUniquePtr<IFramebuffer>    CreateFramebuffer(const FramebufferSpec& InSpec) override;
        TUniquePtr<ITexture2D>      CreateTexture(const void* InPixels, Uint32 InWidth,
                                                  Uint32 InHeight, Int32 InChannels) override;

        //Getters
        EngineRegistries&   GetRegistries()     override { return m_Registries; }
        ResourceManager&    GetResources()      override;
        EngineEventBus&     GetEngineEventBus() override;
        WorldManager&       GetWorldManager()   override;
        DebugDraw&          GetDebugDraw()      override;
        InputManager&       GetInput()          override;
        //~End IEngine interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // App system
        IJobSystem* m_JobSystem = nullptr;
        IPlatform*  m_Platform  = nullptr;
        IPaths*     m_Paths     = nullptr;
        
        // End Delta Time
        FrameInfo m_FrameInfo;

        //Handle Subsystem lifetime
        EngineSubsystemMgr m_Subsystems;

        //Owned here because they are boot-order state, not the state of any one subsystem (see EngineRegistries.h)
        EngineRegistries m_Registries;
        
        // Convenient ptrs
        EngineEventBus*     m_EngineEventBus = nullptr;
        ResourceManager*    m_Resources = nullptr;
        RendererManager*    m_RendererManager = nullptr;
        WorldManager*       m_WorldManager = nullptr;
        InputManager*       m_InputManager = nullptr;

        //Internal
        bool m_bStarted = false;
    };
}
