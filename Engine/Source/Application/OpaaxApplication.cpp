#include "OpaaxApplication.h"

#include <cstring>

#include "Platform/CrashHandler.h"
#include "Platform/IPlatform.h"
#include "Application/Services/IPaths.h"
#include "Core/Log/Logger.h"
#include "Core/Profiling/Profiler.h"
#include "Application/Services/IProjectManager.h"
#include "Application/Services/IJobSystem.h"

#include "Engine/Config/Config_Engine.h"
#include "Engine/Engine.h"
#include "Engine/Modules/ModuleRegistrar.h"
#include "World/WorldManager.h" // GetWorldManager() is forward-declared on IEngine
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Input/InputEvents.h"
#include "Engine/Subsystems/Input/InputManager.h"

#include "Services/IConfigSystem.h"
#include "Services/IEngine.h"
#include "Window/WindowManager.h"

#include "Window/WindowEvents.h"

#include "Core/Events/Event.h"
#include "Core/Events/EventBus.h"
#include "Core/Events/EventTypes.hpp"
#include "Services/Window/IWindowManager.h"

#ifdef OPAAX_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsPlatform.h"
#endif

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
    
    //Log
    // The singleton existed all along (I1, SG); this gives it sinks and replays what it held.
    const OpaaxString lLogFile = lPath.SaveDir() + "/Log/OpaaxEngine.log";
    Logger::Get().Init(lLogFile);
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Logger just initialized");

    //Crash reporting — as early as it can know where to write (I1, SG).
    CrashHandler::Get().Install({ lPath.SaveDir() + "/Crashes", lLogFile, true });
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Platform: {}", lPlatform.GetPlatformName().CStr());
    lPath.LogPaths();
    
    //Config
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Config System");
    IConfigSystem& lConfigSystem = BootConfigSystem(lPath);
    PreRegisterConfig(lConfigSystem);
    
    //Project Manager
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Project Manager");
    IProjectManager& lProjMgr = BootProjectManager(lPath);
    
    //Jobsystem
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Job System");
    IJobSystem& lJobSystem = BootJobSystem();

    //Stats — config-driven, so after the config system.
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Stats");
    BootProfiler(lConfigSystem);

    //Window manager — the window itself is created later, in InitializeApplication (needs a GL/VK context).
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Window Manager");
    IWindowManager& lWindowMgr = BootWindowManager();
    
    //Engine
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Engine");
    IEngine& lEngine = BootEngine();
    
    OnProvideServices(m_Services);

    bHasBootstrap = true;
}

IPlatform& OpaaxApplication::BootPlatform()
{
#ifdef OPAAX_PLATFORM_WINDOWS
    //Windows
    return m_Services.Provide<IPlatform, WindowsPlatform>();
#endif
}

IPaths& OpaaxApplication::BootPaths()
{
    // Adopt whatever CreatePaths built — runtime Paths by default, EditorPaths under the editor host.
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
    // A dev build always profiles — that is what dev means, and it is the same signal IPaths keys
    // on (I12), never the editor flag: a debug GAME build is a dev build with no editor.
#if defined(OPAAX_WORKSPACE_DIR)
    constexpr bool lbDevBuild = true;
#else
    constexpr bool lbDevBuild = false;
#endif

    const bool lbEnable = lbDevBuild
                       || ConfigSystem.Get<Opaax::Config_Engine>().GetData().Stats.EnableInShipBuild;

    // Disabled is the off switch (ST6): every OPAAX_STAT_SCOPE is then one predicted branch.
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
    OPAAX_APP_LOG(Trace, "OnInitializeApplication Not override in child app class");
}

// =============================================================================
// Flow
// =============================================================================

void OpaaxApplication::RunApplication()
{
    EngineStartup();

#if defined(OPAAX_WORKSPACE_DIR)
    // Dev builds only (I12): proves the whole crash path — dump, stack, log copy, dialog.
    if (HasCommandLineFlag(m_Argc, m_Argv, "--crash-test"))
    {
        CrashHandler::TriggerTestCrash();
    }
#endif
    
    while (bIsRunning)
    {
        Window* lWindow = WindowManager().GetMainWindow();
        
        if (lWindow == nullptr)
        {
            bIsRunning = false;
            break;
        }
        
        // ----------------------------------------------------------------
        // 0a. Close the PREVIOUS frame's stats and open the next one — the same boundary, and for
        //     the same reason (IN2). The frame ending here still holds its Present, which happens
        //     after Engine().Loop() returns; publishing inside Loop would drop that row.
        // ----------------------------------------------------------------
        Profiler::Get().BeginFrame();

        // ----------------------------------------------------------------
        // 0. Close the PREVIOUS frame's input, immediately before the new events arrive.
        //
        //    This is the host loop's frame boundary, and input's boundary has to be the same one:
        //    everything that reads input — a game system in Update, an editor panel in its UI pass
        //    AFTER Engine().Loop() returns — must see the same frame's presses. Closing it inside
        //    Loop looked equivalent and was not: it wiped this frame's edges and deltas before the
        //    editor ever drew them (IN2).
        // ----------------------------------------------------------------
        Engine().GetInput().EndFrame();

        // ----------------------------------------------------------------
        // 1. windows events (input, close, etc..)
        // ----------------------------------------------------------------
        lWindow->PollEvents();

        // ----------------------------------------------------------------
        // 1.1 close event?
        // ----------------------------------------------------------------
        bIsRunning = !lWindow->ShouldClose();
        if (!bIsRunning)
        {
            break;
        }
        
        // ----------------------------------------------------------------
        // 2. Tick — via the TickFrame seam (base = Engine().Loop(); editor wraps it with UI).
        // ----------------------------------------------------------------
        TickFrame();

        // ----------------------------------------------------------------
        // 3. Present — the swap, SEPARATE from the frame render (S7 / D2). The host owns WHEN:
        //    after TickFrame, so the editor's UI (drawn inside TickFrame) is on the backbuffer
        //    before the swap. Runtime: the world was rendered straight to the backbuffer above.
        // ----------------------------------------------------------------
        Engine().PresentBackbuffer();
    }

    // The loop has stopped but nothing is destroyed yet — every service, the window and the
    // GPU context are still alive. Subsystems get their one chance here to release anything
    // that needs a live sibling; ShutdownApplication() below is too late for that.
    EngineTeardown();
}

void OpaaxApplication::TickFrame()
{
    Engine().Loop();
}

void OpaaxApplication::OnEvent(Event& InEvent)
{
    EventDispatcher lDispatcher(InEvent);

    // NOTE: category flags are a BITMASK (an input event is Input|Keyboard, Input|Mouse, ...),
    // so a single-value switch can never match a combined value — dispatch by bit-test via
    // IsInCategory. All input events carry the Input bit; window events carry Application.
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
    OPAAX_APP_LOG(Trace, "Shutdown Application");
    
    m_Services.ShutdownAll();

    Profiler::Get().Shutdown();
    CrashHandler::Get().Uninstall();

    // Last: every service above may still log on its way down.
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
    OPAAX_APP_LOG(Info, "OpaaxApplication::EngineStartup ----> Pre Engine Startup.....");
    PreEngineStartup();

    // 1. Infrastructure. Every subsystem is constructed and started — and NO world exists,
    //    which is what leaves ComponentRegistry unsealed for the steps below.
    Engine().Startup();

    // 2. Content types.
        // 2.1 Engine First
    PopulateEngineRegistries();
        //2.2 
    RegisterModules(*m_ModuleRegistrar);
        //2.3 
    OnModulesRegistered();

    // 3. The game. A Play startup world means this host IS a game, so its whole run is one
    //    session. The editor's startup world is an Edit world and gets NO game — PlayInEditor
    //    brackets one per PIE cycle instead.
    //    BEFORE the world, always: a world subsystem's context is built inside CreateWorld.
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
    // ONE call, deliberately: binding each route by hand here is how a new registry ships with a
    // route nobody wired — which is what happened to Resources(). BindEngineRegistries is the seam
    // MR0 built for this, and it grows with the aggregate rather than with this function.
    m_ModuleRegistrar->BindEngineRegistries(Engine().GetRegistries());
}

WorldSpec OpaaxApplication::GetStartupWorldSpec() const
{
    WorldSpec lSpec;

    lSpec.LevelPath = GetAppService<IProjectManager>().StartupLevel();
    lSpec.Mode      = EWorldMode::Play;

    return lSpec;
}

void OpaaxApplication::EngineTeardown()
{
    // Mirrors EngineStartup: the game ends before the engine is torn down, and it takes its
    // Play worlds with it. Unconditional — a silent no-op when this host never started one.
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
        // The engine cannot do this itself: window events reach it only through this feed, and going
        // via the bus would defer the reset to the next Flush — inside Loop, a frame late (IN4/IN5).
        Engine().GetInput().ResetState();
        return false;
    });
}

void OpaaxApplication::HandleAllInputEvent(EventDispatcher& Dispatcher, Event& InEvent)
{
    // The host feeds the engine because the host owns the only gate (IN1), and the feed must be
    // immediate: the bus flushes inside Loop, i.e. AFTER PollEvents, so a subscription is too late (IN4).
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

    // NOTE: KeyTypedEvent is deliberately NOT fed. A Unicode codepoint is text entry, not a key
    // state — it has no "down" to hold. It belongs to whatever owns a text field (ImGui today).
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