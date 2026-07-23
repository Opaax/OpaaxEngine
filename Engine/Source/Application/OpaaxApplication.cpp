#include "OpaaxApplication.h"

#include "Application/Services/Platforms/IPlatform.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IProjectManager.h"
#include "Application/Services/IJobSystem.h"

#include "Engine/Config/Config_Engine.h"
#include "Engine/Engine.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Engine/Subsystems/Input/InputEvents.h"

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
        Engine().Present();
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

    switch (InEvent.GetCategoryFlags())
    {
    case EEventCategory::Application:
        HandleApplicationEvent(lDispatcher, InEvent);
        break;
    case EEventCategory::Input:
    case EEventCategory::Keyboard:
    case EEventCategory::Mouse:
    case EEventCategory::MouseButton:
        HandleAllInputEvent(lDispatcher, InEvent);
        break;
    case EEventCategory::None:
    default: 
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
    
    RegisterModules(m_ModuleRegistrar);
    OnModulesRegistered();
    
    Engine().Startup();
    
    PostEngineStartup();
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
}

void OpaaxApplication::HandleAllInputEvent(EventDispatcher& Dispatcher, Event& InEvent)
{
    
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