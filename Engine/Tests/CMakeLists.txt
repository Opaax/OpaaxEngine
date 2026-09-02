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
    Renderer/RenderTargetTests.cpp
    Renderer/DebugDrawTests.cpp
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
    Core/Engine/CameraResolveTests.cpp
    Core/Engine/ModuleRegistrarTests.cpp
    Core/Engine/InputManagerTests.cpp
    Core/Engine/EngineEventsTests.cpp
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
    Core/Maths/AngleTests.cpp
    Core/Maths/Bounds2DTests.cpp
    Core/Profiling/StatsTests.cpp

    # Editor-side, and the FIRST of its kind — header-only editor types only (see the include dir
    # below). ③'s gizmo delta math, pinned after a scale bug that only showed away from (0,0).
    Editor/EditorGizmoTests.cpp

    # The client-drawn title bar's frame arithmetic. Same reason as the gizmo one level over: a
    # smoke run never drags a window edge, so the 8 regions and the min-size clamp live here.
    Editor/WindowFrameGeometryTests.cpp

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
