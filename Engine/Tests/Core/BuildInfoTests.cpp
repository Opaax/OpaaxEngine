// Suite: Build (the build kind the engine was compiled as).
#include <doctest.h>

#include "Core/Build/BuildInfo.h"

using namespace Opaax;

TEST_CASE("Build: IsDevBuild matches the dev build's baked source roots")
{
    // CMake defines OPAAX_WORKSPACE_DIR only in a dev build: both must agree.
#if defined(OPAAX_WORKSPACE_DIR)
    CHECK(Build::IsDevBuild());
#else
    CHECK_FALSE(Build::IsDevBuild());
#endif
}

TEST_CASE("Build: IsDevBuild is a constant expression")
{
    // Code may remove dev tools with `if constexpr`.
    constexpr bool lbDevBuild = Build::IsDevBuild();
    static_assert(lbDevBuild == (OPAAX_DEV_BUILD != 0));
    CHECK(lbDevBuild == (OPAAX_DEV_BUILD != 0));
}
