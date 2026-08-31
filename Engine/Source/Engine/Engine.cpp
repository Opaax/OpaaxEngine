#include "Engine.h"

#include "World/Components/CameraComponent.h"
#include "World/Components/DummyComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Components/TransformComponent.h"

#include <chrono>

#include "Application/OpaaxApplication.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IJobSystem.h"
#include "Application/Services/IPaths.h"        // the startup level's path is asset-relative
#include "Application/Services/IStatsService.h" // the borrowed FrameProfiler + OPAAX_STAT_SCOPE
#include "Application/Services/Platforms/IPlatform.h"

//Subsystems
#include "Engine/EngineEvents.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Core/Maths/MathsStatics.h"
#include "Subsystems/Camera/CameraManager.h"
#include "Subsystems/EventBus/EngineEventBus.h"
#include "Subsystems/Input/InputManager.h"
#include "Subsystems/Renderer/RendererManager.h"
#include "World/WorldManager.h"
#include "World/WorldEvents.h"
#include "World/Level.h"                         // OpenLevel mounts through the world's Level
#include "World/Serialization/LevelResource.hpp" // a level is resolved as a resource
#include "World/Serialization/MapResource.hpp"   // registered as a native format; Level does the loading

#include "RHI/Framebuffer.h"   // FramebufferSpec + the TUniquePtr<IFramebuffer> deleter
#include "RHI/Texture.h"       // the TUniquePtr<ITexture2D> deleter
#include "Engine/Subsystems/Resources/Types/TextureResource.h" // registered as a native format

namespace Opaax
{
    Engine::Engine()
    {
        RegisterNativeComponents();
        RegisterNativeResourceFormats();
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

    void Engine::RegisterNativeComponents()
    {
        // ESSENTIAL: CreateEntity emplaces it on every entity, so it cannot be removed — picking,
        // the editor's icons and both render joins all stand on it being there.
        m_Registries.Components().Register<TransformComponent>("Transform", /*bEssential*/true);
        m_Registries.Components().Register<DummyComponent>("Dummy");
        m_Registries.Components().Register<SpriteComponent>("Sprite");
        m_Registries.Components().Register<CameraComponent>("Camera");
    }
    
    void Engine::RegisterNativeResourceFormats()
    {
        m_Registries.Resources().Register<LevelResource>(OPAAX_ID("Level"));
        m_Registries.Resources().Register<MapResource>(OPAAX_ID("Map"));
        m_Registries.Resources().Register<TextureResource>(OPAAX_ID("Texture"));
    }

    void Engine::RegisterNativeSubsystems()
    {
        m_Subsystems.RegisterSubsystem<EngineEventBus>();
        m_Subsystems.RegisterSubsystem<ResourceManager>();
        m_Subsystems.RegisterSubsystem<InputManager>();
        m_Subsystems.RegisterSubsystem<WorldManager>(&m_Registries);

        // Before the renderer for readability only — Loop runs UpdateAll and RenderAll as separate
        // passes, so the camera resolves this frame's view whatever order these two sit in.
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

        // Null when stats are off, and that is NOT a warning — it is the configured state.
        m_Profiler = lServices.Get<IStatsService>().GetProfiler();
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
        //The user may want to create a new world each time.
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

        // FailFast: a missing or unreadable level resolves to null rather than to a placeholder,
        // so this IS the existence check — and it covers a corrupt file too, which a stat would not.
        ResourceRef<LevelResource> lRef = m_Resources->Load<LevelResource>(lAbsPath.CStr());
        if (!lRef.IsValid())
        {
            // A project that NAMES a level it cannot open is a real misconfiguration — loud,
            // unlike the empty case above. Booting NullLevel anyway beats refusing to start.
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

        // Held before anything else exists: whatever is active now is what this call replaces,
        // and it must not be destroyed until the new world is up.
        World* const lPrevious = m_WorldManager->GetActiveWorld();

        // The level is read BEFORE the world exists, because the world takes its name from the
        // level's data.
        const ResourceRef<LevelResource> lLevelRef = ResolveLevel(InSpec.LevelPath);
        const LevelResource* const       lLevel    = lLevelRef.Get();

        const OpaaxString lName = (lLevel != nullptr) ? lLevel->Data.Name : OpaaxString(NULL_LEVEL_WORLD_NAME);

        World* lWorld = m_WorldManager->CreateWorld(lName, InSpec.Mode);
        if (lWorld == nullptr)
        {
            return nullptr;
        }

        // Mounted BEFORE activation, so every OnActiveWorldChanged subscriber sees a world with
        // its content already in it rather than one that fills in afterwards.
        if (lLevel != nullptr && lWorld->GetLevel() != nullptr)
        {
            lWorld->GetLevel()->SetData(lLevel->Data);

            const Level::MountResult lResult = lWorld->GetLevel()->MountAll();
            if (!lResult.IsValid())
            {
                // Loud. A level that half-opened leaves a world that LOOKS fine and is missing
                // content, which is the failure mode MapResource is FailFast to avoid — so the
                // engine must not pass over it quietly either.
                OPAAX_ENGINE_LOG(Error, "Level '{}' did not open cleanly ({} map(s) mounted, {} failed)",
                                 lLevel->Data.Name.CStr(), lResult.MapsMounted, lResult.MapsFailed);
            }
        }

        m_WorldManager->SetActiveWorld(lWorld);

        OPAAX_ENGINE_LOG(Info, "World '{}' ({}) opened and activated", lName.CStr(), ToString(InSpec.Mode));

        // THEN the old one goes. That order is the whole point: destroying first would leave a
        // frame with no active world, which is the same reason PIE::Stop re-activates before it
        // destroys the clone.
        if (lPrevious != nullptr && lPrevious != lWorld)
        {
            m_WorldManager->DestroyWorld(lPrevious);
        }

        return lWorld;
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
        
        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Begin start up");

        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Cache Application services....");
        CacheAppServices();

        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Startup Engine Subsystems....");
        m_Subsystems.StartupAll();

        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Cache Engine Subsystems....");
        CacheSubsystems();
        
        if (m_Resources != nullptr)
        {
            OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Push job system to Resources Manager");
            m_Resources->SetJobSystem(OpaaxApplication::GetAppService<IJobSystem>());
        }
        
        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Bind to world manager events");
        BindToWorldMgrEvents();

        m_bStarted  = true;

        if (m_EngineEventBus != nullptr)
        {
            m_EngineEventBus->GetEventBus().Publish(EngineStarted{});
        }

        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Finishing startup: ({} subsystem(s))", m_Subsystems.GetSystems().size());
        return true;
    }

    World* Engine::FinishStartup(const WorldSpec& InSpec)
    {
        if (!CanFinishStartup())
        {
            return nullptr;
        }

        // Boot IS OpenLevel, the first time — there is nothing active for it to replace, which is
        // the only way this call differs. A separate startup path would be a second thing to keep
        // in step with the one the editor uses all session.
        return OpenLevel(InSpec);
    }
    
    void Engine::Loop()
    {
        // Publish finished async jobs back to their main-thread completions first.
        m_JobSystem->DrainCompletions();

        // Single flush point — deliver this frame's queued events (window/input enqueued
        // in OnEvent before Loop, plus any from the job completions above) before update.
        m_EngineEventBus->GetEventBus().Flush();

        // ----------------------------------------------------------------
        // 2. Time
        // ----------------------------------------------------------------

        m_FrameInfo.m_DeltaTime = GetDeltaTime();

        {
            OPAAX_STAT_SCOPE(m_Profiler, "Update");
            Update(m_FrameInfo.m_DeltaTime);
        }

        m_FrameInfo.m_AccumulatedDeltaTime += m_FrameInfo.m_DeltaTime;
        m_FrameInfo.m_FixedDeltaTime = GetFixedDeltaTime();

        while (m_FrameInfo.m_AccumulatedDeltaTime >= m_FrameInfo.m_FixedDeltaTime)
        {
            // INSIDE the loop, so the scope's own Calls IS the step count and its Milliseconds is
            // the total — the profiler merges a re-entered scope (ST1). A separate FixedSteps field
            // said the same thing a second way.
            OPAAX_STAT_SCOPE(m_Profiler, "FixedUpdate");

            FixedUpdate(m_FrameInfo.m_FixedDeltaTime);
            m_FrameInfo.m_AccumulatedDeltaTime -= m_FrameInfo.m_FixedDeltaTime;
        }

        m_FrameInfo.m_AlphaPhysic = m_FrameInfo.m_AccumulatedDeltaTime / m_FrameInfo.m_FixedDeltaTime;

        {
            OPAAX_STAT_SCOPE(m_Profiler, "Render");
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

        OPAAX_ENGINE_LOG(Info, "Engine torn down");
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
        // Scoped HERE rather than in Loop because the HOST calls it, after the frame's UI pass (F2).
        // It is also where vsync blocks, so leaving it unnamed would put most of the frame in a row
        // called "Other" and make the panel useless.
        OPAAX_STAT_SCOPE(m_Profiler, "Present");

        if (m_RendererManager != nullptr)
        {
            m_RendererManager->Present();
        }
    }
    
    void Engine::SetPrimaryRenderTarget(IRenderTarget* InTarget)
    {
        if (m_RendererManager != nullptr)
        {
            m_RendererManager->SetPrimaryRenderTarget(InTarget);
        }
    }

    void Engine::SetVSync(bool InEnabled)
    {
        if (m_RendererManager != nullptr)
        {
            m_RendererManager->SetVSync(InEnabled);
        }
    }

    bool Engine::IsVSyncEnabled() const
    {
        return m_RendererManager != nullptr && m_RendererManager->IsVSyncEnabled();
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

        // Safety net — still nothing means the host has not started the engine yet.
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
        // Resolve-from-manager first (see GetResources): a sibling subsystem may reach
        // the bus during its own Startup, so this must never re-enter Engine::Startup.
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
        // Resolve-from-manager first (see GetResources): never re-enter Startup.
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

    InputManager& Engine::GetInput()
    {
        // Resolve-from-manager first (see GetResources): never re-enter Startup.
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
    
    DebugDraw& Engine::GetDebugDraw()
    {
        // Resolve-from-manager first (see GetResources): never re-enter Startup.
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
