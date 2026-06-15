// P0 smoke — proves three things at once:
//   1. the doctest harness compiles, runs, and is discovered by CTest;
//   2. the test exe can include engine headers (PUBLIC Source include propagates
//      from the OpaaxEngine target);
//   3. the test exe links + loads OpaaxEngine.dll across the DLL boundary.
//
// (3) is the milestone's central de-risk. OpaaxPath::IsAbsolutePath is an
// out-of-line OPAAX_API symbol (defined in OpaaxPath.cpp, compiled INTO the
// engine DLL), so referencing it forces a real cross-DLL import — unlike the
// many header-inline Core helpers, which the test would otherwise compile itself.
// It is pure + needs no OpaaxPath::Init(), so it is safe to call standalone.
#include <doctest.h>

#include "Core/OpaaxPath.h"
#include "Core/OpaaxString.hpp"

TEST_CASE("smoke: harness runs and the engine DLL links")
{
    CHECK(1 + 1 == 2); // harness alive

    // Cross-DLL symbol resolves + behaves.
    CHECK(Opaax::OpaaxPath::IsAbsolutePath(Opaax::OpaaxString("C:/x")) == true);
    CHECK(Opaax::OpaaxPath::IsAbsolutePath(Opaax::OpaaxString("Game/Assets")) == false);
}

// =============================================================================
// TEMP — T2.3 CI gate demo. Proves the matrix goes RED when a test fails.
// DELETE this whole TEST_CASE once CI has shown red (then push the revert → green).
// =============================================================================
TEST_CASE("T2.3: intentional failure to prove the CI gate catches it")
{
    CHECK(1 == 2); // deliberately false — expected to fail on CI
}
