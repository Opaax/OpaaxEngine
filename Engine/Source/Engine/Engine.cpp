#include "Engine.h"

#include "World/Components/DummyComponent.h"

#include <chrono>

#include "Application/OpaaxApplication.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IJobSystem.h"
#include "Application/Services/Platforms/IPlatform.h"

//Subsystems
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Core/Maths/MathsStatics.h"
#include "Subsystems/EventBus/EngineEventBus.h"
#include "Subsystems/Renderer/RendererManager.h"
#include "World/WorldManager.h"
#include "World/WorldEvents.h"

#include "RHI/Framebuffer.h"   // FramebufferSpec + the UniquePtr<IFramebuffer> deleter

namespace Opaax
{
    // =========================================================================
    // Construction only DECLARES the engine subsystems (registration queues a
    // factory). Nothing is constructed or started until Startup() — so the Engine
    // can be Provide()d during Bootstrap, before the window/renderer exist.
    // =========================================================================
    Engine::Engine()
    {
        // Engine natives FIRST, before any subsystem exists and long before a module gets a
        // turn (MR2) — a module can then never shadow an engine type.
        RegisterNativeTypes();

        // WorldManager borrows the registries: it seals them on its way to the first world,
        // and (M4) reads world-subsystem candidates out of them. Injected through the factory
        // rather than reached for, so it never has to know about Engine.
        m_Subsystems.RegisterSubsystem<EngineEventBus>();
        m_Subsystems.RegisterSubsystem<ResourceManager>();
        m_Subsystems.RegisterSubsystem<WorldManager>(&m_Registries);
        m_Subsystems.RegisterSubsystem<RendererManager>();
    }

    void Engine::RegisterNativeTypes()
    {
        // NOTE: DummyComponent is the whole native set today — the only live component type
        // the engine owns. A real type registered DLL-side is also what proves the
        // cross-boundary lookup in ComponentIdentityTests.
        m_Registries.Components().Register<DummyComponent>("Dummy");
    }

    EngineRegistries& Engine::GetRegistries()
    {
        return m_Registries;
    }

    Engine::~Engine()
    {
        Shutdown();
    }

    // =============================================================================
    // Gather App services
    // =============================================================================
    
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
    void Engine::CacheSubsystems()
    {
        // Resolve-from-manager, never a lazy self-Startup (F3 / L6). Safe to run before or
        // after StartupAll: the create pass is what populates the list.
        m_Resources       = m_Subsystems.GetSubsystem<ResourceManager>();
        m_RendererManager = m_Subsystems.GetSubsystem<RendererManager>();
        m_EngineEventBus  = m_Subsystems.GetSubsystem<EngineEventBus>();
        m_WorldManager    = m_Subsystems.GetSubsystem<WorldManager>();
    }

    bool Engine::Startup()
    {
        if (m_bStarted)
        {
            return true;
        }

        CacheAppServices();

        // Constructs then starts. No world exists when this returns — FinishStartup makes it,
        // which is what keeps ComponentRegistry unsealed long enough for a game module to
        // register into it. Startup/FinishStartup bracket that window on purpose (BO4).
        m_Subsystems.StartupAll();

        CacheSubsystems();

        // Wire the async worker pool from the app service locator (null object if none),
        // so ResourceManager::LoadAsync can run file IO/decode off the main thread.
        if (m_Resources != nullptr)
        {
            m_Resources->SetJobSystem(OpaaxApplication::GetAppService<IJobSystem>());
        }
        
        BindToWorldMgrEvents();

        m_bStarted  = true;
        
        //TODO
        //m_EngineEventBus->Publish(EngineStart)

        OPAAX_ENGINE_LOG(Info, "Engine started ({} subsystem(s))", m_Subsystems.GetSystems().size())
        return true;
    }

    World* Engine::FinishStartup(const WorldSpec& InSpec)
    {
        // Loud, not lenient. A host reaching here before Startup used to get the world anyway
        // via a lazy accessor, which silently moved the whole boot 0.7s early and sealed the
        // registries before any module could register (L22). Refusing is the fix; there is no
        // safety net to re-add.
        if (!m_bStarted)
        {
            OPAAX_ENGINE_LOG(Error, "FinishStartup called before Startup — no world created")
            return nullptr;
        }

        if (m_WorldManager == nullptr)
        {
            OPAAX_ENGINE_LOG(Error, "FinishStartup: no WorldManager subsystem — no world created")
            return nullptr;
        }

        World* lWorld = m_WorldManager->CreateWorld(InSpec.Name, InSpec.Mode);
        m_WorldManager->SetActiveWorld(lWorld);

        OPAAX_ENGINE_LOG(Info, "Startup world '{}' ({}) created and activated",
                         InSpec.Name.CStr(), ToString(InSpec.Mode))

        return lWorld;
    }

    // =========================================================================
    // Loop — one frame, driven by the application host (OpaaxApplication::RunApplication).
    // Computes a real delta from a steady clock, then pumps Update (Resources + subsystems)
    // and Render (RenderAll -> RendererManager). NOTE: present currently lives inside
    // RenderSystem::EndFrame (m_Device->Present); S7 moves it host-side. The host does NOT
    // call Window::SwapBuffers today.
    // =========================================================================
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

    // =========================================================================
    // PresentBackbuffer — the swapchain show, driven by the host AFTER TickFrame (S7 / D2). Kept
    // OUT of Render so the editor can draw its UI to the backbuffer between the world render and
    // the present. Only the backbuffer is ever presented — an offscreen primary target is not.
    // Delegates to the renderer adapter, which owns the device.
    // =========================================================================
    void Engine::PresentBackbuffer()
    {
        if (m_RendererManager != nullptr)
        {
            m_RendererManager->Present();
        }
    }

    // =========================================================================
    // SetPrimaryRenderTarget — redirect the world render into a caller-owned target (nullptr =
    // backbuffer, the runtime default). Forwards to the renderer adapter; the engine stores
    // nothing itself (I5). No-op before Startup (the adapter isn't resolved yet).
    // =========================================================================
    void Engine::SetPrimaryRenderTarget(IRenderTarget* InTarget)
    {
        if (m_RendererManager != nullptr)
        {
            m_RendererManager->SetPrimaryRenderTarget(InTarget);
        }
    }

    // =========================================================================
    // CreateFramebuffer — the only render-resource factory the engine exposes. Forwards to the
    // renderer adapter, which owns the device; the engine stores nothing itself (I5).
    // =========================================================================
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
        
        //TODO
        //m_EngineEventBus->Publish(EngineShuttingDown)
        // Wait for event bus flush?

        UnbindFromWorldMgrEvents();

        m_Subsystems.ShutdownAll();
        
        m_Resources         = nullptr;
        m_EngineEventBus    = nullptr;
        m_RendererManager   = nullptr;
        m_WorldManager      = nullptr;
        
        m_bStarted  = false;

        OPAAX_ENGINE_LOG(Info, "Engine shutdown")
    }

    // =========================================================================
    // World event bridge — Tier-2 (WorldManager delegates) -> Tier-3 (EngineEventBus).
    //
    // NOTE: Publish (immediate), never Enqueue — deliberately against the bus's documented
    // default. That default is sized for high-frequency input payloads carrying pure data
    // (which is why a WindowResize can be enqueued). These payloads carry a raw World*: an
    // enqueued WorldDestroyed would be delivered at the next Flush, long after WorldManager
    // erased the world, and the pointer would dangle. Immediate keeps one rule for all
    // three and lets a subscriber still touch the World while it is being destroyed.
    // =========================================================================
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
    // Per-frame tick — pumps every engine subsystem. Harmless before Startup()
    // (the subsystem list is empty, so these are no-ops).
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
    // TearDown — phase 1 of the two-phase stop, driven by the host right after the frame
    // loop exits (OpaaxApplication::RunApplication -> EngineTeardown).
    //
    // Everything is still alive here: every subsystem, every app service, the window, the
    // GPU context, and this Engine's own delegate bindings. That is the entire point — a
    // subsystem can still reach a sibling. Shutdown() is too late: it unbinds and destroys.
    //
    // Reverse registration order (see TearDownAll), so EngineEventBus — registered first —
    // tears down LAST. That is precisely what lets WorldManager announce its dying worlds
    // here and still have them delivered to subscribers.
    // =========================================================================
    void Engine::TearDown()
    {
        if (!m_bStarted)
        {
            return;
        }

        m_Subsystems.TearDownAll();

        OPAAX_ENGINE_LOG(Info, "Engine torn down")
    }

    // =========================================================================
    // GetResources — always valid. Lazily starts the engine if the host hasn't yet
    // (a safety net; the host SHOULD call Startup() during init). Safe today because
    // Resources is IO/GPU-free; revisit when a GPU subsystem joins the startup batch.
    // =========================================================================
    ResourceManager& Engine::GetResources()
    {
        // Resolve from the owned subsystem manager. It is populated by StartupAll's
        // create pass, so a sibling subsystem can reach this during its own Startup
        // WITHOUT re-entering Engine::Startup (the source of the boot re-entrancy).
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

    // =========================================================================
    // GetDebugDraw — the D10 seam. The queue itself lives in RendererManager (it is what drains it);
    // the engine only routes. Same resolve-from-manager shape as the three accessors above.
    // =========================================================================
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
