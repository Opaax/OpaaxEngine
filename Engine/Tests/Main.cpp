// OpaaxTests entry point.
//
// doctest generates the test registry; we provide main() ourselves (CONFIG_IMPLEMENT,
// not IMPLEMENT_WITH_MAIN). No logger init is needed: OPAAX_LOG dereferences
// GetLogger().AppLogger, and GetLogger() resolves ILogger from the locator — when no
// ILogger service is provided (the test case) it returns ILogger::Null(), a NullLogger
// that holds a valid *null-sink* logger. So engine code under test logs into a silent
// sink instead of dereferencing null — the old OpaaxLog::Init()+set_level(off) is now
// the default behavior. (The old OpaaxLog moved to Legacy/ and is not linked — M0.5.)
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

int main(int argc, char** argv)
{
    doctest::Context lContext;
    lContext.applyCommandLine(argc, argv);
    return lContext.run();
}
