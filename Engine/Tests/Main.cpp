// OpaaxTests entry point. doctest's registry with our own main(). The Logger is never
// initialized here, so engine code under test logs nothing.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

int main(int argc, char** argv)
{
    doctest::Context lContext;
    lContext.applyCommandLine(argc, argv);
    return lContext.run();
}
