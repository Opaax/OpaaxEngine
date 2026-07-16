#include "Engine.h"

#include <chrono>

#include "Core/Application/OpaaxApplication.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/IJobSystem.h"
#include "Core/Application/Services/Platforms/IPlatform.h"

//Subsystems
#include "Core/Engine/Subsystems/Resources/ResourceManager.h"
#include "Maths/MathsStatics.h"
#include "Subsystems/EventBus/EngineEventBus.h"
#include "Subsystems/Renderer/RendererManager.h"

namespace Opaax
{
    // =========================================================================
    // Construction only DECLARES the engine subsystems (registration queues a
    // factory). Nothing is constructed or started until Startup() — so the Engine
    // can be Provide()d during Bootstrap, before the window/renderer exist.
    // =========================================================================
    Engine::Engine()
    {
        m_Subsystems.RegisterSubsystem<EngineEventBus>();
        m_Subsystems.RegisterSubsystem<ResourceManager>();
        m_Subsystems.RegisterSubsystem<RendererManager>();
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
        
        CacheAppServices();

        m_Subsystems.StartupAll();
        
        m_Resources         = m_Subsystems.GetSubsystem<ResourceManager>();
        m_RendererManager   = m_Subsystems.GetSubsystem<RendererManager>();
        m_EngineEventBus    = m_Subsystems.GetSubsystem<EngineEventBus>();
        
        // Wire the async worker pool from the app service locator (null object if none),
        // so ResourceManager::LoadAsync can run file IO/decode off the main thread.
        if (m_Resources != nullptr)
        {
            m_Resources->SetJobSystem(OpaaxApplication::GetAppService<IJobSystem>());
        }

        m_bStarted  = true;
        
        //TODO
        //m_EngineEventBus->Publish(EngineStart)

        OPAAX_ENGINE_LOG(Info, "Engine started ({} subsystem(s))", m_Subsystems.GetSystems().size())
        return true;
    }
    
    // =========================================================================
    // Loop — one frame, driven by the application host (OpaaxApplication::RunApplication,
    // which presents via Window::SwapBuffers right after). Computes a real delta from a
    // steady clock, then pumps Update (Resources + subsystems) and Render (RenderAll ->
    // RendererManager clears the backbuffer).
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

    void Engine::Shutdown()
    {
        if (!m_bStarted)
        {
            return;
        } // idempotent (dtor + OnShutdown both call this)
        
        //TODO
        //m_EngineEventBus->Publish(EngineShuttingDown)
        // Wait for event bus flush?

        m_Subsystems.ShutdownAll();
        m_Resources = nullptr;
        m_bStarted  = false;

        OPAAX_ENGINE_LOG(Info, "Engine shutdown")
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
    // GetResources — always valid. Lazily starts the engine if the host hasn't yet
    // (a safety net; the host SHOULD call Startup() during init). Safe today because
    // Resources is IO/GPU-free; revisit when a GPU subsystem joins the startup batch.
    // =========================================================================
    ResourceManager& Engine::GetResources()
    {
        if (!m_bStarted)
        {
            Startup();
        }
        
        OPAAX_ASSERT(m_Resources != nullptr);
        return *m_Resources;
    }

    EngineEventBus& Engine::GetEngineEventBus()
    {
        if (!m_bStarted)
        {
            Startup();
        }
        
        OPAAX_ASSERT(m_EngineEventBus != nullptr);
        return *m_EngineEventBus;
    }
}
