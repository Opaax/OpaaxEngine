// Suite: ExampleComponent, a starting point for the __NAME__ module's unit tests. Each suite is a file
// of this folder, listed in opaax_add_game_tests (CMakeLists.txt); the engine's Engine/Tests/README.md
// says what makes a good one.
#include <doctest.h>

#include "Components/ExampleComponent.h"

TEST_CASE("ExampleComponent: starts at its default speed")
{
    const __NAME__::ExampleComponent lComponent{};
    CHECK(lComponent.Speed == doctest::Approx(100.f));
}
