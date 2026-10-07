#include "Engine/Engine.h"

#include <chrono>

#include "Application/OpaaxApplication.h"
#include "Core/Log/Logger.h"
#include "Application/Services/IJobSystem.h"
#include "Application/Services/IPaths.h"
#include "Audio/AudioManager.h"
#include "Core/Profiling/Profiler.h"
#include "Platform/IPlatform.h"

#include "Core/Image/PngWriter.h"
#include "Core/Maths/MathsStatics.h"
#include "Engine/EngineEvents.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Registries/AutoRegistration.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/RendererManager.h"
#include "Resources/ResourceManager.h"
#include "RHI/Framebuffer.h"
#include "RHI/Texture.h"
#include "World/Level.h"
#include "World/Serialization/LevelResource.hpp"
#include "World/WorldEvents.h"
#include "World/WorldManager.h"

namespace Opaax
{
    Engine::Engine()
    {
        RegisterTypes();
        RegisterNativeSubsystems();
    }

    Engine::~Engine()
    {
        Shutdown();
    }
    
    // =============================================================================
    // Native Engine
    // =============================================================================

    bool Engine::CanFinishStartup()
    {
        bool lReturnState = true;
        
        if (!m_bStarted)
        {
            OPAAX_ENGINE_LOG(Error, "FinishStartup called before Startup — no world created");
            lReturnState = false;
        }

        if (m_WorldManager == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "FinishStartup: no WorldManager subsystem — no world created");
            lReturnState = false;
        }
        
        return lReturnState;
    }

    void Engine::RegisterTypes()
    {
        // Every type the program registered with an OPAAX_REGISTER_* macro: the engine's and the
        // game modules'. Runs before any world exists, so the registries are still open.
        ModuleRegistrar lRegistrar;
        lRegistrar.BindEngineRegistries(m_Registries);

        const Uint64 lCount = RunAutoRegistrations(lRegistrar);

        OPAAX_ENGINE_LOG(Trace, "Registered {} type(s): {} component(s), {} world subsystem(s), {} resource format(s)",
                         lCount, m_Registries.Components().Count(), m_Registries.WorldSubsystems().Count(),
                         m_Registries.Resources().Count());
    }

    void Engine::RegisterNativeSubsystems()
    {
        m_Subsystems.RegisterSubsystem<EngineEventBus>();
        m_Subsystems.RegisterSubsystem<ResourceManager>();
        m_Subsystems.RegisterSubsystem<InputManager>();

        // Before the worlds: they stop their sounds when they end, so audio shuts down after them.
        m_Subsystems.RegisterSubsystem<AudioManager>();

        // Before WorldManager: updates run in registration order (session before worlds)
        // and teardown in reverse (worlds destroyed before the session).
        m_Subsystems.RegisterSubsystem<GameInstanceManager>(&m_Registries);
        m_Subsystems.RegisterSubsystem<WorldManager>(&m_Registries);

        // Order does not matter: Loop runs UpdateAll and RenderAll as separate passes.
        m_Subsystems.RegisterSubsystem<CameraManager>();
        m_Subsystems.RegisterSubsystem<RendererManager>();
    }
    
    void Engine::CacheSubsystems()
    {
        m_Resources       = m_Subsystems.GetSubsystem<ResourceManager>();
        m_RendererManager = m_Subsystems.GetSubsystem<RendererManager>();
        m_EngineEventBus  = m_Subsystems.GetSubsystem<EngineEventBus>();
        m_WorldManager    = m_Subsystems.GetSubsystem<WorldManager>();
        m_InputManager    = m_Subsystems.GetSubsystem<InputManager>();
        m_AudioManager    = m_Subsystems.GetSubsystem<AudioManager>();
        m_GameInstances   = m_Subsystems.GetSubsystem<GameInstanceManager>();
    }
    
    void Engine::CacheAppServices()
    {        
        AppServiceLocator& lServices = OpaaxApplication::GetServices();
        
        m_Platform = &lServices.Get<IPlatform>();
        if (m_Platform->IsNull())
        {
            OPAAX_ENGINE_LOG(Warn, "Platform service is a Null service");
        }
        
        m_JobSystem = &lServices.Get<IJobSystem>();
        if (m_JobSystem->IsNull())
        {
            OPAAX_ENGINE_LOG(Warn, "JobSystem service is a Null service");
        }
        
        m_Paths = &lServices.Get<IPaths>();
        if (m_Paths->IsNull())
        {
            OPAAX_ENGINE_LOG(Warn, "Paths service is a Null service");
        }

    }

    // =============================================================================
    // Delta Time
    // =============================================================================
    
    double Engine::GetDeltaTime()
    {
        const double lTimeNow   = m_Platform->GetTimeSeconds();
        double lDelta = lTimeNow - m_FrameInfo.m_LastTime;
        m_FrameInfo.m_LastTime = lTimeNow;
        
        if (lDelta > MAX_FRAME_DELTA)
        {
            lDelta = MAX_FRAME_DELTA;
        }
        
        return lDelta;
    }

    double Engine::GetFixedDeltaTime()
    {
        constexpr double lFixedDelta = D60_HZ;
        return lFixedDelta;
    }
    
    // =========================================================================
    // Startup
    // =========================================================================

    ResourceRef<LevelResource> Engine::ResolveLevel(const OpaaxString& InAssetRelPath) const
    {
        // The user may want to create a new world each time.
        if (InAssetRelPath.IsEmpty())
        {
            OPAAX_ENGINE_LOG(Info, "No startup level configured — booting '{}'", NULL_LEVEL_WORLD_NAME);
            return {};
        }

        if (m_Resources == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "No ResourceManager — cannot open startup level '{}'", InAssetRelPath.CStr());
            return {};
        }

        const OpaaxString lAbsPath = m_Paths->AssetToAbsolute(InAssetRelPath);

        // FailFast: a missing or unreadable level gives null.
        ResourceRef<LevelResource> lRef = m_Resources->Load<LevelResource>(lAbsPath.CStr());
        if (!lRef.IsValid())
        {
            // The project names a level that cannot be opened: warn, then boot an empty world.
            OPAAX_ENGINE_LOG(Warn, "Startup level '{}' could not be opened — falling back to '{}'",
                             InAssetRelPath.CStr(), NULL_LEVEL_WORLD_NAME);
        }

        return lRef;
    }

    World* Engine::OpenLevel(const WorldSpec& InSpec)
    {
        if (m_WorldManager == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "OpenLevel: no WorldManager subsystem — no world created");
            return nullptr;
        }

        // The current world is kept until the new one is up.
        World* const lPrevious = m_WorldManager->GetActiveWorld();

        // Read the level first: the world takes its name from it.
        const ResourceRef<LevelResource> lLevelRef = ResolveLevel(InSpec.LevelPath);
        const LevelResource* const       lLevel    = lLevelRef.Get();

        const OpaaxString lName = (lLevel != nullptr) ? lLevel->Data.Name : OpaaxString(NULL_LEVEL_WORLD_NAME);

        World* lWorld = m_WorldManager->CreateWorld(lName, InSpec.Mode);
        if (lWorld == nullptr)
        {
            return nullptr;
        }

        // Mount before activating, so OnActiveWorldChanged subscribers see the content.
        if (lLevel != nullptr && lWorld->GetLevel() != nullptr)
        {
            lWorld->GetLevel()->SetData(lLevel->Data);

            const Level::MountResult lResult = lWorld->GetLevel()->MountAll();
            if (!lResult.IsValid())
            {
                // A half-opened level looks fine but misses content: report it.
                OPAAX_ENGINE_LOG(Error, "Level '{}' did not open cleanly ({} map(s) mounted, {} failed)",
                                 lLevel->Data.Name.CStr(), lResult.MapsMounted, lResult.MapsFailed);
            }
        }

        m_WorldManager->SetActiveWorld(lWorld);

        OPAAX_ENGINE_LOG(Info, "World '{}' ({}) opened and activated", lName.CStr(), ToString(InSpec.Mode));

        // Destroy the old world last, so there is always an active world.
        if (lPrevious != nullptr && lPrevious != lWorld)
        {
            m_WorldManager->DestroyWorld(lPrevious);
        }

        return lWorld;
    }

    void Engine::RequestOpenLevel(const WorldSpec& InSpec)
    {
        if (m_bLevelPending)
        {
            OPAAX_ENGINE_LOG(Warn, "RequestOpenLevel: '{}' replaces the pending '{}' — last wins",
                             InSpec.LevelPath.CStr(), m_PendingLevel.LevelPath.CStr());
        }

        m_PendingLevel  = InSpec;
        m_bLevelPending = true;

        // Published now, so a loading screen can be drawn this frame.
        if (m_EngineEventBus != nullptr)
        {
            m_EngineEventBus->GetEventBus().Publish(LevelLoadRequested{});
        }

        OPAAX_ENGINE_LOG(Info, "Level '{}' requested — opens at the next frame's start", InSpec.LevelPath.CStr());
    }

    void Engine::ResolvePendingLevel()
    {
        if (!m_bLevelPending)
        {
            return;
        }

        // Clear first: a level that fails to open is not retried every frame.
        m_bLevelPending = false;
        const WorldSpec lSpec = m_PendingLevel;

        OpenLevel(lSpec);

        if (m_EngineEventBus != nullptr)
        {
            m_EngineEventBus->GetEventBus().Publish(LevelLoadFinished{});
        }
    }

    // =========================================================================
    // World
    // =========================================================================
    
    void Engine::BindToWorldMgrEvents()
    {
        if (m_WorldManager != nullptr && m_EngineEventBus != nullptr)
        {
            m_WorldManager->OnWorldCreated.AddMember(this, &Engine::HandleWorldCreated);
            m_WorldManager->OnWorldDestroyed.AddMember(this, &Engine::HandleWorldDestroyed);
            m_WorldManager->OnActiveWorldChanged.AddMember(this, &Engine::HandleActiveWorldChanged);
        }
    }

    void Engine::UnbindFromWorldMgrEvents()
    {
        if (m_WorldManager != nullptr)
        {
            m_WorldManager->OnWorldCreated.RemoveAll(this);
            m_WorldManager->OnWorldDestroyed.RemoveAll(this);
            m_WorldManager->OnActiveWorldChanged.RemoveAll(this);
        }
    }
    
    void Engine::HandleWorldCreated(World* InWorld)
    {
        m_EngineEventBus->GetEventBus().Publish(WorldCreated{InWorld});
    }

    void Engine::HandleWorldDestroyed(World* InWorld)
    {
        m_EngineEventBus->GetEventBus().Publish(WorldDestroyed{InWorld});
    }

    void Engine::HandleActiveWorldChanged(World* InOldWorld, World* InNewWorld)
    {
        m_EngineEventBus->GetEventBus().Publish(ActiveWorldChanged{InOldWorld, InNewWorld});
    }
    
    // =========================================================================
    // Overrides
    // =========================================================================
    // =========================================================================
    // IAppService
    // =========================================================================
    void Engine::OnShutdown()
    {
        Shutdown();
    }
    
    // =========================================================================
    // IEngine
    // =========================================================================
    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool Engine::Startup()
    {
        if (m_bStarted)
        {
            return true;
        }

        CacheAppServices();

        m_Subsystems.StartupAll();

        CacheSubsystems();
        
        if (m_Resources != nullptr)
        {
            m_Resources->SetJobSystem(OpaaxApplication::GetAppService<IJobSystem>());
        }
        
        BindToWorldMgrEvents();

        m_bStarted  = true;

        if (m_EngineEventBus != nullptr)
        {
            m_EngineEventBus->GetEventBus().Publish(EngineStarted{});
        }

        OPAAX_ENGINE_LOG(Info, "Engine started — {} subsystem(s)", m_Subsystems.GetSystems().size());
        return true;
    }

    bool Engine::StartGame()
    {
        if (!m_bStarted)
        {
            OPAAX_ENGINE_LOG(Error, "StartGame called before Startup — no game created");
            return false;
        }

        if (m_GameInstances == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "StartGame: no GameInstanceManager subsystem — no game created");
            return false;
        }

        return m_GameInstances->StartGame();
    }

    bool Engine::EndGame()
    {
        if (m_GameInstances == nullptr || !m_GameInstances->IsGameRunning())
        {
            return false;
        }

        // Worlds first: a world subsystem may point into a session subsystem.
        const Uint64 lDestroyed = (m_WorldManager != nullptr)
                                      ? m_WorldManager->DestroyWorldsOfMode(EWorldMode::Play)
                                      : 0;

        // Logged here so the log follows execution order.
        OPAAX_ENGINE_LOG(Trace, "EndGame — {} play world(s) destroyed, now ending the game instance", lDestroyed);

        return m_GameInstances->EndGame();
    }

    World* Engine::FinishStartup(const WorldSpec& InSpec)
    {
        if (!CanFinishStartup())
        {
            return nullptr;
        }

        // Startup opens the level the same way as OpenLevel.
        return OpenLevel(InSpec);
    }
    
    void Engine::Loop()
    {
        // Run the completions of finished jobs first.
        m_JobSystem->DrainCompletions();

        // Deliver this frame's queued events (window/input, job completions) before update.
        m_EngineEventBus->GetEventBus().Flush();

        // A level requested last frame is swapped in here, before anything ticks.
        ResolvePendingLevel();

        // ----------------------------------------------------------------
        // 2. Time
        // ----------------------------------------------------------------

        m_FrameInfo.m_DeltaTime = GetDeltaTime();

        {
            OPAAX_STAT_SCOPE("Update");
            Update(m_FrameInfo.m_DeltaTime);
        }

        m_FrameInfo.m_AccumulatedDeltaTime += m_FrameInfo.m_DeltaTime;
        m_FrameInfo.m_FixedDeltaTime = GetFixedDeltaTime();

        while (m_FrameInfo.m_AccumulatedDeltaTime >= m_FrameInfo.m_FixedDeltaTime)
        {
            // Inside the loop: the scope's Calls is the step count.
            OPAAX_STAT_SCOPE("FixedUpdate");

            FixedUpdate(m_FrameInfo.m_FixedDeltaTime);
            m_FrameInfo.m_AccumulatedDeltaTime -= m_FrameInfo.m_FixedDeltaTime;
        }

        m_FrameInfo.m_AlphaPhysic = m_FrameInfo.m_AccumulatedDeltaTime / m_FrameInfo.m_FixedDeltaTime;

        {
            OPAAX_STAT_SCOPE("Render");
            Render(m_FrameInfo.m_AlphaPhysic);
        }
    }
    
    void Engine::TearDown()
    {
        if (!m_bStarted)
        {
            return;
        }
        
        if (m_EngineEventBus != nullptr)
        {
            m_EngineEventBus->GetEventBus().Publish(EngineTearingDown{});
        }

        m_Subsystems.TearDownAll();
    }

    void Engine::Shutdown()
    {
        if (!m_bStarted)
        {
            return;
        }
        
        UnbindFromWorldMgrEvents();

        m_Subsystems.ShutdownAll();
        
        m_Resources         = nullptr;
        m_EngineEventBus    = nullptr;
        m_RendererManager   = nullptr;
        m_WorldManager      = nullptr;
        m_InputManager      = nullptr;
        
        m_bStarted  = false;

        OPAAX_ENGINE_LOG(Info, "Engine shutdown");
    }
    
    // =========================================================================
    // Tick
    // =========================================================================
    
    void Engine::Update(double InDeltaTime)
    {
        m_Subsystems.UpdateAll(InDeltaTime);
    }
    
    void Engine::FixedUpdate(double InFixedDeltaTime)
    {
        m_Subsystems.FixedUpdateAll(InFixedDeltaTime);
    }
    void Engine::Render(double InAlphaPhysicStep)
    {
        m_Subsystems.RenderAll(InAlphaPhysicStep);
    }
    
    // =========================================================================
    // Render
    // =========================================================================
    
    void Engine::PresentBackbuffer()
    {
        // Scoped here: the host calls it after the UI pass, and vsync blocks here.
        OPAAX_STAT_SCOPE("Present");

        if (m_RendererManager != nullptr)
        {
            m_RendererManager->Present();
        }
    }
    
    bool Engine::CaptureFrame(const OpaaxString& InPngPath)
    {
        TDynArray<Uint8> lPixels;
        Uint32           lWidth  = 0;
        Uint32           lHeight = 0;

        if (m_RendererManager == nullptr || !m_RendererManager->CaptureBackbuffer(lPixels, lWidth, lHeight))
        {
            OPAAX_ENGINE_LOG(Error, "CaptureFrame: no frame to read (no render device)");
            return false;
        }

        if (!PngWriter::Write(InPngPath, lWidth, lHeight, 4, lPixels.data()))
        {
            OPAAX_ENGINE_LOG(Error, "CaptureFrame: '{}' could not be written", InPngPath.CStr());
            return false;
        }

        OPAAX_ENGINE_LOG(Info, "Frame captured to '{}' ({}x{})", InPngPath.CStr(), lWidth, lHeight);
        return true;
    }

    void Engine::SubmitRenderView(IRenderTarget& InTarget, const CameraView& InView, bool bInDrawOverlays,
                                  World* InSource, bool bInDrawUI)
    {
        if (m_RendererManager != nullptr)
        {
            m_RendererManager->SubmitRenderView(InTarget, InView, bInDrawOverlays, InSource, bInDrawUI);
        }
    }

    void Engine::SubmitUICanvas(UICanvas& InCanvas, IRenderTarget* InTarget, const CameraView* InView)
    {
        if (m_RendererManager != nullptr)
        {
            m_RendererManager->SubmitUICanvas(InCanvas, InTarget, InView);
        }
    }

    TUniquePtr<IFramebuffer> Engine::CreateFramebuffer(const FramebufferSpec& InSpec)
    {
        if (m_RendererManager == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "Engine::CreateFramebuffer before Startup — no renderer; none created.");
            return nullptr;
        }

        return m_RendererManager->CreateFramebuffer(InSpec);
    }

    TUniquePtr<ITexture2D> Engine::CreateTexture(const void* InPixels, Uint32 InWidth, Uint32 InHeight, Int32 InChannels)
    {
        if (m_RendererManager == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "Engine::CreateTexture before Startup — no renderer; none created.");
            return nullptr;
        }

        return m_RendererManager->CreateTexture(InPixels, InWidth, InHeight, InChannels);
    }
    
    // =========================================================================
    // Getters
    // =========================================================================
    
    ResourceManager& Engine::GetResources()
    {
        if (m_Resources == nullptr)
        {
            m_Resources = m_Subsystems.GetSubsystem<ResourceManager>();
        }

        // Still nothing: the engine has not started yet.
        if (m_Resources == nullptr && !m_bStarted)
        {
            Startup();
            m_Resources = m_Subsystems.GetSubsystem<ResourceManager>();
        }

        OPAAX_ASSERT(m_Resources != nullptr);
        return *m_Resources;
    }

    EngineEventBus& Engine::GetEngineEventBus()
    {
        // Look in the manager first: a subsystem may need the bus during its own Startup.
        if (m_EngineEventBus == nullptr)
        {
            m_EngineEventBus = m_Subsystems.GetSubsystem<EngineEventBus>();
        }

        if (m_EngineEventBus == nullptr && !m_bStarted)
        {
            Startup();
            m_EngineEventBus = m_Subsystems.GetSubsystem<EngineEventBus>();
        }

        OPAAX_ASSERT(m_EngineEventBus != nullptr);
        return *m_EngineEventBus;
    }

    WorldManager& Engine::GetWorldManager()
    {
        // Look in the manager first (see GetResources).
        if (m_WorldManager == nullptr)
        {
            m_WorldManager = m_Subsystems.GetSubsystem<WorldManager>();
        }

        if (m_WorldManager == nullptr && !m_bStarted)
        {
            Startup();
            m_WorldManager = m_Subsystems.GetSubsystem<WorldManager>();
        }

        OPAAX_ASSERT(m_WorldManager != nullptr);
        return *m_WorldManager;
    }

    GameInstanceManager& Engine::GetGameInstances()
    {
        // Look in the manager first (see GetResources).
        if (m_GameInstances == nullptr)
        {
            m_GameInstances = m_Subsystems.GetSubsystem<GameInstanceManager>();
        }

        if (m_GameInstances == nullptr && !m_bStarted)
        {
            Startup();
            m_GameInstances = m_Subsystems.GetSubsystem<GameInstanceManager>();
        }

        OPAAX_ASSERT(m_GameInstances != nullptr);
        return *m_GameInstances;
    }

    InputManager& Engine::GetInput()
    {
        // Look in the manager first (see GetResources).
        if (m_InputManager == nullptr)
        {
            m_InputManager = m_Subsystems.GetSubsystem<InputManager>();
        }

        if (m_InputManager == nullptr && !m_bStarted)
        {
            Startup();
            m_InputManager = m_Subsystems.GetSubsystem<InputManager>();
        }

        OPAAX_ASSERT(m_InputManager != nullptr);
        return *m_InputManager;
    }
    
    AudioManager& Engine::GetAudio()
    {
        // Look in the manager first (see GetResources).
        if (m_AudioManager == nullptr)
        {
            m_AudioManager = m_Subsystems.GetSubsystem<AudioManager>();
        }

        if (m_AudioManager == nullptr && !m_bStarted)
        {
            Startup();
            m_AudioManager = m_Subsystems.GetSubsystem<AudioManager>();
        }

        OPAAX_ASSERT(m_AudioManager != nullptr);
        return *m_AudioManager;
    }

    DebugDraw& Engine::GetDebugDraw()
    {
        // Look in the manager first (see GetResources).
        if (m_RendererManager == nullptr)
        {
            m_RendererManager = m_Subsystems.GetSubsystem<RendererManager>();
        }

        if (m_RendererManager == nullptr && !m_bStarted)
        {
            Startup();
            m_RendererManager = m_Subsystems.GetSubsystem<RendererManager>();
        }

        OPAAX_ASSERT(m_RendererManager != nullptr);
        return m_RendererManager->GetDebugDraw();
    }
}
