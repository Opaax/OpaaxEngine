// Suite: OpaaxApplication boot — the host owns the locator and Provides Platform + Paths.
// Constructs the real host headless (window/renderer creation is not part of Bootstrap)
// and checks the services resolve to real, non-null instances. OpaaxLog::Init is
// idempotent, so the app ctor's Init no-ops against Main.cpp's.
#include <doctest.h>

#include "Application/OpaaxApplication.h"
#include "Application/Services/Platforms/IPlatform.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/ILogger.h"

using namespace Opaax;

TEST_CASE("OpaaxApplication: boot provides Platform + Paths through the locator")
{
    char  lArg0[] = "OpaaxTests.exe";
    char* lArgv[] = { lArg0 };
    OpaaxApplication lApp(1, lArgv);
    lApp.Bootstrap(); // two-phase lifecycle: the ctor builds infra, Bootstrap provides services

#ifdef OPAAX_PLATFORM_WINDOWS
    CHECK_FALSE(lApp.Platform().IsNull());
    CHECK_FALSE(lApp.Platform().GetExecutablePath().IsEmpty());
#endif

    CHECK_FALSE(lApp.Paths().IsNull());
    CHECK_FALSE(lApp.Paths().ProjectRoot().IsEmpty());
    CHECK_FALSE(lApp.Paths().EngineRoot().IsEmpty());

    CHECK_FALSE(lApp.Logger().IsNull());
}

// =============================================================================
// WorldSpec: the world's NAME is derived from the level PATH (M5)
//
// Until M5 the base seam put IProjectManager::StartupLevel() straight into WorldSpec::Name,
// which was harmless only because that value was always empty. The moment a project actually
// names its level, that would have produced a world called "Levels/Main.opaaxlevel". Static and
// pure precisely so the rule can be pinned without booting a host.
// =============================================================================
TEST_CASE("WorldSpec: DeriveWorldName takes the level file's stem")
{
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString("Levels/Main.opaaxlevel")) == OpaaxString("Main"));
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString("Main.opaaxlevel"))        == OpaaxString("Main"));
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString("A/B/C/Arena01.opaaxlevel")) == OpaaxString("Arena01"));

    // Backslashes too: the engine writes forward slashes, but a hand-edited .opaaxproj on
    // Windows may well carry the other kind.
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString("Levels\\Main.opaaxlevel")) == OpaaxString("Main"));

    // A name with dots keeps everything up to the LAST one.
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString("Levels/Main.v2.opaaxlevel")) == OpaaxString("Main.v2"));

    // No extension is fine — the stem is the whole leaf.
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString("Levels/Main")) == OpaaxString("Main"));
}

TEST_CASE("WorldSpec: DeriveWorldName falls back to \"Main\" when there is no stem to take")
{
    // The pre-M5 behaviour, preserved: no level configured still boots into something named.
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString()) == OpaaxString("Main"));
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString("")) == OpaaxString("Main"));

    // Degenerate paths that leave nothing behind, rather than an empty world name.
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString("Levels/")) == OpaaxString("Main"));
    CHECK(OpaaxApplication::DeriveWorldName(OpaaxString(".opaaxlevel")) == OpaaxString("Main"));
}
