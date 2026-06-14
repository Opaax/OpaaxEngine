# OpaaxTests — How to write a unit test

The engine's automated test layer. Framework: **[doctest](https://github.com/doctest/doctest) 2.4.11**,
vendored as a single header at `Engine/Vendors/doctest/doctest.h`. Tests are pure-CPU only — no GPU, no
window, no running game.

---

## Running the tests

```bash
./build.bat test          # builds OpaaxTests (debug-editor / Debug) + runs CTest, tail shown
```

Other ways:

```bash
# Run the exe directly to see doctest's per-case summary (N cases / M assertions):
./build/debug-editor/bin/Debug/OpaaxTests.exe

# Run a subset by case name or source file:
./build/debug-editor/bin/Debug/OpaaxTests.exe -tc="*Hierarchy*"
./build/debug-editor/bin/Debug/OpaaxTests.exe -sf="*FontKerning*"

# Release config (after a release build):
./build.bat release
ctest --test-dir build/release -C Release --output-on-failure
```

> The Visual Studio generator is multi-config, so CTest needs `-C <Config>` (Debug for `debug-editor`,
> Release for `release`). `build.bat test` handles this for you.

---

## How tests reach engine code (the one thing to understand)

`OpaaxTests.exe` links the **`OpaaxEngine` import lib exactly like `Game.exe`**. A symbol is reachable two ways:

1. **Header-inline logic** — anything `inline` / `constexpr` / `static`-in-header (e.g. `OpaaxString`,
   `MakeSortKey`, `KerningLookup`). The test compiles it directly; **no DLL symbol needed**. Prefer this for
   pure logic.
2. **Out-of-line `OPAAX_API` symbols** — defined in a `.cpp` compiled into the engine DLL (e.g. `Hierarchy::*`,
   `CollisionProfile::*`, `World`). Reachable because the class/function is marked `OPAAX_API`.

If the logic you want to test lives in a **private** member, **do NOT friend the test or make it public.**
Instead **extract the pure logic into a free function in a header**, have the class delegate to it, and test
the free function directly. Examples already in the tree:
- `Renderer/Text/FontKerning.h` — `KerningLookup` (extracted from `FontAsset::GetKerning`)
- `Assets/AssetIdResolve.h` — `ResolveCanonicalAssetId` (extracted from `AssetRegistry::Normalize`)

---

## Adding a new suite (3 steps)

**1. Create the file** at `Engine/Tests/<Domain>/<Thing>Tests.cpp`, mirroring `Engine/Source/<Domain>/`.

```cpp
// Suite: <one line — what behaviour this pins>.
#include <doctest.h>

#include "Path/To/TheThing.h"   // engine headers after doctest

using namespace Opaax;

TEST_CASE("TheThing: does the expected thing")
{
    CHECK(1 + 1 == 2);                          // boolean check, keeps going on failure
    REQUIRE(SomePointer != nullptr);            // hard stop — aborts the case if it fails
    CHECK(SomeFloat == doctest::Approx(3.14f)); // float comparison (never == on floats)
}
```

**2. Register it** by adding the path to `OPAAX_TEST_SOURCES` in `Engine/Tests/CMakeLists.txt`:

```cmake
set(OPAAX_TEST_SOURCES
    Main.cpp
    SmokeTest.cpp
    ...
    <Domain>/<Thing>Tests.cpp   # <-- add this line
)
```

> The list is **explicit on purpose** (no `file(GLOB)`) — a stale CMake cache silently dropping a new test
> file is worse than one extra line here. Adding a suite is a deliberate edit.

**3. Run** `./build.bat test` and confirm it's green.

---

## Conventions in this codebase

- **No `main()` in your suite.** `Main.cpp` owns it: it calls `OpaaxLog::Init()` then silences the log level,
  so engine code under test can log (e.g. a missing-file asset ctor) without a null-logger crash and without
  cluttering CTest output. Just write `TEST_CASE`s.
- **Naming matches the engine:** `l`-prefixed locals, `In`-prefixed params, mirror the surrounding style.
- Use the **engine aliases** (`TDynArray`, `UnorderedMap`, ...) not raw `std::`; use **`OPAAX_ID("Name")`** for
  string IDs.
- **Floats:** always `doctest::Approx(expected)` (optionally `.epsilon(0.001)`), never `==`.
- **ECS tests** build a headless `World` on the stack — `World lWorld; auto e = lWorld.CreateEntity("x");
  lWorld.AddComponent<T>(e);` — no engine init, no scene needed. See `ECS/HierarchyTests.cpp`.

---

## What does NOT belong here

- **GPU / rendering / window** — no GL or Vulkan context exists in the test process. Renderer *logic* that is
  pure (sort keys, kerning math, UV packing) is fine; anything that touches a device is not.
- **Subsystem behaviour that needs a live world/loop** — e.g. `MoverSubsystem` self-heal/rejection needs a
  physics world; the `PhysicsSubsystem` needs PIE. Unit-test the pure component/data surface here; that
  integration belongs in a future integration-test target, not `OpaaxTests`.
- **Asset loading from disk, manifest population** — the manifest + path resolution are global singletons.
  Extract and test the pure decision (like `ResolveCanonicalAssetId`); leave the global lookups to integration.

---

## Layout

```
Engine/Tests/
  CMakeLists.txt        # OpaaxTests target + explicit source list
  Main.cpp              # doctest main() + logger init/silence — don't add cases here
  SmokeTest.cpp         # proves the harness + DLL link
  <Domain>/*Tests.cpp   # one file per area (Renderer/, Core/, ECS/, Physics/, Assets/)
```
