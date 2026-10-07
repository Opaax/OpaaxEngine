#include "Application/OpaaxApplication.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "Platform/CrashHandler.h"
#include "Platform/IPlatform.h"
#include "Application/Services/IPaths.h"
#include "Core/Log/Logger.h"
#include "Core/Profiling/Profiler.h"
#include "Application/Services/IProjectManager.h"
#include "Application/Services/IJobSystem.h"

#include "Engine/Config/Config_Engine.h"
#include "Engine/EngineEvents.h"
#include "Engine/Engine.h"
#include "Engine/Registries/ModuleRegistrar.h"
#include "World/WorldManager.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputEvents.h"
#include "Input/InputManager.h"

#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IEngine.h"
#include "Platform/Window/WindowManager.h"

#include "Platform/Window/WindowEvents.h"

#include "Core/Events/Event.h"
#include "Core/Events/EventBus.h"
#include "Core/Events/EventTypes.hpp"
#include "Application/Services/IWindowManager.h"

#include "Platform/NativePlatform.h"

using namespace Opaax;

namespace
{
    bool HasCommandLineFlag(const int InArgc, char** InArgv, const char* InFlag)
    {
        for (int i = 1; i < InArgc; ++i)
        {
            if (InArgv[i] != nullptr && std::strcmp(InArgv[i], InFlag) == 0) { return true; }
        }
        return false;
    }

    /** The argument after InFlag, or null. */
    const char* FindCommandLineValue(const int InArgc, char** InArgv, const char* InFlag)
    {
        for (int i = 1; i + 1 < InArgc; ++i)
        {
            if (InArgv[i] != nullptr && std::strcmp(InArgv[i], InFlag) == 0) { return InArgv[i + 1]; }
        }
        return nullptr;
    }

    /** Frames drawn before a --capture is taken, when --capture-frame does not say. */
    constexpr Uint64 DEFAULT_CAPTURE_FRAME = 60;
}

AppServiceLocator OpaaxApplication::m_Services = AppServiceLocator();

// =============================================================================
// CTORS - DTORS
// =============================================================================

OpaaxApplication::OpaaxApplication(int InArgc, char** InArgv)
    : m_Argc(InArgc)
    , m_Argv(InArgv)
    , m_ModuleRegistrar(MakeUnique<ModuleRegistrar>())
{
}

OpaaxApplication::~OpaaxApplication()
{
    if (!bHasShutdown)
    {
        ShutdownApplication();
    }
}

// =============================================================================
// Bootstrap
// =============================================================================

void OpaaxApplication::Bootstrap()
{
    // Platform
    IPlatform& lPlatform = BootPlatform();
    
    //Path
    IPaths& lPath = BootPaths();
    
    //Log — adds the sinks and replays the messages logged so far.
    const OpaaxString lLogFile = lPath.SaveDir() + "/Log/OpaaxEngine.log";
    Logger::Get().Init(lLogFile);

    //Crash reporting — as soon as we know where to write.
    CrashHandler::Get().Install({ lPath.SaveDir() + "/Crashes", lLogFile, true });
    lPath.LogPaths();
    
    //Config
    IConfigSystem& lConfigSystem = BootConfigSystem(lPath);
    PreRegisterConfig(lConfigSystem);
    
    //Project Manager
    IProjectManager& lProjMgr = BootProjectManager(lPath);
    
    //Jobsystem
    IJobSystem& lJobSystem = BootJobSystem();

    //Stats — reads the config, so after the config system.
    BootProfiler(lConfigSystem);

    //Window manager — the window itself is created in InitializeApplication.
    IWindowManager& lWindowMgr = BootWindowManager();
    
    //Engine
    IEngine& lEngine = BootEngine();
    
    OnProvideServices(m_Services);

    bHasBootstrap = true;
}

IPlatform& OpaaxApplication::BootPlatform()
{
    return m_Services.Provide<IPlatform, NativePlatform>();
}

IPaths& OpaaxApplication::BootPaths()
{
    return m_Services.ProvideInstance<IPaths>(CreatePaths(Platform(), m_Argc, m_Argv));
}

TUniquePtr<IPaths> OpaaxApplication::CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv)
{
    return MakeUnique<Opaax::Paths>(InPlatform, InArgc, InArgv);
}

IConfigSystem& OpaaxApplication::BootConfigSystem(const IPaths& Paths)
{
    return m_Services.Provide<IConfigSystem, Opaax::ConfigSystem>(Paths);
}

IProjectManager& OpaaxApplication::BootProjectManager(const IPaths& Paths)
{
    return m_Services.Provide<IProjectManager, Opaax::ProjectManager>(Paths);
}

IJobSystem& OpaaxApplication::BootJobSystem()
{
    return m_Services.Provide<IJobSystem, Opaax::JobSystem>();
}

void OpaaxApplication::BootProfiler(IConfigSystem& ConfigSystem)
{
    // Dev builds always profile (editor or not).
#if defined(OPAAX_WORKSPACE_DIR)
    constexpr bool lbDevBuild = true;
#else
    constexpr bool lbDevBuild = false;
#endif

    const bool lbEnable = lbDevBuild
                       || ConfigSystem.Get<Opaax::Config_Engine>().GetData().Stats.EnableInShipBuild;

    // When disabled, OPAAX_STAT_SCOPE costs a single branch.
    Profiler::Get().Init(lbEnable);

    if (!lbEnable)
    {
        OPAAX_APP_LOG(Info, "Stats DISABLED (ship build; set Stats.EnableInShipBuild to profile)");
    }
}

IWindowManager& OpaaxApplication::BootWindowManager()
{
    return m_Services.Provide<IWindowManager, Opaax::WindowManager>();
}

IEngine& OpaaxApplication::BootEngine()
{
    return m_Services.Provide<IEngine, Opaax::Engine>();
}

void OpaaxApplication::PreRegisterConfig(IConfigSystem& ConfigSystem)
{
    if (ConfigSystem.IsNull())
    {
        OPAAX_APP_LOG(Error, "Pre register config with a null config system");
        return;
    }
    
    ConfigSystem.Register<Opaax::Config_Engine>();
}

// =============================================================================
// Initialization
// =============================================================================

void OpaaxApplication::InitializeApplication()
{
    CreateApplicationWindow();
    
    OnInitializeApplication();
    
    bHasInitialized = true;
    bIsRunning      = true;
}

void OpaaxApplication::CreateApplicationWindow()
{
    WindowManager().CreateMainWindow();
    
    if (Window* lWindow = WindowManager().GetMainWindow())
    {
        lWindow->SetEventCallback([this](Event& InEvent)
        {
            OnEvent(InEvent);
        });
    }
}

void OpaaxApplication::OnInitializeApplication()
{
}

// =============================================================================
// Flow
// =============================================================================

void OpaaxApplication::RunApplication()
{
    EngineStartup();

#if defined(OPAAX_WORKSPACE_DIR)
    // Dev builds only: tests the crash path (dump, stack, log copy, dialog).
    if (HasCommandLineFlag(m_Argc, m_Argv, "--crash-test"))
    {
        CrashHandler::TriggerTestCrash();
    }
#endif

    // --capture <file.png> [--capture-frame N]: saves frame N (default 60) as a PNG, then quits.
    // What an automated check or an agent uses to see the game.
    const char*  lCapturePath  = FindCommandLineValue(m_Argc, m_Argv, "--capture");
    const char*  lCaptureFrame = FindCommandLineValue(m_Argc, m_Argv, "--capture-frame");
    const Uint64 lCaptureAt    = (lCaptureFrame != nullptr) ? std::strtoull(lCaptureFrame, nullptr, 10) : DEFAULT_CAPTURE_FRAME;
    Uint64       lFrameIndex   = 0;

    while (bIsRunning)
    {
        Window* lWindow = WindowManager().GetMainWindow();
        
        if (lWindow == nullptr)
        {
            bIsRunning = false;
            break;
        }
        
        // ----------------------------------------------------------------
        // 0a. Close the previous frame's stats (includes its Present) and open a new frame.
        // ----------------------------------------------------------------
        Profiler::Get().BeginFrame();

        // ----------------------------------------------------------------
        // 0. Close the previous frame's input, right before new events arrive.
        //    Done here, not in Loop, so the editor UI sees the same input as the game.
        // ----------------------------------------------------------------
        Engine().GetInput().EndFrame();

        // ----------------------------------------------------------------
        // 1. Window events (input, close, ...)
        // ----------------------------------------------------------------
        lWindow->PollEvents();

        // ----------------------------------------------------------------
        // 1.1 Close requested?
        // ----------------------------------------------------------------
        bIsRunning = !lWindow->ShouldClose();
        if (!bIsRunning)
        {
            break;
        }
        
        // ----------------------------------------------------------------
        // 2. Tick (the editor adds its UI around Engine().Loop())
        // ----------------------------------------------------------------
        TickFrame();

        // ----------------------------------------------------------------
        // 2.1 Capture, before the swap (the backbuffer holds the frame).
        // ----------------------------------------------------------------
        ++lFrameIndex;
        if (lCapturePath != nullptr && lFrameIndex == std::max<Uint64>(lCaptureAt, 1))
        {
            Engine().CaptureFrame(OpaaxString(lCapturePath));
            lWindow->RequestClose();
        }

        // ----------------------------------------------------------------
        // 3. Present — after TickFrame, so the editor UI is on the backbuffer before the swap.
        // ----------------------------------------------------------------
        Engine().PresentBackbuffer();
    }

    // Services, window and GPU context are still alive here, so subsystems can
    // release what depends on them.
    EngineTeardown();
}

void OpaaxApplication::TickFrame()
{
    Engine().Loop();
}

void OpaaxApplication::OnEvent(Event& InEvent)
{
    EventDispatcher lDispatcher(InEvent);

    // Categories are a bitmask (Input|Keyboard, ...), so test bits instead of switching.
    if (InEvent.IsInCategory(EEventCategory::Application))
    {
        HandleApplicationEvent(lDispatcher, InEvent);
    }
    else if (InEvent.IsInCategory(EEventCategory::Input))
    {
        HandleAllInputEvent(lDispatcher, InEvent);
    }
    else
    {
        UnknownEvent(lDispatcher, InEvent);
    }
}

void OpaaxApplication::ShutdownApplication()
{
    m_Services.ShutdownAll();

    Profiler::Get().Shutdown();
    CrashHandler::Get().Uninstall();

    // Last: services may still log while shutting down.
    Logger::Get().Shutdown();

    bHasShutdown    = true;
    bHasBootstrap   = false;
    bHasInitialized = false;
    bIsRunning      = false;
}

// =============================================================================
// Engine
// =============================================================================

void OpaaxApplication::EngineStartup()
{
    PreEngineStartup();

    // 1. Start every subsystem. No world exists yet, so the registries stay open.
    Engine().Startup();

    // Gameplay may ask to quit; the host decides what quitting means.
    Engine().GetEngineEventBus().GetEventBus().Subscribe<QuitGameRequested>(
        [this](const QuitGameRequested&) { OnQuitGameRequested(); });

    // 2. Content types: engine first, then the game modules.
    PopulateEngineRegistries();
    RegisterModules(*m_ModuleRegistrar);
    OnModulesRegistered();

    // 3. The game. Only a Play startup world starts it here; the editor starts one per Play session.
    //    Must happen before the world is created.
    const WorldSpec lStartupSpec = GetStartupWorldSpec();

    if (lStartupSpec.Mode == EWorldMode::Play)
    {
        Engine().StartGame();
    }

    // 4. Content.
    Engine().FinishStartup(lStartupSpec);

    PostEngineStartup();
}

void OpaaxApplication::PopulateEngineRegistries()
{
    m_ModuleRegistrar->BindEngineRegistries(Engine().GetRegistries());
}

WorldSpec OpaaxApplication::GetStartupWorldSpec() const
{
    WorldSpec lSpec;

    lSpec.LevelPath = GetAppService<IProjectManager>().StartupLevel();
    lSpec.Mode      = EWorldMode::Play;

    return lSpec;
}

void OpaaxApplication::OnQuitGameRequested()
{
    OPAAX_APP_LOG(Info, "Quit requested by the game");

    if (Window* lWindow = WindowManager().GetMainWindow())
    {
        lWindow->RequestClose();
    }
}

void OpaaxApplication::EngineTeardown()
{
    // End the game (and its Play worlds) before the engine. No-op if no game was started.
    Engine().EndGame();

    Engine().TearDown();
}



void OpaaxApplication::HandleApplicationEvent(EventDispatcher& Dispatcher, Event& InEvent)
{
    Dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&)
    {
        OPAAX_APP_LOG(Info, "WindowCloseEvent - requesting shutdown");
        bIsRunning = false;
        return true;
    });
    
    Dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& InResize)
    {
        Engine().GetEngineEventBus().GetEventBus().Enqueue(InResize.GetPayload());
        return false;
    });

    Dispatcher.Dispatch<WindowLostFocusEvent>([this](WindowLostFocusEvent&)
    {
        // Reset now: going through the event bus would be a frame late.
        Engine().GetInput().ResetState();
        return false;
    });
}

void OpaaxApplication::HandleAllInputEvent(EventDispatcher& Dispatcher, Event& InEvent)
{
    // Fed directly: the event bus flushes inside Loop, which would be too late.
    InputManager& lInput = Engine().GetInput();

    Dispatcher.Dispatch<KeyPressedEvent>([&lInput](KeyPressedEvent& InKey)
    {
        lInput.OnKeyPressed(InKey.GetKeyCode(), InKey.IsRepeat());
        return false;
    });

    Dispatcher.Dispatch<KeyReleasedEvent>([&lInput](KeyReleasedEvent& InKey)
    {
        lInput.OnKeyReleased(InKey.GetKeyCode());
        return false;
    });

    Dispatcher.Dispatch<MouseButtonPressedEvent>([&lInput](MouseButtonPressedEvent& InButton)
    {
        lInput.OnMouseButtonPressed(InButton.GetMouseButton());
        return false;
    });

    Dispatcher.Dispatch<MouseButtonReleasedEvent>([&lInput](MouseButtonReleasedEvent& InButton)
    {
        lInput.OnMouseButtonReleased(InButton.GetMouseButton());
        return false;
    });

    Dispatcher.Dispatch<MouseMovedEvent>([&lInput](MouseMovedEvent& InMove)
    {
        lInput.OnMouseMoved(InMove.GetX(), InMove.GetY());
        return false;
    });

    Dispatcher.Dispatch<MouseScrolledEvent>([&lInput](MouseScrolledEvent& InScroll)
    {
        lInput.OnMouseScrolled(InScroll.GetXOffset(), InScroll.GetYOffset());
        return false;
    });

    // KeyTypedEvent is text entry, not key state: left to text fields (ImGui).
    (void)InEvent;
}

void OpaaxApplication::UnknownEvent(EventDispatcher& Dispatcher, Event& InEvent)
{
    
}

// =============================================================================
// Getters
// =============================================================================

IPlatform&          OpaaxApplication::Platform()        { return m_Services.Get<IPlatform>();       }
IPaths&             OpaaxApplication::Paths()           { return m_Services.Get<IPaths>();          }
IProjectManager&    OpaaxApplication::ProjectManager()  { return m_Services.Get<IProjectManager>(); }
IConfigSystem&      OpaaxApplication::ConfigSystem()    { return m_Services.Get<IConfigSystem>();   }
IJobSystem&         OpaaxApplication::JobSystem()       { return m_Services.Get<IJobSystem>();      }
IWindowManager&     OpaaxApplication::WindowManager()   { return m_Services.Get<IWindowManager>();  }
IEngine&            OpaaxApplication::Engine()          { return m_Services.Get<IEngine>();         }
