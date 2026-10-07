#pragma once

#include <Application/Services/IEngine.h>
#include <Engine/Subsystems/EngineSubsystem.h>
#include "Application/Services/IJobSystem.h"
#include "Renderer/RendererManager.h"
#include "Engine/FrameInfo.hpp"
#include "Core/Events/EventBus.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Resources/ResourceRef.hpp"

namespace Opaax
{
    class IPlatform;
    class IPaths;
    class ResourceManager;
    class AudioManager;
    class EngineEventBus;
    class WorldManager;
    class GameInstanceManager;
    class World;
    class IFramebuffer;
    class IRenderTarget;
    struct FramebufferSpec;
    struct LevelResource;

    inline constexpr double MAX_FRAME_DELTA = 0.25;

    // =============================================================================
    // Engine — the concrete IEngine. Owns the engine subsystems and runs the frame.
    // Provided last to the service locator, so it shuts down first (before the window).
    // =============================================================================
    class Engine final : public IEngine
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
        /** @return True if not started yet and the world is not null */
        bool CanFinishStartup();
    
        /**
         * Registers every self-registered type (OPAAX_REGISTER_* macros) of the engine and the game
         * modules: components, behaviours, subsystems, resource formats, widgets, mover modes.
         */
        void RegisterTypes();

        /** Registers the default engine subsystems. */
        void RegisterNativeSubsystems();
        
        /** Caches frequently used subsystems. */
        void CacheSubsystems();
        /** Caches frequently used app services. */
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
         * Loads the level named by InSpec, before the world is created.
         * @return Null for an empty path (no warning) or a missing level (warning)
         */
        ResourceRef<LevelResource> ResolveLevel(const OpaaxString& InAssetRelPath) const;

        /** Opens the level requested by RequestOpenLevel, at the start of the frame. */
        void ResolvePendingLevel();
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
        // Override
        // =============================================================================
        
        //~Begin IAppService interface
    public:
        void OnShutdown() override;
        //~End IAppService interface
        
        //~Begin IEngine interface
    public:
        //Life cycle
        bool    Startup() override;
        bool    StartGame() override;
        bool    EndGame() override;
        World*  FinishStartup(const WorldSpec& InSpec) override;
        World*  OpenLevel(const WorldSpec& InSpec) override;
        void    RequestOpenLevel(const WorldSpec& InSpec) override;
        void    Loop() override;
        void    TearDown() override;
        void    Shutdown() override;
        
        //Tick
        void Update(double InDeltaTime) override;
        void FixedUpdate(double InFixedDeltaTime) override;
        void Render(double InAlphaPhysicStep) override;
        
        //Render
        void                        PresentBackbuffer() override;
        void                        SubmitRenderView(IRenderTarget& InTarget, const CameraView& InView,
                                                     bool bInDrawOverlays, World* InSource = nullptr,
                                                     bool bInDrawUI = false) override;
        void                        SubmitUICanvas(UICanvas& InCanvas, IRenderTarget* InTarget = nullptr,
                                                   const CameraView* InView = nullptr) override;
        bool                        CaptureFrame(const OpaaxString& InPngPath) override;
        TUniquePtr<IFramebuffer>    CreateFramebuffer(const FramebufferSpec& InSpec) override;
        TUniquePtr<ITexture2D>      CreateTexture(const void* InPixels, Uint32 InWidth,
                                                  Uint32 InHeight, Int32 InChannels) override;

        //Getters
        EngineRegistries&   GetRegistries()     override { return m_Registries; }
        ResourceManager&    GetResources()      override;
        EngineEventBus&     GetEngineEventBus() override;
        WorldManager&       GetWorldManager()   override;
        GameInstanceManager& GetGameInstances() override;
        DebugDraw&          GetDebugDraw()      override;
        InputManager&       GetInput()          override;
        AudioManager&       GetAudio()          override;
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

        // Owned here: registration state, not a subsystem's state (see EngineRegistries.h)
        EngineRegistries m_Registries;
        
        // Convenient ptrs
        EngineEventBus*     m_EngineEventBus = nullptr;
        ResourceManager*    m_Resources = nullptr;
        RendererManager*    m_RendererManager = nullptr;
        WorldManager*       m_WorldManager = nullptr;
        InputManager*       m_InputManager = nullptr;
        AudioManager*       m_AudioManager = nullptr;
        GameInstanceManager* m_GameInstances = nullptr;

        //Internal
        bool m_bStarted = false;

        /** Level requested by RequestOpenLevel, opened at the start of the next frame. */
        WorldSpec m_PendingLevel;
        bool      m_bLevelPending = false;
    };
}
