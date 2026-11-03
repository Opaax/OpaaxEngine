// Suite: OpaaxApplication boot — the host owns the locator and Provides Platform + Paths.
// Constructs the real host headless (window/renderer creation is not part of Bootstrap)
// and checks the services resolve to real, non-null instances, and that the Logger
// singleton gets its sinks for the app's lifetime and loses them at shutdown.
#include <doctest.h>

#include "Application/OpaaxApplication.h"
#include "Platform/IPlatform.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/ISaveStorage.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxPathString.h"

using namespace Opaax;

TEST_CASE("OpaaxApplication: boot provides Platform + Paths through the locator")
{
    char  lArg0[] = "OpaaxTests.exe";
    char* lArgv[] = { lArg0 };
    {
        OpaaxApplication lApp(1, lArgv);
        lApp.Bootstrap(); // two-phase lifecycle: the ctor builds infra, Bootstrap provides services

        CHECK_FALSE(lApp.Platform().IsNull());
        CHECK_FALSE(lApp.Platform().GetExecutablePath().IsEmpty());

        CHECK_FALSE(lApp.Paths().IsNull());
        CHECK_FALSE(lApp.Paths().ProjectRoot().IsEmpty());
        CHECK_FALSE(lApp.Paths().EngineRoot().IsEmpty());

        // The player's saves, in the user folder.
        CHECK_FALSE(lApp.SaveStorage().IsNull());
        CHECK(lApp.SaveStorage().GetSaveDirectory() == PathString::Join(lApp.Paths().UserDir(), "SaveGames"));

        CHECK(Logger::Get().HasSinks());
    }

    CHECK_FALSE(Logger::Get().HasSinks());
}
