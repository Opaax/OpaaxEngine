// Suite: ILogger facade. The locator resolves a logging service that forwards to OpaaxLog's
// engine logger; the null object drops. Output isn't asserted (Main.cpp silences the engine
// logger's level) — this verifies wiring + null safety, since the facade is thin forwarding.
#include <doctest.h>

#include "Core/Application/Services/ILogger.h"
#include "Core/Application/Services/AppServiceLocator.h"

using namespace Opaax;

TEST_CASE("ILogger: the null object drops messages safely")
{
    ILogger& lNull = ILogger::Null();
    CHECK(lNull.IsNull());

    lNull.Info(OpaaxString("dropped"));                       // must not crash
    lNull.Error(OpaaxString("dropped"));
    lNull.Log(ELogLevel::Critical, OpaaxString("dropped"));
}

TEST_CASE("ILogger: unprovided resolves to the null object; provided returns the facade")
{
    AppServiceLocator lLocator;
    CHECK(lLocator.Get<ILogger>().IsNull());
    CHECK(&lLocator.Get<ILogger>() == &ILogger::Null());

    ILogger& lLogger = lLocator.Provide<ILogger, Logger>();
    CHECK_FALSE(lLogger.IsNull());

    // Facade forwards to OpaaxLog without crashing (output silenced in tests).
    lLogger.Info(OpaaxString("hello from ILogger"));
    lLogger.Log(ELogLevel::Warn, OpaaxString("warn via Log()"));
}
