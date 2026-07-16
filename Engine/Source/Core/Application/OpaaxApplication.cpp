#include "OpaaxApplication.h"

#include "Core/Application/Services/Platforms/IPlatform.h"
#include "Core/Application/Services/IPaths.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/IProjectManager.h"
#include "Core/Application/Services/IJobSystem.h"
#include "Core/Config/Config_Engine.h"
#include "Core/Engine/Engine.h"
#include "Core/Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Services/IConfigSystem.h"
#include "Services/IEngine.h"

#include "Services/Window/IWindowManager.h"
#include "Services/Window/WindowManager.h"
#include "Core/Events/Event.h"
#include "Core/Window/WindowEvents.h"
#include "Core/Engine/Subsystems/Input/InputEvents.h"
#include "Core/Events/EventBus.h"

#ifdef OPAAX_PLATFORM_WINDOWS
#include "Core/Application/Services/Platforms/Windows/WindowsPlatform.h"
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
    
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Workspace Path:      {}", lPath.WorkspaceRoot().CStr());
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Engine Path:         {}", lPath.EngineRoot().CStr());
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Project Root Path:   {}", lPath.ProjectRoot().CStr());
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Project File Path:   {}", lPath.ProjectFile().CStr());
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Assets Directory:    {}", lPath.AssetsDir().CStr());
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Configs Directory:   {}", lPath.ConfigsDir().CStr());
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Sources Directory:   {}", lPath.SourceDir().CStr());
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Save Directory:      {}", lPath.SaveDir().CStr());
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Temp Directory:      {}", lPath.TempDir().CStr());
    
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
    return m_Services.Provide<IPaths, Opaax::Paths>(Platform(), m_Argc, m_Argv);
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

    // Route the main window's Tier-1 events into OnEvent (the app-level sink).
    if (Window* lWindow = WindowManager().GetMainWindow())
    {
        lWindow->SetEventCallback([this](Event& InEvent) { OnEvent(InEvent); });
    }
}

void OpaaxApplication::CreateApplicationRenderer()
{

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
        // 2. Tick
        // ----------------------------------------------------------------
        Engine().Loop();
    }
}

void OpaaxApplication::OnEvent(Event& InEvent)
{
    EventDispatcher lDispatcher(InEvent);

    lDispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent&)
    {
        OPAAX_APP_LOG(Info, "WindowCloseEvent - requesting shutdown")
        bIsRunning = false;
        return true;
    });

    // Republish the resize as a Tier-3 POD so decoupled systems (renderer, camera, ...)
    // react without the window ever knowing them. Queued — delivered at the frame's Flush.
    lDispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& InResize)
    {
        Engine().GetEngineEventBus().GetEventBus().Enqueue(InResize.GetPayload());
        return false;
    });
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
    Engine().Startup();
    PostEngineStartup();
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