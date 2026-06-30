#include "OpaaxApplication.h"

#include "Core/Log/OpaaxLog.h"
#include "Core/Application/Services/IPlatform.h"
#include "Core/Application/Services/IPaths.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/IProjectManager.h"
#include "Core/Application/Services/IJobSystem.h"
#include "Core/Application/Services/IWindowManager.h"
#include "Core/Config/Config_Engine.h"
#include "Services/IConfigSystem.h"

#ifdef OPAAX_PLATFORM_WINDOWS
#include "Core/Application/Services/WindowsPlatform.h"
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
    bHasBeenShuttingDown = false;
    //OpaaxLog::Init();

    // Bootstrap();
    // InitializeApplication();
}

OpaaxApplication::~OpaaxApplication()
{
    if (!bHasBeenShuttingDown)
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
#ifdef OPAAX_PLATFORM_WINDOWS
    //Windows
    m_Services.Provide<IPlatform, WindowsPlatform>();
#endif
    
    //Path
    IPaths& lPath = m_Services.Provide<IPaths, Opaax::Paths>(Platform(), m_Argc, m_Argv);
    
    //Log
    m_Services.Provide<ILogger, Opaax::Logger>(lPath);
    OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Logger just initialized");
    
    //Config
    IConfigSystem& lConfigSystem = m_Services.Provide<IConfigSystem, Opaax::ConfigSystem>(lPath);
    lConfigSystem.Register<Opaax::Config_Engine>();
    
    //Project Manager
    m_Services.Provide<IProjectManager, Opaax::ProjectManager>(lPath);
    
    //Jobsystem
    m_Services.Provide<IJobSystem, Opaax::JobSystem>();

    //Window manager — the window itself is created later, in InitializeApplication (needs a GL/VK context).
    m_Services.Provide<IWindowManager, Opaax::WindowManager>();
}

IPlatform&          OpaaxApplication::Platform()        { return m_Services.Get<IPlatform>();        }
IPaths&             OpaaxApplication::Paths()           { return m_Services.Get<IPaths>();           }
ILogger&            OpaaxApplication::Logger()          { return m_Services.Get<ILogger>();          }
IProjectManager&    OpaaxApplication::ProjectManager()  { return m_Services.Get<IProjectManager>();  }
IConfigSystem&      OpaaxApplication::ConfigSystem()    { return m_Services.Get<IConfigSystem>();    }
IJobSystem&         OpaaxApplication::JobSystem()       { return m_Services.Get<IJobSystem>();       }
IWindowManager&     OpaaxApplication::WindowManager()   { return m_Services.Get<IWindowManager>();   }

// =============================================================================
// Initialization
// =============================================================================

void OpaaxApplication::InitializeApplication()
{
    CreateApplicationWindow();
    OnInitializeApplication();
    
    bIsRunning = true;
}

void OpaaxApplication::RunApplication()
{
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
        // 1.1 close event called?
        // ----------------------------------------------------------------
        bIsRunning = !lWindow->ShouldClose();
        if (!bIsRunning)
        {
            break;
        }
        
        // ----------------------------------------------------------------
        // 3. Present AFTER render, always last (graphics context owns the swap).
        // ----------------------------------------------------------------
        lWindow->SwapBuffers();
    }
}

void OpaaxApplication::ShutdownApplication()
{
    m_Services.ShutdownAll();
    
    bHasBeenShuttingDown = true;
}

void OpaaxApplication::CreateApplicationWindow()
{
    WindowManager().CreateMainWindow();
}

void OpaaxApplication::CreateApplicationRenderer()
{

}

void OpaaxApplication::OnInitializeApplication()
{
    OPAAX_CORE_TRACE("OnInitializeApplication Not override in child app class");
}
