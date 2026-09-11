#include "Engine.h"

#include "World/Components/CameraComponent.h"
#include "World/Components/ColliderComponent.h"
#include "World/Components/DummyComponent.h"
#include "World/Components/RigidbodyComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Components/TransformComponent.h"

#include <chrono>

#include "Application/OpaaxApplication.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IJobSystem.h"
#include "Application/Services/IPaths.h"        // the startup level's path is asset-relative
#include "Application/Services/IStatsService.h" // the borrowed FrameProfiler + OPAAX_STAT_SCOPE
#include "Platform/IPlatform.h"

//Subsystems
#include "Engine/EngineEvents.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/Subsystems/Resources/Types/Input/InputActionResource.h"
#include "Engine/Subsystems/Resources/Types/Input/InputMappingContextResource.h"
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
#include "Engine/Subsystems/Resources/Types/Texture/TextureResource.h" // registered as a native format
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetResource.h" // registered as a native format
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipResource.h" // registered as a native format
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryResource.h" // registered as a native format
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeResource.h"             // registered as a native format
#include "Engine/Subsystems/Resources/Types/Mover/MoverResource.h"                // registered as a native format
#include "Engine/Subsystems/Resources/Types/Font/FontFaceResource.h" // registered as a native format
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyResource.h" // registered as a native format
#include "World/Components/TextComponent.h"
#include "World/Components/SpriteAnimatorComponent.h"
#include "World/Components/MoverComponent.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Prefab/PrefabResource.hpp"       // registered as a native format
#include "World/Systems/ColliderDebugSubsystem.h"
#include "World/Systems/Movement/FlyMoveMode.h"
#include "World/Systems/Movement/GroundMoveMode.h"
#include "World/Systems/MoverSubsystem.h"
#include "World/Systems/PhysicsSubsystem.h"
#include "World/Systems/SpriteAnimationSubsystem.h"

namespace Opaax
{
    Engine::Engine()
    {
        RegisterNativeComponents();
        RegisterNativeResourceFormats();
        RegisterNativeSubsystems();
        RegisterNativeWorldSubsystems();
        RegisterNativeMoverModes();
        RegisterNativeGameInstanceSubsystems();
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
        m_Registries.Components().Register<SpriteAnimatorComponent>("SpriteAnimator");
        m_Registries.Components().Register<TextComponent>("Text");

        // ⑦-A. The collider is what puts an entity in the physics world; the rigidbody only says
        // what KIND of body it gets, which is why one is optional and the other is not.
        m_Registries.Components().Register<ColliderComponent>("Collider");
        m_Registries.Components().Register<RigidbodyComponent>("Rigidbody");
        m_Registries.Components().Register<MoverComponent>("Mover");

        // ⑦-C P1b. IDENTITY, not user data: it says which prefab an entity came from, which
        // placement, and which entity of that prefab it is. Registered like any other component
        // so the link is written into the map with no extra format work.
        m_Registries.Components().Register<PrefabInstanceComponent>("PrefabInstance");
    }
    
    void Engine::RegisterNativeResourceFormats()
    {
        m_Registries.Resources().Register<LevelResource>(OPAAX_ID("Level"));
        m_Registries.Resources().Register<MapResource>(OPAAX_ID("Map"));
        m_Registries.Resources().Register<TextureResource>(OPAAX_ID("Texture"));
        m_Registries.Resources().Register<SpriteSheetResource>(OPAAX_ID("SpriteSheet"));
        m_Registries.Resources().Register<AnimationClipResource>(OPAAX_ID("AnimationClip"));
        m_Registries.Resources().Register<AnimationLibraryResource>(OPAAX_ID("AnimationLibrary"));
        m_Registries.Resources().Register<FontFaceResource>(OPAAX_ID("FontFace"));
        m_Registries.Resources().Register<FontFamilyResource>(OPAAX_ID("FontFamily"));

        // ⑦-A P5a. The animation pair's shape one family over: a MoveMode is the CLIP (one tuning,
        // reusable across entities) and a Mover is the LIBRARY that names them.
        m_Registries.Resources().Register<MoveModeResource>(OPAAX_ID("MoveMode"));
        m_Registries.Resources().Register<MoverResource>(OPAAX_ID("Mover"));

        // ⑦-B B2. An action is what gameplay BINDS; a mapping context is which keys reach it.
        // Split so rebinding never touches the action, and one action can be driven by four keys
        // in one context and a single key in another.
        m_Registries.Resources().Register<InputActionResource>(OPAAX_ID("InputAction"));
        m_Registries.Resources().Register<InputMappingContextResource>(OPAAX_ID("InputMappingContext"));

        // ⑦-C P1b. A prefab is a Map's entities without a map's membership, so it is a resource
        // for MapResource's reasons — dedup above all: a level placing forty instances of one
        // prefab parses the file once.
        m_Registries.Resources().Register<PrefabResource>(OPAAX_ID("Prefab"));
    }

    void Engine::RegisterNativeWorldSubsystems()
    {
        // Play worlds only — its own ShouldCreate decides, so registering it here costs an Edit
        // world nothing: a rejected candidate is never constructed (WS2).
        m_Registries.WorldSubsystems().Register<SpriteAnimationSubsystem>(OPAAX_ID("SpriteAnimation"));

        // Also Play-only, and for the same reason: it MOVES authored transforms, which an Edit
        // world must never have done to it.
        m_Registries.WorldSubsystems().Register<PhysicsSubsystem>(OPAAX_ID("Physics"));

        // AFTER Physics, and the ORDER IS THE DESIGN: a mover sweeps against the world's shapes,
        // so it must see the poses this step produced rather than last step's. The subsystem
        // manager ticks in registration order, which is the only thing that guarantees it.
        m_Registries.WorldSubsystems().Register<MoverSubsystem>(OPAAX_ID("Mover"));

        // NO ShouldCreate — the first native subsystem without one, deliberately. A collider has
        // to be visible while you AUTHOR it, which is exactly when physics does not exist. What
        // switches it off is the debug CHANNEL, not the world's mode.
        m_Registries.WorldSubsystems().Register<ColliderDebugSubsystem>(OPAAX_ID("ColliderDebug"));
    }

    void Engine::RegisterNativeMoverModes()
    {
        // The id a `.opaaxmovemode` writes in its Mode field, so these names are FILE KEYS —
        // renaming one breaks every asset that names it.
        m_Registries.MoverModes().Register<GroundMoveMode>(OPAAX_ID("GroundMove"));
        m_Registries.MoverModes().Register<FlyMoveMode>(OPAAX_ID("FlyMove"));
    }

    void Engine::RegisterNativeGameInstanceSubsystems()
    {
        // The engine's own session subsystem, and the tier's first tenant: the layer that turns
        // InputManager's physical keys into named actions.
        m_Registries.GameInstanceSubsystems().Register<InputMappingSubsystem>(OPAAX_ID("InputMapping"));
    }

    void Engine::RegisterNativeSubsystems()
    {
        m_Subsystems.RegisterSubsystem<EngineEventBus>();
        m_Subsystems.RegisterSubsystem<ResourceManager>();
        m_Subsystems.RegisterSubsystem<InputManager>();

        // BEFORE WorldManager, and the ORDER IS THE DESIGN, twice over: UpdateAll walks
        // registration order, so a session subsystem publishes this frame's answer before any
        // world subsystem reads it — and TearDownAll walks it in REVERSE, so worlds are destroyed
        // before the session they belong to, with nothing enforcing it by hand.
        m_Subsystems.RegisterSubsystem<GameInstanceManager>(&m_Registries);
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

        // Worlds FIRST. A world subsystem may hold a pointer into a session subsystem, so the
        // session has to outlive every world that belongs to it — the same reasoning that puts
        // GameInstanceManager before WorldManager in the registration order.
        const Uint64 lDestroyed = (m_WorldManager != nullptr)
                                      ? m_WorldManager->DestroyWorldsOfMode(EWorldMode::Play)
                                      : 0;

        // Logged HERE, not after the call below, so the log reads in execution order: the worlds
        // are already gone by the time the session's own "GAME ENDED" line prints.
        OPAAX_ENGINE_LOG(Info, "EndGame — {} play world(s) destroyed, now ending the game instance", lDestroyed);

        return m_GameInstances->EndGame();
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
    
    void Engine::SubmitRenderView(IRenderTarget& InTarget, const CameraView& InView, bool bInDrawOverlays,
                                  World* InSource)
    {
        if (m_RendererManager != nullptr)
        {
            m_RendererManager->SubmitRenderView(InTarget, InView, bInDrawOverlays, InSource);
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

    GameInstanceManager& Engine::GetGameInstances()
    {
        // Resolve-from-manager first (see GetResources): never re-enter Startup.
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
