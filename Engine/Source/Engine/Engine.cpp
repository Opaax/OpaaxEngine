#include "Engine.h"

#include "World/Components/CameraComponent.h"
#include "World/Components/ColliderComponent.h"
#include "World/Components/DummyComponent.h"
#include "World/Components/RigidbodyComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Components/TransformComponent.h"

#include <chrono>

#include "Application/OpaaxApplication.h"
#include "Core/Log/Logger.h"
#include "Application/Services/IJobSystem.h"
#include "Application/Services/IPaths.h"
#include "Core/Profiling/Profiler.h"
#include "Platform/IPlatform.h"

//Subsystems
#include "Engine/EngineEvents.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/UI/UISubsystem.h"
#include "Engine/Subsystems/Resources/Types/UI/UICanvasResource.h"
#include "UI/Widgets/UIButton.h"
#include "UI/Widgets/UIImage.h"
#include "UI/Widgets/UIMask.h"
#include "UI/Widgets/UIPanel.h"
#include "UI/Widgets/UISafeArea.h"
#include "UI/Widgets/UIStack.h"
#include "UI/Widgets/UIText.h"
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
#include "World/Level.h"
#include "World/Serialization/LevelResource.hpp"
#include "World/Serialization/MapResource.hpp"

#include "RHI/Framebuffer.h"
#include "RHI/Texture.h"
#include "Engine/Subsystems/Resources/Types/Texture/TextureResource.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetResource.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipResource.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryResource.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeResource.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverResource.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFaceResource.h"
#include "Engine/Subsystems/Resources/Types/Font/FontFamilyResource.h"
#include "World/Components/TextComponent.h"
#include "World/Components/SpriteAnimatorComponent.h"
#include "World/Components/MoverComponent.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Prefab/PrefabResource.hpp"
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
        RegisterNativeUIWidgets();
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
        // Essential: every entity has one (CreateEntity adds it); it cannot be removed.
        m_Registries.Components().Register<TransformComponent>("Transform", /*bEssential*/true);
        m_Registries.Components().Register<DummyComponent>("Dummy");
        m_Registries.Components().Register<SpriteComponent>("Sprite");
        m_Registries.Components().Register<CameraComponent>("Camera");
        m_Registries.Components().Register<SpriteAnimatorComponent>("SpriteAnimator");
        m_Registries.Components().Register<TextComponent>("Text");

        // The collider puts an entity in the physics world; the rigidbody (optional) sets the body type.
        m_Registries.Components().Register<ColliderComponent>("Collider");
        m_Registries.Components().Register<RigidbodyComponent>("Rigidbody");
        m_Registries.Components().Register<MoverComponent>("Mover");

        // Which prefab an entity comes from. Saved in the map like any component.
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

        // A MoveMode is one tuning (like an animation clip); a Mover names several (like a library).
        m_Registries.Resources().Register<MoveModeResource>(OPAAX_ID("MoveMode"));
        m_Registries.Resources().Register<MoverResource>(OPAAX_ID("Mover"));

        // Gameplay binds to actions; a mapping context says which keys trigger them.
        m_Registries.Resources().Register<InputActionResource>(OPAAX_ID("InputAction"));
        m_Registries.Resources().Register<InputMappingContextResource>(OPAAX_ID("InputMappingContext"));

        // A prefab is loaded once, however many instances a level places.
        m_Registries.Resources().Register<PrefabResource>(OPAAX_ID("Prefab"));

        // An authored widget tree (.opaaxui). Each instance builds its own widgets from it.
        m_Registries.Resources().Register<UICanvasResource>(OPAAX_ID("UICanvas"));
    }

    void Engine::RegisterNativeUIWidgets()
    {
        // Widget types a .opaaxui can use. Game modules add their own; unknown types are skipped.
        m_Registries.UIWidgets().Register<UIPanel>(OPAAX_ID("UIPanel"));
        m_Registries.UIWidgets().Register<UIImage>(OPAAX_ID("UIImage"));
        m_Registries.UIWidgets().Register<UIText>(OPAAX_ID("UIText"));
        m_Registries.UIWidgets().Register<UIButton>(OPAAX_ID("UIButton"));

        // Masks its children: white shows, black hides.
        m_Registries.UIWidgets().Register<UIMask>(OPAAX_ID("UIMask"));

        // Keeps its children inside the screen's safe area.
        m_Registries.UIWidgets().Register<UISafeArea>(OPAAX_ID("UISafeArea"));

        // Lays its children out along an axis.
        m_Registries.UIWidgets().Register<UIStack>(OPAAX_ID("UIStack"));
    }

    void Engine::RegisterNativeWorldSubsystems()
    {
        // Play worlds only (its ShouldCreate decides).
        m_Registries.WorldSubsystems().Register<SpriteAnimationSubsystem>(OPAAX_ID("SpriteAnimation"));

        // Play worlds only: it moves transforms.
        m_Registries.WorldSubsystems().Register<PhysicsSubsystem>(OPAAX_ID("Physics"));

        // After Physics (subsystems tick in registration order): the mover must see this step's poses.
        m_Registries.WorldSubsystems().Register<MoverSubsystem>(OPAAX_ID("Mover"));

        // No ShouldCreate: colliders must be visible while editing. The debug channel toggles it.
        m_Registries.WorldSubsystems().Register<ColliderDebugSubsystem>(OPAAX_ID("ColliderDebug"));
    }

    void Engine::RegisterNativeMoverModes()
    {
        // These names are saved in .opaaxmovemode files: renaming one breaks existing assets.
        m_Registries.MoverModes().Register<GroundMoveMode>(OPAAX_ID("GroundMove"));
        m_Registries.MoverModes().Register<FlyMoveMode>(OPAAX_ID("FlyMove"));
    }

    void Engine::RegisterNativeGameInstanceSubsystems()
    {
        // The UI canvas first: it routes raw input and consumes what the UI used,
        // before input mapping evaluates this frame.
        m_Registries.GameInstanceSubsystems().Register<UISubsystem>(OPAAX_ID("UI"));

        m_Registries.GameInstanceSubsystems().Register<InputMappingSubsystem>(OPAAX_ID("InputMapping"));
    }

    void Engine::RegisterNativeSubsystems()
    {
        m_Subsystems.RegisterSubsystem<EngineEventBus>();
        m_Subsystems.RegisterSubsystem<ResourceManager>();
        m_Subsystems.RegisterSubsystem<InputManager>();

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
