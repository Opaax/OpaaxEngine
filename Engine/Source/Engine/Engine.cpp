#include "Engine.h"

#include "World/Components/DummyComponent.h"

#include <chrono>

#include "Application/OpaaxApplication.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IJobSystem.h"
#include "Application/Services/IPaths.h"        // the startup level's path is asset-relative
#include "Application/Services/Platforms/IPlatform.h"

//Subsystems
#include "Engine/EngineEvents.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Core/Maths/MathsStatics.h"
#include "Subsystems/EventBus/EngineEventBus.h"
#include "Subsystems/Input/InputManager.h"
#include "Subsystems/Renderer/RendererManager.h"
#include "World/WorldManager.h"
#include "World/WorldEvents.h"
#include "World/Serialization/LevelLoader.h"     // M5: FinishStartup opens the startup level
#include "World/Serialization/LevelResource.hpp" // the startup level is resolved as a resource

#include "RHI/Framebuffer.h"   // FramebufferSpec + the UniquePtr<IFramebuffer> deleter

namespace Opaax
{
    Engine::Engine()
    {
        RegisterNativeComponents();
        RegisterNativeSubsystems();
    }

    Engine::~Engine()
    {
        Shutdown();
    }

    bool Engine::CanFinishStartup()
    {
        bool lReturnState = true;
        
        if (!m_bStarted)
        {
            OPAAX_ENGINE_LOG(Error, "FinishStartup called before Startup — no world created")
            lReturnState = false;
        }

        if (m_WorldManager == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "FinishStartup: no WorldManager subsystem — no world created")
            lReturnState = false;
        }
        
        return lReturnState;
    }

    void Engine::RegisterNativeComponents()
    {
        m_Registries.Components().Register<DummyComponent>("Dummy");
    }
    
    void Engine::RegisterNativeSubsystems()
    {
        m_Subsystems.RegisterSubsystem<EngineEventBus>();
        m_Subsystems.RegisterSubsystem<ResourceManager>();
        m_Subsystems.RegisterSubsystem<InputManager>();
        m_Subsystems.RegisterSubsystem<WorldManager>(&m_Registries);
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
        AppServiceLocator& lServices = OpaaxApplication::Services();
        
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

    void Engine::OnShutdown()
    {
        Shutdown();
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================

    bool Engine::Startup()
    {
        if (m_bStarted)
        {
            return true;
        }
        
        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Begin start up")

        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Cache Application services....")
        CacheAppServices();

        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Startup Engine Subsystems....")
        m_Subsystems.StartupAll();

        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Cache Engine Subsystems....")
        CacheSubsystems();
        
        if (m_Resources != nullptr)
        {
            OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Push job system to Resources Manager")
            m_Resources->SetJobSystem(OpaaxApplication::GetAppService<IJobSystem>());
        }
        
        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Bind to world manager events")
        BindToWorldMgrEvents();

        m_bStarted  = true;

        if (m_EngineEventBus != nullptr)
        {
            m_EngineEventBus->GetEventBus().Publish(EngineStarted{});
        }

        OPAAX_ENGINE_LOG(Info, "Engine::Startup ----> Finishing startup: ({} subsystem(s))", m_Subsystems.GetSystems().size())
        return true;
    }

    World* Engine::FinishStartup(const WorldSpec& InSpec)
    {
        if (!CanFinishStartup())
        {
            return nullptr;
        }

        // The level is read BEFORE the world exists, because the world takes its name from the
        // level's data. Instantiation still happens after CreateWorld — WS7 is unchanged.
        const ResourceRef<LevelResource> lLevelRef = ResolveStartupLevel(InSpec.LevelPath);
        const LevelResource* const       lLevel    = lLevelRef.Get();

        const OpaaxString lName = (lLevel != nullptr) ? lLevel->Data.Name : OpaaxString(NULL_LEVEL_WORLD_NAME);

        World* lWorld = m_WorldManager->CreateWorld(lName, InSpec.Mode);
        m_WorldManager->SetActiveWorld(lWorld);

        OPAAX_ENGINE_LOG(Info, "Startup world '{}' ({}) created and activated", lName.CStr(), ToString(InSpec.Mode))

        if (lLevel != nullptr && lWorld != nullptr)
        {
            OpenStartupLevel(lLevel->Data, *lWorld);
        }

        return lWorld;
    }

    ResourceRef<LevelResource> Engine::ResolveStartupLevel(const OpaaxString& InAssetRelPath) const
    {
        if (InAssetRelPath.IsEmpty())
        {
            OPAAX_ENGINE_LOG(Info, "No startup level configured — booting '{}'", NULL_LEVEL_WORLD_NAME)
            return {};
        }

        if (m_Resources == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "No ResourceManager — cannot open startup level '{}'",
                             InAssetRelPath.CStr())
            return {};
        }

        const OpaaxString lAbsPath = OpaaxApplication::GetAppService<IPaths>()
                                     .AssetToAbsolute(InAssetRelPath);

        // FailFast: a missing or unreadable level resolves to null rather than to a placeholder,
        // so this IS the existence check — and it covers a corrupt file too, which a stat would not.
        ResourceRef<LevelResource> lRef = m_Resources->Load<LevelResource>(lAbsPath.CStr());

        if (lRef.Get() == nullptr)
        {
            // A project that NAMES a level it cannot open is a real misconfiguration — loud,
            // unlike the empty case above. Booting NullLevel anyway beats refusing to start.
            OPAAX_ENGINE_LOG(Warn, "Startup level '{}' could not be opened — falling back to '{}'",
                             InAssetRelPath.CStr(), NULL_LEVEL_WORLD_NAME)
        }

        return lRef;
    }

    void Engine::OpenStartupLevel(const LevelData& InLevel, World& InWorld)
    {
        const LevelLoader::Result lResult = LevelLoader::LoadInto(
            InLevel, InWorld, GetRegistries().Components(),
            OpaaxApplication::GetAppService<IPaths>(), *m_Resources);

        if (!lResult.IsOk())
        {
            // Loud. A level that half-opened leaves a world that LOOKS fine and is missing
            // content, which is the failure mode MapResource is FailFast to avoid — so the
            // engine must not pass over it quietly either.
            OPAAX_ENGINE_LOG(Error, "Startup level '{}' did not open cleanly ({} map(s) loaded, {} failed)",
                             InLevel.Name.CStr(), lResult.MapsLoaded, lResult.MapsFailed)
        }
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
        Update(m_FrameInfo.m_DeltaTime);
        
        m_FrameInfo.m_AccumulatedDeltaTime += m_FrameInfo.m_DeltaTime;
        m_FrameInfo.m_FixedDeltaTime = GetFixedDeltaTime();
        
        while (m_FrameInfo.m_AccumulatedDeltaTime >= m_FrameInfo.m_FixedDeltaTime)
        {
            FixedUpdate(m_FrameInfo.m_FixedDeltaTime);
            m_FrameInfo.m_AccumulatedDeltaTime -= m_FrameInfo.m_FixedDeltaTime;
        }
        
        m_FrameInfo.m_AlphaPhysic = m_FrameInfo.m_AccumulatedDeltaTime / m_FrameInfo.m_FixedDeltaTime;
        Render(m_FrameInfo.m_AlphaPhysic);
    }
    
    void Engine::PresentBackbuffer()
    {
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
    
    UniquePtr<IFramebuffer> Engine::CreateFramebuffer(const FramebufferSpec& InSpec)
    {
        if (m_RendererManager == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "Engine::CreateFramebuffer before Startup — no renderer; none created.");
            return nullptr;
        }

        return m_RendererManager->CreateFramebuffer(InSpec);
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
        
        m_bStarted  = false;

        OPAAX_ENGINE_LOG(Info, "Engine shutdown")
    }
    
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

        OPAAX_ENGINE_LOG(Info, "Engine torn down")
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
