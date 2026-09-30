// Smoke test: the doctest harness runs and is found by CTest, the test exe can include engine
// headers, and it links and loads OpaaxEngine.dll (ResolveProjectLayout is an exported,
// out-of-line, pure function, so calling it proves a real cross-DLL import).
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
