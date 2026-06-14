// OpaaxTests entry point.
//
// doctest generates the test registry; we provide main() ourselves (CONFIG_IMPLEMENT,
// not IMPLEMENT_WITH_MAIN) so the engine logger is initialised BEFORE any suite runs.
// Engine code under test logs through OpaaxLog::GetCoreLogger(); without Init() that
// SharedPtr is null and the first OPAAX_CORE_* call would deref null (e.g. a missing-file
// asset ctor logging an error). We silence the level so expected error-path logs from
// tests don't clutter the CTest output.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

#include "Core/Log/OpaaxLog.h"

int main(int argc, char** argv)
{
    Opaax::OpaaxLog::Init();
    Opaax::OpaaxLog::GetCoreLogger()->set_level(spdlog::level::off);
    Opaax::OpaaxLog::GetClientLogger()->set_level(spdlog::level::off);

    doctest::Context lContext;
    lContext.applyCommandLine(argc, argv);
    const int lResult = lContext.run();

    Opaax::OpaaxLog::Shutdown();
    return lResult;
}
