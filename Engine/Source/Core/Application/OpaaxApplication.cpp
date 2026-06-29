#include "OpaaxApplication.h"

#include "Core/Log/OpaaxLog.h"
#include "Core/Application/Services/IPlatform.h"
#include "Core/Application/Services/IPaths.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/IProjectManager.h"
#include "Core/Application/Services/IJobSystem.h"
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
    //OpaaxLog::Init();

    // Bootstrap();
    // InitializeApplication();
}

OpaaxApplication::~OpaaxApplication() {}

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
}

IPlatform&          OpaaxApplication::Platform()        { return m_Services.Get<IPlatform>();        }
IPaths&             OpaaxApplication::Paths()           { return m_Services.Get<IPaths>();           }
ILogger&            OpaaxApplication::Logger()          { return m_Services.Get<ILogger>();          }
IProjectManager&    OpaaxApplication::ProjectManager()  { return m_Services.Get<IProjectManager>();  }
IConfigSystem&      OpaaxApplication::ConfigSystem()    { return m_Services.Get<IConfigSystem>();    }
IJobSystem&         OpaaxApplication::JobSystem()       { return m_Services.Get<IJobSystem>();       }

// =============================================================================
// Initialization
// =============================================================================

void OpaaxApplication::InitializeApplication()
{
    OnInitializeApplication();
}

void OpaaxApplication::CreateApplicationWindow()
{

}

void OpaaxApplication::CreateApplicationRenderer()
{

}

void OpaaxApplication::OnInitializeApplication()
{
    OPAAX_CORE_TRACE("OnInitializeApplication Not override in child app class");
}
