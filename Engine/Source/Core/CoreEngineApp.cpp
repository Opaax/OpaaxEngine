#include "CoreEngineApp.h"
#include "Window.h"
#include <GLFW/glfw3.h>

#include "OpaaxString.hpp"
#include "OpaaxStringID.hpp"
#include "Log/OpaaxLog.h"
#include "Systems/EngineSubsystem.h"

using namespace Opaax;

CoreEngineApp::CoreEngineApp()
{
    //The only system to be created at very first
    OpaaxLog::Init();
    
    OPAAX_CORE_TRACE("CoreEngineApp created");

    m_Window = UniquePtr<Window>(Opaax::Window::Create());
}

CoreEngineApp::~CoreEngineApp()
{
    OPAAX_CORE_TRACE("CoreEngineApp destroyed");
}

void CoreEngineApp::Initialize()
{
    OPAAX_CORE_TRACE("CoreEngineApp::Initialize()");
    
// #if OPAAX_WITH_EDITOR
//     if (IsDebugMode())
//     {
//         LaunchEditor();
//         return;
//     }
// #endif
    
    // Load engine assets
    OPAAX_CORE_TRACE("Loading engine assets...");
    
    m_EngineSubsystemManager.RegisterSubsystem<EngineSubsystemBase>(this);
    
    // Call derived class initialization
    OnInitialize();
    
    bIsRunning = true;
}

void CoreEngineApp::Startup()
{
    OPAAX_CORE_TRACE("CoreEngineApp::Startup()");

    m_EngineSubsystemManager.StartupAll();
}

void CoreEngineApp::Run()
{
    Initialize();
    Startup();
    
    if(m_Window == nullptr)
    {
        OPAAX_CORE_ERROR("CoreEngineApp::Run() - Window is null!!");
        Shutdown();
        return;
    }
    
    OPAAX_CORE_TRACE("CoreEngineApp::Run() - Game loop started");

    const double lFixedDeltaTime = 1.0 / 60.0;  // physics at 60 Hz
    double lAccumulator = 0.0;
    double lLastTime = glfwGetTime();
    
    while (bIsRunning)
    {
        // -------------------------
        // Time management
        // -------------------------
        double lTimeNow = glfwGetTime();
        double lDeltaTime = lTimeNow - lLastTime;
        lLastTime = lTimeNow;

        // Avoid spiral of death on big lags
        if (lDeltaTime > 0.25)
        {
            lDeltaTime = 0.25;
        }

        lAccumulator += lDeltaTime;

        // -------------------------
        // 1. Variable Update (gameplay)
        // -------------------------
        // Input, AI, scripts, animations, etc.
        //manager.UpdateAllVariable((float)DeltaTime);
        OnUpdate(lDeltaTime);

        // -------------------------
        // 2. Fixed Update (Physics)
        // -------------------------
        while (lAccumulator >= lFixedDeltaTime)
        {
            OnFixedUpdate(lFixedDeltaTime);
            //manager.UpdateAllFixed((float)FixedDelta);
            lAccumulator -= lFixedDeltaTime;
        }

        // -------------------------
        // 3. Render (interpolated)
        // -------------------------
        // Alpha = percent between last and next physics step
        double lAlpha = lAccumulator / lFixedDeltaTime;

        OnRender(lAlpha);

        // Poll / Swap
        m_Window->Update();

        bIsRunning = !m_Window->ShouldClose();
    }
    
    OPAAX_CORE_TRACE("CoreEngineApp::Run() - Game loop ended");
}

void CoreEngineApp::Shutdown()
{
    OPAAX_CORE_TRACE("CoreEngineApp::Shutdown()");
    
    bIsRunning = false;
    
    OnShutdown();

    m_EngineSubsystemManager.ShutdownAll();

    OpaaxLog::Shutdown();
}

#if OPAAX_WITH_EDITOR
void CoreEngineApp::LaunchEditor()
{
    OPAAX_CORE_TRACE("[EDITOR MODE] Launching editor...");
    
    // TODO: Initialize editor UI
}

bool CoreEngineApp::IsDebugMode() const {
#ifdef _DEBUG
    return true;
#else
    return false;
#endif
}
#endif