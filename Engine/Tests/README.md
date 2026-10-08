# Tests

Two layers:

- **Unit tests** (`OpaaxTests`, this folder): doctest, no GPU, no window, run everywhere in seconds.
- **Feature tests** (TestWorld, `TestWorld/Tests`): the TestWorld game plays a level per engine feature
  and checks it through automation scripts. They draw, so they need a display.

Both run in CTest; the feature tests carry the label `feature`.

## Running

```
build.bat test                 # Windows: builds OpaaxTests, runs every CTest test
./build.sh test                # Linux, macOS

build/debug-editor/bin/Debug/OpaaxTests.exe                # unit tests, with doctest's summary
build/debug-editor/bin/Debug/OpaaxTests.exe -tc="Shadows*" # cases by name
build/debug-editor/bin/Debug/OpaaxTests.exe -sf="*Physics*"# cases by source file

ctest --test-dir build/debug-editor -C Debug -LE feature   # unit tests only
ctest --test-dir build/debug-editor -C Debug -L feature    # feature tests only
```

Without Visual Studio the binaries are in `build/<preset>/bin/` (no `Debug/`). CTest needs
`-C <Config>` with Visual Studio's multi-config generator.

CI runs the unit tests on Windows, Ubuntu and macOS (debug-editor and release), and the feature tests
on Ubuntu under a virtual screen (`xvfb-run`, Mesa), keeping their answers and screenshots as an
artifact.

## Unit tests

`OpaaxTests` links the engine library like a game does: every engine symbol is reachable. Header-only
editor types can be tested too (`Editor/`); editor code with a `.cpp` cannot.

### Adding a suite

1. `Engine/Tests/<Module>/<Thing>Tests.cpp`, mirroring `Engine/Source/<Module>/`:

```cpp
// Suite: <one line: what this pins down>.
#include <doctest.h>

#include "Module/Thing.h"

using namespace Opaax;

TEST_CASE("Thing: does what it says")
{
    CHECK(Thing::Count() == 2);                    // keeps going on failure
    REQUIRE(Thing::Find("a") != nullptr);          // stops the case on failure
    CHECK(Thing::Ratio() == doctest::Approx(0.5f)); // floats: never ==
}
```

2. Add the path to `OPAAX_TEST_SOURCES` in `Engine/Tests/CMakeLists.txt` (the list is explicit: a new
   suite is a deliberate edit).
3. Run it, and break the code once to see the test fail.

### What makes a good unit test

- Name the behaviour, not the function: `"Lights: past the limit, the strongest are kept"`.
- Test logic, not the GPU: packing, layouts, limits, parsing and decisions are pure functions; the
  code that touches OpenGL stays thin around them. The shaders themselves are checked as text
  (`Renderer/EngineShaderTests.cpp` ports each one and checks the constants it repeats from C++);
  tests can read engine files from `OPAAX_TEST_ENGINE_ASSETS`.
- Worlds without an engine: `World lWorld("Test"); Entity lEntity = lWorld.CreateEntity("A");`.
  Behaviours, physics and audio have fixtures that build a world with its subsystems and step it by
  hand (`World/BehaviourRuntimeTests.cpp`, `Physics/PhysicsMotionTests.cpp`, `Audio/AudioTests.cpp`).
- Files: write under a temporary folder removed at the end (see `ScopedTempDir` in
  `Automation/AutomationTests.cpp`).
- A private member is not made public for a test: move the logic into a free function and test that.
- `Main.cpp` owns `main()` and keeps the logger quiet; never add cases there.

## Feature tests

`TestWorld` is a game made for testing: `TestWorld/Assets/Levels/<Feature>.opaaxlevel` and its map put
the feature on screen, and `TestWorld/Tests/<Feature>.json` plays it and checks it:

```json
[
    { "command": "level.play", "params": { "path": "Levels/Physics.opaaxlevel" } },
    { "command": "world.wait", "params": { "seconds": 3.0 } },
    { "id": "crate-rests", "command": "expect.value",
      "params": { "entity": "Crate1", "path": "Transform/Position/y", "near": -205, "tolerance": 3 } },
    { "command": "screenshot", "params": { "path": "Physics.png" } }
]
```

Every `*.json` in `TestWorld/Tests` becomes a CTest test (`TestWorld.<Feature>`); its answers
(`<Feature>.out.json`) and screenshot go to `build/<preset>/TestWorldResults/`. A failed check's
answer says what was expected and what was found.

To check behaviour from inside the game, write a probe: a behaviour that records what happened in its
fields (`TestWorld/Source/TestWorld/Probes`), placed in the level and read with `expect.value`.
Use `world.wait` (game time) rather than counting frames: machines and CI run at different speeds.
The commands are listed in [Docs/Automation.md](../../Docs/Automation.md).
