#include "OpaaxApplication.h"

#include "Application/Services/Platforms/IPlatform.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/ILogger.h"
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
#include "Services/Window/IWindowManager.h"
#include "Services/Window/WindowManager.h"

#include "Core/Window/WindowEvents.h"

#include "Core/Events/Event.h"
#include "Core/Events/EventBus.h"
#include "Core/Events/EventTypes.hpp"

#ifdef OPAAX_PLATFORM_WINDOWS
#include "Application/Services/Platforms/Windows/WindowsPlatform.h"
#endif

using namespace Opaax;

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
    ILogger& lLogger = BootLogger(lPath);
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Logger just initialized");
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

UniquePtr<IPaths> OpaaxApplication::CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv)
{
    return MakeUnique<Opaax::Paths>(InPlatform, InArgc, InArgv);
}

ILogger& OpaaxApplication::BootLogger(IPaths& Paths)
{
    return m_Services.Provide<ILogger, Opaax::Logger>(Paths);
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
        OPAAX_APP_LOG(Error, "Pre register config with a null config system")
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
    
    while (bIsRunning)
    {
        Window* lWindow = WindowManager().GetMainWindow();
        
        if (lWindow == nullptr)
        {
            bIsRunning = false;
            break;
        }
        
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
    OPAAX_APP_LOG(Trace, "Shutdown Application")
    
    m_Services.ShutdownAll();

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

    // 1. Infrastructure. Every subsystem is constructed and started — and NO world exists,
    //    which is what leaves ComponentRegistry unsealed for the steps below.
    Engine().Startup();

    // 2. Content types. The registries are live; nothing has locked them yet (MR2).
    m_ModuleRegistrar->BindEngineRegistries(Engine().GetRegistries());
    RegisterModules(*m_ModuleRegistrar);
    OnModulesRegistered();

    // 3. Content. The first CreateWorld seals the registries on its way through, so this must
    //    come last — and being last is exactly why the registration above had room to happen.
    //    The host only says WHICH; the engine creates it (BO4).
    Engine().FinishStartup(GetStartupWorldSpec());

    PostEngineStartup();
}

WorldSpec OpaaxApplication::GetStartupWorldSpec() const
{
    WorldSpec lSpec;

    // The project decides, not the engine: <Name>.opaaxproj carries the startup level, as an
    // ASSET-RELATIVE path ("Levels/Main.opaaxlevel"). Empty (or no project file) means no level
    // to open — a bare host still boots, into an empty world.
    lSpec.LevelPath = GetAppService<IProjectManager>().StartupLevel();

    // The NAME is derived from the path, not the path itself. Until M5 the raw value went
    // straight into Name, which only ever worked because it was empty — a project that names its
    // level would otherwise have produced a world called "Levels/Main.opaaxlevel".
    lSpec.Name = DeriveWorldName(lSpec.LevelPath);

    // A runtime host plays. The editor overrides this to Edit.
    lSpec.Mode = EWorldMode::Play;

    return lSpec;
}

OpaaxString OpaaxApplication::DeriveWorldName(const OpaaxString& InLevelPath)
{
    // "Levels/Main.opaaxlevel" -> "Main". Both separators are handled: the engine writes forward
    // slashes everywhere, but a hand-edited .opaaxproj on Windows may well carry backslashes.
    if (InLevelPath.IsEmpty())
    {
        return OpaaxString("Main");   // the pre-M5 fallback, unchanged
    }

    const std::string lPath(InLevelPath.CStr());

    const size_t lSlash = lPath.find_last_of("/\\");
    const size_t lStart = (lSlash == std::string::npos) ? 0 : lSlash + 1;

    const size_t lDot = lPath.find_last_of('.');
    const size_t lEnd = (lDot == std::string::npos || lDot < lStart) ? lPath.size() : lDot;

    const std::string lStem = lPath.substr(lStart, lEnd - lStart);

    // A path that is nothing but a directory or an extension leaves no stem to use.
    return lStem.empty() ? OpaaxString("Main") : OpaaxString(lStem.c_str());
}

void OpaaxApplication::EngineTeardown()
{
    Engine().TearDown();
}

void OpaaxApplication::HandleApplicationEvent(EventDispatcher& Dispatcher, Event& InEvent)
{
    Dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&)
    {
        OPAAX_APP_LOG(Info, "WindowCloseEvent - requesting shutdown")
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
        // The OS delivers the RELEASE to whoever has focus, and that is no longer us — so a key
        // held right now would stay held forever. This is the runtime's only route-closing event
        // (the editor has three more); without it, alt-tabbing mid-move leaves the player walking
        // into a wall (D5's stuck-key contract).
        Engine().GetInput().ResetState();
        return false;
    });
}

void OpaaxApplication::HandleAllInputEvent(EventDispatcher& Dispatcher, Event& InEvent)
{
    // The engine end of D5's route. Reaching here at all IS the routing decision: a host that
    // wants to withhold input (the editor, when the world is in Edit mode or the viewport has no
    // focus) consumes the event in OnEvent and never calls up. So there is no gate to re-check.
    //
    // Fed IMMEDIATELY rather than through the event bus: this runs inside PollEvents, before the
    // frame ticks, so anything asking mid-route ("is Shift held?") gets this frame's truth. A
    // queued feed would answer with last frame's state.
    //
    // Every handler returns false — feeding is observation, not consumption, and a later
    // subscriber must still see the event.
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
ILogger&            OpaaxApplication::Logger()          { return m_Services.Get<ILogger>();         }
IProjectManager&    OpaaxApplication::ProjectManager()  { return m_Services.Get<IProjectManager>(); }
IConfigSystem&      OpaaxApplication::ConfigSystem()    { return m_Services.Get<IConfigSystem>();   }
IJobSystem&         OpaaxApplication::JobSystem()       { return m_Services.Get<IJobSystem>();      }
IWindowManager&     OpaaxApplication::WindowManager()   { return m_Services.Get<IWindowManager>();  }
IEngine&            OpaaxApplication::Engine()          { return m_Services.Get<IEngine>();         }