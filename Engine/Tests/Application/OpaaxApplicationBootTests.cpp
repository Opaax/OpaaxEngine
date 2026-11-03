// Suite: OpaaxApplication boot — the host owns the locator and Provides Platform + Paths.
// Constructs the real host headless (window/renderer creation is not part of Bootstrap)
// and checks the services resolve to real, non-null instances, and that the Logger
// singleton gets its sinks for the app's lifetime and loses them at shutdown.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Application/OpaaxApplication.h"
#include "Platform/IPlatform.h"
#include "Application/Services/IPaths.h"
#include "Application/Services/ISaveStorage.h"
#include "Core/Log/Logger.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/OpaaxPathString.h"
#include "Platform/CommandLine.h"
#include "Platform/CrashHandler.h"

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

namespace
{
    // The app with the player's folder named as --user-dir names it. Given to Paths here: on Windows the
    // app reads the process's own command line, not the arguments a test hands to its constructor.
    class UserDirApplication final : public OpaaxApplication
    {
    public:
        UserDirApplication(int InArgc, char** InArgv, OpaaxString InUserDir)
            : OpaaxApplication(InArgc, InArgv), m_UserDir(Move(InUserDir)) {}

    protected:
        TUniquePtr<IPaths> CreatePaths(const IPlatform& InPlatform, const CommandLine&) override
        {
            const CommandLine lCommandLine({ OpaaxString("OpaaxTests.exe"), OpaaxString("--user-dir"), m_UserDir });
            // Qualified: inside the app, Paths is its accessor.
            return MakeUnique<Opaax::Paths>(InPlatform, lCommandLine);
        }

    private:
        OpaaxString m_UserDir;
    };
}

TEST_CASE("OpaaxApplication: the log, the crash reports and the saves go to the player's folder")
{
    // As a ship build, which writes nothing where it was installed.
    const TestTempDir lTemp("boot_user_dir");
    const OpaaxString lUserDir = lTemp.Sub("User");

    char  lArg0[] = "OpaaxTests.exe";
    char* lArgv[] = { lArg0 };
    {
        UserDirApplication lApp(1, lArgv, lUserDir);
        lApp.Bootstrap();

        CHECK(lApp.Paths().UserDir() == lUserDir);
        CHECK(FileSystem::IsFile(PathString::Join(lUserDir, "Log/OpaaxEngine.log")));
        CHECK(CrashHandler::Get().GetSettings().DumpDir == PathString::Join(lUserDir, "Crashes"));
        CHECK(CrashHandler::Get().GetSettings().LogFile == PathString::Join(lUserDir, "Log/OpaaxEngine.log"));
        CHECK(lApp.SaveStorage().GetSaveDirectory() == PathString::Join(lUserDir, "SaveGames"));
    }
}
