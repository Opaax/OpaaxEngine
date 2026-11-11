#include "Application/OpaaxApplication.h"

#include "Core/Build/BuildInfo.h"
#include "Core/Maths/Maths.h"
#include "Core/String/CString.h"

#include "Automation/AutomationSession.h"
#include "Automation/EngineAutomationCommands.h"
#include "Commands/CommandRegistry.h"
#include "Platform/CrashHandler.h"
#include "Platform/IPlatform.h"
#include "Application/Services/IPaths.h"
#include "Core/Log/Logger.h"
#include "Core/Profiling/Profiler.h"
#include "Application/Services/IProjectManager.h"
#include "Application/Services/IJobSystem.h"
#include "Application/Services/IOnlineServices.h"
#include "Online/Config/Config_Online.h"
#include "Online/OnlineAPI.h"
#include "Platform/Process.h"
#include "Application/Services/ISaveStorage.h"
#include "Application/Services/IUserSettings.h"

#include "Engine/Config/Config_Engine.h"
#include "Engine/EngineEvents.h"
#include "Engine/Engine.h"
#include "Engine/FramePacer.h"
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
    /** Frames drawn before a --capture is taken, when --capture-frame does not say. */
    constexpr Uint64 DEFAULT_CAPTURE_FRAME = 60;
}

AppServiceLocator OpaaxApplication::m_Services = AppServiceLocator();

// =============================================================================
// CTORS - DTORS
// =============================================================================

OpaaxApplication::OpaaxApplication(int InArgc, char** InArgv)
    : m_CommandLine(CommandLine::FromMain(InArgc, InArgv))
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
    
    //Log — adds the sinks and replays the messages logged so far. In the player's folder, with the
    //saves: a ship build may not be able to write where it was installed.
    const OpaaxString lLogFile = lPath.UserDir() + "/Log/OpaaxEngine.log";
    Logger::Get().Init(lLogFile);

    //Crash reporting — as soon as we know where to write.
    CrashHandler::Get().Install({ lPath.UserDir() + "/Crashes", lLogFile, true });
    lPath.LogPaths();

    // Before anything reads the project's files.
    OnPathsResolved(lPath, m_CommandLine);

    //Config
    IConfigSystem& lConfigSystem = BootConfigSystem(lPath);
    PreRegisterConfig(lConfigSystem);
    
    //Project Manager
    IProjectManager& lProjMgr = BootProjectManager(lPath);
    
    //Jobsystem
    IJobSystem& lJobSystem = BootJobSystem();

    //Save storage — the player's saves, in the user folder.
    ISaveStorage& lSaveStorage = BootSaveStorage(lPath);

    //An app that starts its game at once is that game from the start: its window is the game's. The
    //editor starts in Edit. Asked once the project is read (its startup level).
    m_bStartsGame = GetStartupWorldSpec().Mode == EWorldMode::Play;

    //Online services — the player's store, or none: the game plays the same without one.
    BootOnlineServices(lConfigSystem);

    //Window manager — the window itself is created in InitializeApplication.
    IWindowManager& lWindowMgr = BootWindowManager();

    //User settings — the player's values over the configs, from the save storage. Before the window:
    //a game's opens at the player's size.
    BootUserSettings(lSaveStorage, lConfigSystem, lWindowMgr);

    //Stats — reads the config, so after the config system.
    BootProfiler(lConfigSystem);

    //Commands — the engine's subsystems take the registry into their contexts.
    BootCommands();

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
    return m_Services.ProvideInstance<IPaths>(CreatePaths(Platform(), m_CommandLine));
}

TUniquePtr<IPaths> OpaaxApplication::CreatePaths(const IPlatform& InPlatform, const CommandLine& InCommandLine)
{
    return MakeUnique<Opaax::Paths>(InPlatform, InCommandLine);
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

ISaveStorage& OpaaxApplication::BootSaveStorage(const IPaths& Paths)
{
    return m_Services.Provide<ISaveStorage, Opaax::SaveStorage>(Paths);
}

IOnlineServices& OpaaxApplication::BootOnlineServices(IConfigSystem& ConfigSystem)
{
    const OnlineConfigData& lConfig = ConfigSystem.Get<Config_Online>().GetData();

    OnlineLaunch lLaunch;
    lLaunch.CommandLine      = m_CommandLine.GetValue("--online");
    lLaunch.bEditor          = !m_bStartsGame;
    lLaunch.bLaunchedBySteam = Process::GetEnvironment("SteamGameId").has_value();
    lLaunch.BuiltIn          = OnlineAPI::GetBuiltInBackends();

    const OnlineBackendChoice lChoice = ChooseOnlineBackends(lConfig.Backends, lLaunch);

    OpaaxString lReason;
    TUniquePtr<IOnlineBackend> lBackend = OnlineAPI::CreateFirstBackend(lChoice, lConfig, lReason);

    IOnlineServices& lOnline = m_Services.Provide<IOnlineServices, Opaax::OnlineServices>(Move(lBackend), Move(lReason));
    lOnline.SetLeaderboards(ToLeaderboardDefinitions(lConfig));
    return lOnline;
}

IUserSettings& OpaaxApplication::BootUserSettings(ISaveStorage& SaveStorage, IConfigSystem& ConfigSystem,
                                                  const IWindowManager& WindowManager)
{
    IUserSettings& lSettings = m_Services.Provide<IUserSettings, Opaax::UserSettings>(SaveStorage, ConfigSystem,
                                                                                     WindowManager);

    // The editor's window and Config panel keep the project's values; each Play reads the player's
    // (Engine::StartGame).
    if (m_bStartsGame)
    {
        lSettings.Load();
        lSettings.Activate();
    }
    return lSettings;
}

void OpaaxApplication::BootProfiler(IConfigSystem& ConfigSystem)
{
    // Dev builds always profile (editor or not).
    const bool lbEnable = Build::IsDevBuild()
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

ICommandRegistry& OpaaxApplication::BootCommands()
{
    return m_Services.Provide<ICommandRegistry, Opaax::CommandRegistry>();
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

    // A game's window takes the player's mode and size as they change; the editor's keeps its own.
    if (m_bStartsGame)
    {
        WindowManager().FollowEngineConfig();
    }

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
    // --exec <script.json> [--exec-out <answers.json>], --automation <folder>: driven from outside
    // (AutomationSession). Made before the engine starts, so the app knows from the start whether a
    // script owns the input. A script that cannot be run fails the run instead of starting it.
    const OpaaxString* lScript = m_CommandLine.FindValue("--exec");
    const OpaaxString* lInbox  = m_CommandLine.FindValue("--automation");
    m_Automation = AutomationSession::Create(m_CommandLine.GetValue("--exec"), m_CommandLine.GetValue("--exec-out"),
                                             m_CommandLine.GetValue("--automation"), Commands());

    EngineStartup();

    // Dev builds only: tests the crash path (dump, stack, log copy, dialog).
    if (Build::IsDevBuild() && m_CommandLine.HasFlag("--crash-test"))
    {
        CrashHandler::TriggerTestCrash();
    }

    // --frame-pacing <pacing>: the frames get the time of a screen rate instead of the clock's
    // ("144,jitter=0.25", FramePacing::Parse). A pacing that does not read fails the run.
    if (const OpaaxString* lPacingText = m_CommandLine.FindValue("--frame-pacing"))
    {
        FramePacing lPacing;
        OpaaxString lError;
        if (FramePacing::Parse(*lPacingText, lPacing, lError))
        {
            Engine().SetFramePacing(lPacing);
        }
        else
        {
            OPAAX_APP_LOG(Error, "--frame-pacing '{}': {}", lPacingText->CStr(), lError.CStr());
            SetExitCode(1);
            bIsRunning = false;
        }
    }

    // --capture <file.png> [--capture-frame N]: saves frame N (default 60) as a PNG, then quits.
    // What an automated check or an agent uses to see the game.
    const OpaaxString* lCapturePath  = m_CommandLine.FindValue("--capture");
    const OpaaxString* lCaptureFrame = m_CommandLine.FindValue("--capture-frame");
    const Uint64       lCaptureAt    = (lCaptureFrame != nullptr) ? CString::ToUint64(lCaptureFrame->CStr())
                                                                  : DEFAULT_CAPTURE_FRAME;

    if (m_Automation == nullptr && (lScript != nullptr || lInbox != nullptr))
    {
        SetExitCode(1);
        bIsRunning = false;
    }

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
        // 1.2 Automation: the requests due this frame, before anything ticks.
        // ----------------------------------------------------------------
        ++m_FrameIndex;
        if (m_Automation != nullptr)
        {
            m_Automation->BeginFrame();
        }

        // ----------------------------------------------------------------
        // 2. Tick (the editor adds its UI around Engine().Loop())
        // ----------------------------------------------------------------
        TickFrame();

        // ----------------------------------------------------------------
        // 2.1 Capture, before the swap (the backbuffer holds the frame).
        // ----------------------------------------------------------------
        if (lCapturePath != nullptr && m_FrameIndex == Maths::Max<Uint64>(lCaptureAt, 1))
        {
            Engine().CaptureFrame(*lCapturePath);
            lWindow->RequestClose();
        }

        // ----------------------------------------------------------------
        // 2.2 Commands: a screenshot asked for, a quit, the end of a script.
        // ----------------------------------------------------------------
        if (!m_PendingScreenshot.IsEmpty())
        {
            Engine().CaptureFrame(m_PendingScreenshot);
            m_PendingScreenshot.Clear();
        }

        const bool bScriptDone = m_Automation != nullptr && m_Automation->EndFrame();
        if (m_bQuitRequested || bScriptDone)
        {
            lWindow->RequestClose();
        }

        // ----------------------------------------------------------------
        // 3. Present — after TickFrame, so the editor UI is on the backbuffer before the swap.
        // ----------------------------------------------------------------
        Engine().PresentBackbuffer();
    }

    // Before the engine ends: what the app closed on is answered while the world is still there.
    if (m_Automation != nullptr)
    {
        m_Automation->Close();
        SetExitCode(Maths::Max(GetExitCode(), m_Automation->GetExitCode()));
        m_Automation.reset();
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

    // The engine's commands, before any world: its subsystems may refine them.
    EngineAutomation::Register(Commands(), Engine(), *this);

    // A cheat that ran is told to the game at once, on the engine bus.
    Commands().SetCheatListener([this](const CheatUsed& InCheat)
    {
        Engine().GetEngineEventBus().GetEventBus().Publish(InCheat);
    });

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

    // The app's commands, once it is up (the editor's need its context).
    OnRegisterCommands(Commands());
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

    // Its bus is going: a cheat is no longer told to anyone.
    Commands().SetCheatListener(nullptr);

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
        // Reset now: going through the event bus would be a frame late. A script's keys do not depend
        // on the window's focus.
        if (!IsInputScripted())
        {
            Engine().GetInput().ResetState();
        }
        return false;
    });
}

bool OpaaxApplication::IsInputScripted() const noexcept
{
    return m_Automation != nullptr && m_Automation->OwnsInput();
}

void OpaaxApplication::HandleAllInputEvent(EventDispatcher& Dispatcher, Event& InEvent)
{
    // A script's input is the only input.
    if (IsInputScripted())
    {
        return;
    }

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

    // Text entry, apart from the keys: the game's own text fields (the console) read it.
    Dispatcher.Dispatch<KeyTypedEvent>([&lInput](KeyTypedEvent& InTyped)
    {
        lInput.OnCharTyped(InTyped.GetPayload().Codepoint);
        return false;
    });
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
ISaveStorage&       OpaaxApplication::SaveStorage()     { return m_Services.Get<ISaveStorage>();    }
IUserSettings&      OpaaxApplication::UserSettings()    { return m_Services.Get<IUserSettings>();   }
IOnlineServices&    OpaaxApplication::Online()          { return m_Services.Get<IOnlineServices>(); }
IWindowManager&     OpaaxApplication::WindowManager()   { return m_Services.Get<IWindowManager>();  }
ICommandRegistry&   OpaaxApplication::Commands()        { return m_Services.Get<ICommandRegistry>(); }
IEngine&            OpaaxApplication::Engine()          { return m_Services.Get<IEngine>();         }
