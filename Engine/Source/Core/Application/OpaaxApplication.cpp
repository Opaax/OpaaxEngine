#include "OpaaxApplication.h"

#include "Core/Log/OpaaxLog.h"
#include "Core/Application/Services/IPlatform.h"
#include "Core/Application/Services/IPaths.h"
#include "Core/Application/Services/ILogger.h"

#ifdef OPAAX_PLATFORM_WINDOWS
#include "Core/Application/Services/WindowsPlatform.h"
#endif

using namespace Opaax;

// =============================================================================
// CTORS - DTORS
// =============================================================================

OpaaxApplication::OpaaxApplication(int InArgc, char** InArgv)
    : m_Argc(InArgc)
    , m_Argv(InArgv)
{
    OpaaxLog::Init();

    Bootstrap();
    InitializeApplication();
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
    // Opaax:: qualifies the TYPE — the same-named accessors (Logger()/Paths()) shadow it here.
    m_Services.Provide<ILogger, Opaax::Logger>();
    m_Services.Provide<IPaths, Opaax::Paths>(Platform(), m_Argc, m_Argv);

    const OpaaxString lProjectRoot = Paths().ProjectRoot();
    OPAAX_CORE_INFO("OpaaxApplication ========> Booted. Project root: {0}", lProjectRoot.CStr());
}

IPlatform& OpaaxApplication::Platform() { return m_Services.Get<IPlatform>(); }
IPaths&    OpaaxApplication::Paths()    { return m_Services.Get<IPaths>(); }
ILogger&   OpaaxApplication::Logger()   { return m_Services.Get<ILogger>(); }

// =============================================================================
// Initialization
// =============================================================================

void OpaaxApplication::InitializeApplication()
{
    OPAAX_CORE_TRACE("Initializing Opaax Application");
}

void OpaaxApplication::CreateApplicationWindow()
{

}

void OpaaxApplication::CreateApplicationRenderer()
{

}
