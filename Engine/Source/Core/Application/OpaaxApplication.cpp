#include "OpaaxApplication.h"

#include "Core/Log/OpaaxLog.h"
#include "Core/Application/Services/IPlatform.h"
#include "Core/Application/Services/IPaths.h"
#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/IProjectManager.h"
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
    // Dependency order: Platform first (OS primitives), then Paths (consumes it).
#ifdef OPAAX_PLATFORM_WINDOWS
    m_Services.Provide<IPlatform, WindowsPlatform>();
#endif
    
    IPaths& lPath = m_Services.Provide<IPaths, Opaax::Paths>(Platform(), m_Argc, m_Argv);
    m_Services.Provide<ILogger, Opaax::Logger>(lPath);
    IConfigSystem& lConfigSystem = m_Services.Provide<IConfigSystem, Opaax::ConfigSystem>(lPath);
    m_Services.Provide<IProjectManager, Opaax::ProjectManager>(lPath);
    
    lConfigSystem.Register<Opaax::Config_Engine>();

    const OpaaxString lProjectRoot = Paths().ProjectRoot();
    //OPAAX_CORE_INFO("OpaaxApplication ========> Booted. Project root: {0}", lProjectRoot.CStr());
}

IPlatform&          OpaaxApplication::Platform()        { return m_Services.Get<IPlatform>();        }
IPaths&             OpaaxApplication::Paths()           { return m_Services.Get<IPaths>();           }
ILogger&            OpaaxApplication::Logger()          { return m_Services.Get<ILogger>();          }
IProjectManager&    OpaaxApplication::ProjectManager()  { return m_Services.Get<IProjectManager>();  }
IConfigSystem&      OpaaxApplication::ConfigSystem()    { return m_Services.Get<IConfigSystem>();    }

// =============================================================================
// Initialization
// =============================================================================

inline constexpr LogCategory LogRenderer{"Renderer"};

void OpaaxApplication::InitializeApplication()
{
    //OPAAX_CORE_TRACE("Initializing Opaax Application");
    
    OPAAX_LOG(LogRenderer, Info, "Initializing Opaax Application");
    
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
