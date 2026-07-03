#include "Engine.h"

#include "Core/Application/Services/ILogger.h"
#include "Core/Engine/Subsystems/Resources/ResourceManager.h"

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
        m_Resources = m_Subsystems.GetSubsystem<ResourceManager>();
        m_bStarted  = true;

        OPAAX_ENGINE_LOG(Info, "Engine started ({} subsystem(s))", m_Subsystems.GetSystems().size())
        return true;
    }

    void Engine::Loop()
    {
        
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

        OPAAX_ENGINE_LOG(Info, "Engine shut down")
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
