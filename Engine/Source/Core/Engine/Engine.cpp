#include "Engine.h"

#include <chrono>

#include "Core/Application/OpaaxApplication.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/IJobSystem.h"

//Subsystems
#include "Core/Engine/Subsystems/Resources/ResourceManager.h"

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
        // Resources is the FIRST engine subsystem (design §9). Register more here in
        // startup order as they land (Render, Input, World, Physics...).
        m_Subsystems.RegisterSubsystem<ResourceManager>();
        m_Subsystems.RegisterSubsystem<RendererManager>();
    }

    Engine::~Engine()
    {
        Shutdown();
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

        m_Subsystems.StartupAll();
        
        m_Resources         = m_Subsystems.GetSubsystem<ResourceManager>();
        m_RendererManager   = m_Subsystems.GetSubsystem<RendererManager>();
        
        // Wire the async worker pool from the app service locator (null object if none),
        // so ResourceManager::LoadAsync can run file IO/decode off the main thread.
        if (m_Resources != nullptr)
        {
            m_Resources->SetJobSystem(OpaaxApplication::GetAppService<IJobSystem>());
        }

        m_bStarted  = true;

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
        OpaaxApplication::GetAppService<IJobSystem>().DrainCompletions();

        // Delta-time from the steady clock (first frame is 0 — no last tick yet).
        const Uint64 lNowNs = static_cast<Uint64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        const double lDeltaTime = m_bHasTick ? static_cast<double>(lNowNs - m_LastTickNs) * 1e-9 : 0.0;
        m_LastTickNs = lNowNs;
        m_bHasTick   = true;

        Update(lDeltaTime);
        // FixedUpdate stepping lands with physics; render-interpolation alpha is 1.0 for now.
        Render(1.0);
    }

    void Engine::Shutdown()
    {
        if (!m_bStarted)
        {
            return;
        } // idempotent (dtor + OnShutdown both call this)

        m_Subsystems.ShutdownAll();
        m_Resources = nullptr;
        m_bStarted  = false;

        OPAAX_ENGINE_LOG(Info, "Engine shutdown")
    }

    // =========================================================================
    // Per-frame tick — pumps every engine subsystem. Harmless before Startup()
    // (the subsystem list is empty, so these are no-ops).
    // =========================================================================
    void Engine::Update(double InDeltaTime)            { m_Subsystems.UpdateAll(InDeltaTime); }
    void Engine::FixedUpdate(double InFixedDeltaTime)  { m_Subsystems.FixedUpdateAll(InFixedDeltaTime); }
    void Engine::Render(double InAlphaPhysicStep)      { m_Subsystems.RenderAll(InAlphaPhysicStep); }

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
        
        return *m_Resources;
    }
}
