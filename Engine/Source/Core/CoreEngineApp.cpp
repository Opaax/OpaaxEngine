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
 
    if (!m_Window)
    {
        OPAAX_CORE_ERROR("CoreEngineApp::Run() — window is null, aborting...");
        Shutdown();
        return;
    }
 
    OPAAX_CORE_TRACE("CoreEngineApp::Run() — entering game loop");

    // NOTE: Fixed timestep accumulator pattern.
    //   Physics/logic ticks at a stable 60 Hz regardless of render frame rate.
    //   Render receives the interpolation alpha so it can lerp between physics steps.
    constexpr double FIXED_DELTA_TIME    = 1.0 / 60.0;
    constexpr double MAX_FRAME_DELTA     = 0.25;  // spiral-of-death guard
 
    double lAccumulator = 0.0;
    double lLastTime    = glfwGetTime();
    
    while (bIsRunning)
    {
        // ----------------------------------------------------------------
        // 1. windows events (input, close, etc..)
        // ----------------------------------------------------------------
        m_Window->PollEvents();

        // ----------------------------------------------------------------
        // 1.1 close event called?
        // ----------------------------------------------------------------
        bIsRunning = !m_Window->ShouldClose();
        if (!bIsRunning)
        {
            break;
        }

        // ----------------------------------------------------------------
        // 2. Time
        // ----------------------------------------------------------------
        const double lTimeNow   = glfwGetTime();
        double       lDeltaTime = lTimeNow - lLastTime;
        lLastTime               = lTimeNow;
 
        if (lDeltaTime > MAX_FRAME_DELTA) { lDeltaTime = MAX_FRAME_DELTA; }
 
        lAccumulator += lDeltaTime;
        
        // ----------------------------------------------------------------
        // 2.1 Variable update — gameplay, animations, AI
        // ----------------------------------------------------------------
        m_EngineSubsystemManager.UpdateAll(lDeltaTime);
        OnUpdate(lDeltaTime);

        // ----------------------------------------------------------------
        // 2.2 Fixed update — physics, at stable 60 Hz
        // ----------------------------------------------------------------
        while (lAccumulator >= FIXED_DELTA_TIME)
        {
            m_EngineSubsystemManager.FixedUpdateAll(FIXED_DELTA_TIME);
            OnFixedUpdate(FIXED_DELTA_TIME);
            lAccumulator -= FIXED_DELTA_TIME;
        }

        // ----------------------------------------------------------------
        // 2.3 Render — interpolated between last and next physics step
        // ----------------------------------------------------------------
        const double lAlpha = lAccumulator / FIXED_DELTA_TIME;
 
        m_EngineSubsystemManager.RenderAll(lAlpha);
        OnRender(lAlpha);
        
        // ----------------------------------------------------------------
        // 3. Swap AFTER render, always last
        // ----------------------------------------------------------------
        m_Window->SwapBuffers();
    }
    
    OPAAX_CORE_TRACE("CoreEngineApp::Run() — game loop exited");

    Shutdown();
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