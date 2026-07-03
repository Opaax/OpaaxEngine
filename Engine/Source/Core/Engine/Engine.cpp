#include "Engine.h"

#include "Core/Application/OpaaxApplication.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/IJobSystem.h"

//Subsystems
#include "Core/Engine/Subsystems/Resources/ResourceManager.h"
#include "Subsystems/Resources/Types/BinaryResource.hpp"

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
    
    double lTime = 0;
    ResourceRef<BinaryResource> m_TestRef;
    bool                        m_TestKicked = false;
    
    void Engine::Loop()
    {
        OpaaxApplication::GetAppService<IJobSystem>().DrainCompletions();
        
        double lPrev = lTime;
        lTime += 0.016;
        
        double lDeltaTime = lTime - lPrev;
        m_Resources->Update(lDeltaTime);
        
        ResourceRef<BinaryResource> m_TestRef2 = m_Resources->LoadAsync(path);
        
        if (!m_TestKicked)
        {
            m_TestKicked = true;
            OpaaxString path = OpaaxApplication::GetAppService<IPaths>().EngineToAbsolute("Assets/Test.bin");
            m_Resources->LoadAsync<BinaryResource>(path.CStr(),
                [this](LoadAsyncResult<BinaryResource> LoadedResource)          // capture 'this', not '&' of stack locals
                {
                    if (LoadedResource.bFailed) { OPAAX_ENGINE_LOG(Error, "Test.bin failed") }
                    else
                    {
                        OPAAX_ENGINE_LOG(Info, "Test.bin loaded: {} bytes in {} time", LoadedResource.Ref->Bytes.size(), lTime)
                        m_TestRef = LoadedResource.Ref;                         // member -> outlives the callback
                    }
                });
        }

        if (m_TestRef.IsValid())
        {
            OPAAX_ENGINE_LOG(Info, "VALID: {} bytes", m_TestRef->Bytes.size())
        }
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
