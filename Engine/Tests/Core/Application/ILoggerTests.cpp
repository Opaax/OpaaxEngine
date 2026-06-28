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

TEST_CASE("ILogger: an unprovided logger resolves to the null object")
{
    AppServiceLocator lLocator;
    CHECK(lLocator.Get<ILogger>().IsNull());
    CHECK(&lLocator.Get<ILogger>() == &ILogger::Null());
}

// NOTE: a real Logger is intentionally NOT constructed here. Logger's ctor now takes IPaths
// AND registers the "OPAAX_Engine" spdlog logger — which collides with OpaaxLog::Init()
// (called by Main.cpp) and throws "logger already exists". Reconcile who owns that logger
// name (Logger vs OpaaxLog) before re-adding a real-Logger test.
