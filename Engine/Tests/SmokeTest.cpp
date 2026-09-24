// P0 smoke — proves three things at once:
//   1. the doctest harness compiles, runs, and is discovered by CTest;
//   2. the test exe can include engine headers (PUBLIC Source include propagates
//      from the OpaaxEngine target);
//   3. the test exe links + loads OpaaxEngine.dll across the DLL boundary.
//
// (3) is the milestone's central de-risk, and it needs a probe with three properties:
// OPAAX_API, defined OUT-OF-LINE in a .cpp compiled into the DLL (so referencing it
// forces a real cross-DLL import, unlike the many header-inline Core helpers the test
// would otherwise compile itself), and pure enough to call with nothing initialised.
//
// ResolveProjectLayout is all three — and it is the successor of the OpaaxPath::IsAbsolutePath
// this probe used until OpaaxPath was quarantined to Legacy/ (X1): same domain, same purity,
// same out-of-line export. It documents itself as "no OS calls, no globals, no defines".
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/String/OpaaxString.hpp"

TEST_CASE("smoke: harness runs and the engine DLL links")
{
    CHECK(1 + 1 == 2); // harness alive

    // Cross-DLL symbol resolves + behaves.
    const Opaax::ProjectLayout lLayout = Opaax::ResolveProjectLayout(
        Opaax::OpaaxString("W:/ws/bin/Game.exe"),
        Opaax::OpaaxString("W:/ws"),
        Opaax::OpaaxString());

    CHECK(lLayout.WorkspaceRoot == "W:/ws");
    CHECK(lLayout.ProjectFile   == "W:/ws/Game/Game.opaaxproj");
}
