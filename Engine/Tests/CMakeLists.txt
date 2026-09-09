# =============================================================================
# OpaaxTests — unit-test executable (doctest)
#
# Links the OpaaxEngine import lib exactly like Game.exe: only OPAAX_API-exported
# (or header-inline) symbols are reachable — NO OBJECT-library split needed. The
# engine target PUBLIC-propagates its Source/ include dir and its PUBLIC compile
# definitions; ENGINE_EXPORTS is PRIVATE to the engine, so here OPAAX_API resolves
# to dllimport (correct).
#
# Test sources are listed EXPLICITLY (no file(GLOB)) so adding a suite is a
# deliberate edit and never silently desyncs a stale CMake cache.
# =============================================================================

set(OPAAX_TEST_SOURCES
    Main.cpp
    SmokeTest.cpp
    Renderer/CameraViewTests.cpp
    Renderer/SortKeyTests.cpp
    Renderer/BatchPlanTests.cpp
    Renderer/RenderTargetTests.cpp
    # ⑥ S4's headless half: the codepoint-keyed glyph tables and the bake's refusals. Baking a real
    # typeface needs a real .ttf, which this suite does not read — the smoke run's counting log line
    # is that gate.
    Renderer/FontTests.cpp
    Renderer/DebugDrawTests.cpp
    # ⑦-C P1a. Guid::Derive is what lets one prefab be instantiated twice into one world, so a
    # collision here is not a hash nitpick — it is an instance that silently fails to appear.
    Core/GuidTests.cpp
    Core/StringTests.cpp
    Core/StringViewTests.cpp
    Core/StringIDTests.cpp
    Core/PathStringTests.cpp
    Core/TagTests.cpp
    Core/TagContainerTests.cpp
    Core/ColorTests.cpp
    Core/Reflection/PropertyTests.cpp
    Core/Reflection/EnumTests.cpp
    Core/Config/EngineConfigDataTests.cpp
    Core/Application/AppServiceLocatorTests.cpp
    Core/Application/PathsTests.cpp
    Core/Application/FileSystemTests.cpp
    Core/Application/Utf8PathTests.cpp
    Core/Application/ILoggerTests.cpp
    Core/Application/IProjectManagerTests.cpp
    Core/Application/IConfigSystemTests.cpp
    Core/Application/IJobSystemTests.cpp
    Core/Application/IWindowManagerTests.cpp
    Core/Application/OpaaxApplicationBootTests.cpp
    Core/Engine/Subsystems/Resources/ResourceSystemTests.cpp
    Core/Engine/Subsystems/Resources/ResourceFormatRegistryTests.cpp
    Core/Engine/Subsystems/Resources/TextureResourceTests.cpp
    Core/Engine/Subsystems/Resources/SpriteSheetTests.cpp
    Core/Engine/Subsystems/Resources/AnimationTests.cpp
    # ⑦-A P5a. The animation pair's shape one family over — a MoveMode is the clip, a Mover is the
    # library — so the resolution rules under test are the ones ⑥ S3 settled.
    Core/Engine/Subsystems/Resources/MoverAssetTests.cpp
    Core/Engine/CameraResolveTests.cpp
    Core/Engine/ModuleRegistrarTests.cpp
    Core/Engine/InputManagerTests.cpp
    Core/Engine/EngineEventsTests.cpp
    # ⑦-B B0. The game bracket and the GameInstance tier need no GL context and no host, so the
    # mechanism is gated here; where the HOSTS call it is gated by their ordered boot log (L22).
    Core/Engine/GameInstanceTests.cpp
    # ⑦-B B1. The transform half of input mapping is pure — modifiers, the context stack, the
    # four trigger phases — so it is gated here against a REAL InputManager driven by its own
    # feed, rather than by a fake input source that would share the bug it is meant to catch.
    Core/Engine/InputMappingTests.cpp
    Core/World/WorldEventsTests.cpp
    Core/World/ComponentIdentityTests.cpp
    Core/World/ComponentRegistryTests.cpp
    Core/World/WorldEntityTests.cpp
    Core/World/MapSnapshotTests.cpp
    Core/World/MapRestoreTests.cpp
    Core/World/MapJsonTests.cpp
    Core/World/MapFileTests.cpp
    Core/World/LevelFileTests.cpp
    Core/World/LevelTests.cpp
    # Two TUs on purpose: the identity question is "one tag per type or per TU?", which a
    # single translation unit cannot answer. Probes in WorldSubsystemProbes.h.
    Core/World/WorldSubsystemIdentityTests.cpp
    Core/World/WorldSubsystemIdentityTU2.cpp
    Core/World/WorldModeTests.cpp
    Core/World/WorldSubsystemRegistryTests.cpp
    Core/World/WorldCloneTests.cpp
    Core/World/WorldTickGateTests.cpp
    Core/World/EntityQueryTests.cpp
    # P6. The blend is pure, so the shortest-arc wrap and the no-previous-pose case are
    # testable without a world — and neither is judgeable from a screenshot.
    Core/World/TransformInterpolationTests.cpp
    Core/Maths/AngleTests.cpp
    Core/Maths/Bounds2DTests.cpp
    Core/Profiling/StatsTests.cpp
    # ⑦-A P0. Physics needs no GL context, so unlike the renderer the seam can be gated by a test
    # instead of by a smoke run — and because box2d is PRIVATE to the DLL, a case that builds a
    # world here also proves the seam is exported and that no consumer needs the vendor (L11).
    Core/Physics/PhysicsSeamTests.cpp
    # P1. A World needs no GL context either, so the SUBSYSTEM runs headless too — which is what
    # lets "a box falls and lands" be a gate rather than a screenshot.
    Core/Physics/PhysicsSubsystemTests.cpp
    # P2. The ordering cases (a Stayed must not outlive its Ended; a handler that destroys an
    # entity must not produce a phantom) are what M9 had to find by running it.
    Core/Physics/PhysicsEventTests.cpp
    # P4. The seam's queries are covered in PhysicsSeamTests; what is here is the resolution to
    # EntityID, and the kill volume's LATCH — "it fires" would pass for a version firing 60x/sec.
    Core/Physics/PhysicsQueryTests.cpp
    # P5b. The MODES are pure policy plus one MoveCapsule call, which is where "almost right"
    # hides — a jump that does not clear the ground, an air-steer that equals the ground one.
    Core/Physics/MoverModeTests.cpp

    # Editor-side, and the FIRST of its kind — header-only editor types only (see the include dir
    # below). ③'s gizmo delta math, pinned after a scale bug that only showed away from (0,0).
    Editor/EditorGizmoTests.cpp

    # The rect arithmetic behind the title bar AND the sprite sheet editor's frames. Same reason as
    # the gizmo one level over: a smoke run never drags an edge, so the 8 regions, the min-size
    # clamp and the clamp-inside-the-texture live here.
    Editor/EditorRectGeometryTests.cpp

    # NOTE: every quarantined suite now lives under Tests/Legacy/ with the code it exercises
    # (ARCHITECTURE.md X1) — ECS/*, Physics/CollisionProfile, Assets/AssetIdResolve and
    # Renderer/FontKerning. None are listed here; re-add live equivalents when the new world
    # grows them. Nothing under Tests/Legacy/ is compiled.

    # Perf micro-benchmarks — doctest suite "perf", SKIPPED by default so `build.bat test` + CI
    # stay fast. Run in RELEASE via `build.bat bench`. See Perf/PerfBench.h.
    Perf/EcsPerf.cpp
    Perf/MapCapturePerf.cpp
)

add_executable(OpaaxTests ${OPAAX_TEST_SOURCES})

target_link_libraries(OpaaxTests PRIVATE OpaaxEngine)

target_include_directories(OpaaxTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Vendors/doctest

    # The editor's source dir, for HEADER-ONLY editor types only — no link against OpaaxEditorLib,
    # and nothing here may pull in ImGui. Opened for EditorGizmo (③), whose delta math produced a
    # silently wrong scale away from the world origin that no smoke run could reach; a bug found by
    # eye that a test can hold is a bug the test should hold.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Editor/Source
)

# By default doctest forward-declares std::tuple / std::string / std::ostream to keep
# compile times down. Modern MSVC forbids re-declaring standard templates (warning
# C5285, N5014), so it fires on every test TU. This flag makes doctest #include the
# real standard headers instead — fixes the warning at its source, no suppression.
target_compile_definitions(OpaaxTests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)

# OpaaxEngine.dll already lands in the shared CMAKE_RUNTIME_OUTPUT_DIRECTORY bin
# dir alongside this exe (same as Game.exe), so no DLL copy step is needed.

add_test(NAME OpaaxTests COMMAND OpaaxTests)

message(STATUS "[Opaax] OpaaxTests: unit-test target created (doctest)")
