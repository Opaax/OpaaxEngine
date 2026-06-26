// Suite: OpaaxApplication boot — the host owns the locator and Provides Platform + Paths.
// Constructs the real host headless (window/renderer creation is not part of Bootstrap)
// and checks the services resolve to real, non-null instances. OpaaxLog::Init is
// idempotent, so the app ctor's Init no-ops against Main.cpp's.
#include <doctest.h>

#include "Core/Application/OpaaxApplication.h"
#include "Core/Application/Services/IPlatform.h"
#include "Core/Application/Services/IPaths.h"

using namespace Opaax;

TEST_CASE("OpaaxApplication: boot provides Platform + Paths through the locator")
{
    char  lArg0[] = "OpaaxTests.exe";
    char* lArgv[] = { lArg0 };
    OpaaxApplication lApp(1, lArgv);

#ifdef OPAAX_PLATFORM_WINDOWS
    CHECK_FALSE(lApp.Platform().IsNull());
    CHECK_FALSE(lApp.Platform().GetExecutablePath().IsEmpty());
#endif

    CHECK_FALSE(lApp.Paths().IsNull());
    CHECK_FALSE(lApp.Paths().ProjectRoot().IsEmpty());
    CHECK_FALSE(lApp.Paths().EngineRoot().IsEmpty());
}
