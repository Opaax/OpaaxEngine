// OpaaxTests entry point.
//
// doctest generates the test registry; we provide main() ourselves (CONFIG_IMPLEMENT,
// not IMPLEMENT_WITH_MAIN). No logger setup: the Logger singleton holds lines until Init
// gives it sinks, and nothing here calls Init, so engine code under test logs silently.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

int main(int argc, char** argv)
{
    doctest::Context lContext;
    lContext.applyCommandLine(argc, argv);
    return lContext.run();
}
