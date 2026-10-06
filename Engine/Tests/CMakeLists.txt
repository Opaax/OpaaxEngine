# =============================================================================
# OpaaxTests — unit tests (doctest). Links the engine library like a game does.
# Test sources are listed explicitly, so adding a suite is a deliberate edit.
# =============================================================================

set(OPAAX_TEST_SOURCES
    Main.cpp
    SmokeTest.cpp

    # Renderer (pure parts: matrices, sort keys, batch planning, fonts, debug draw)
    Renderer/CameraViewTests.cpp
    Renderer/SortKeyTests.cpp
    Renderer/BatchPlanTests.cpp
    Renderer/RenderTargetTests.cpp
    Renderer/FontTests.cpp
    Renderer/DebugDrawTests.cpp

    # Core
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
    Core/Reflection/PropertyVisitorTests.cpp
    Core/Config/EngineConfigDataTests.cpp
    Core/Log/LoggerTests.cpp
    Core/Log/LogHistoryTests.cpp
    Core/Maths/AngleTests.cpp
    Core/Maths/Bounds2DTests.cpp
    Core/Profiling/StatsTests.cpp

    # Application services
    Core/Application/AppServiceLocatorTests.cpp
    Core/Application/PathsTests.cpp
    Core/Application/FileSystemTests.cpp
    Core/Application/CrashHandlerTests.cpp
    Core/Application/Utf8PathTests.cpp
    Core/Application/IProjectManagerTests.cpp
    Core/Application/IConfigSystemTests.cpp
    Core/Application/IJobSystemTests.cpp
    Core/Application/IWindowManagerTests.cpp
    Core/Application/OpaaxApplicationBootTests.cpp

    # Resources
    Core/Engine/Subsystems/Resources/ResourceSystemTests.cpp
    Core/Engine/Subsystems/Resources/ResourceFormatRegistryTests.cpp
    Core/Engine/Subsystems/Resources/TextureResourceTests.cpp
    Core/Engine/Subsystems/Resources/SpriteSheetTests.cpp
    Core/Engine/Subsystems/Resources/AnimationTests.cpp
    Core/Engine/Subsystems/Resources/MoverAssetTests.cpp
    Core/Engine/Subsystems/Resources/DataAssetTests.cpp

    # Engine
    Core/Engine/CameraResolveTests.cpp
    Core/Engine/ModuleRegistrarTests.cpp
    Core/Engine/InputManagerTests.cpp
    Core/Engine/EngineEventsTests.cpp
    Core/Engine/GameInstanceTests.cpp
    Core/Engine/InputMappingTests.cpp
    Core/Engine/UIInputRouterTests.cpp

    # World
    Core/World/WorldEventsTests.cpp
    Core/World/ComponentIdentityTests.cpp
    Core/World/ComponentRegistryTests.cpp
    Core/World/TypeNameStabilityTests.cpp
    Core/World/WorldEntityTests.cpp
    Core/World/MapSnapshotTests.cpp
    Core/World/MapRestoreTests.cpp
    Core/World/MapJsonTests.cpp
    Core/World/MapFileTests.cpp
    Core/World/PrefabTests.cpp
    Core/World/PrefabOverrideTests.cpp
    Core/World/PrefabNestingTests.cpp
    Core/World/HardReferencesTests.cpp
    Core/World/LevelFileTests.cpp
    Core/World/LevelTests.cpp
    # Two TUs on purpose: they check "one tag per type", not "one tag per TU".
    Core/World/WorldSubsystemIdentityTests.cpp
    Core/World/WorldSubsystemIdentityTU2.cpp
    Core/World/WorldModeTests.cpp
    Core/World/WorldSubsystemRegistryTests.cpp
    Core/World/WorldCloneTests.cpp
    Core/World/WorldTickGateTests.cpp
    Core/World/EntityQueryTests.cpp
    Core/World/EntityHierarchyTests.cpp
    Core/World/TransformInterpolationTests.cpp

    # Physics (no GL context needed)
    Core/Physics/PhysicsSeamTests.cpp
    Core/Physics/PhysicsSubsystemTests.cpp
    Core/Physics/PhysicsEventTests.cpp
    Core/Physics/PhysicsQueryTests.cpp
    Core/Physics/MoverModeTests.cpp

    # Runtime UI (layout, events, serialization; no GL context needed)
    UI/UIRectTests.cpp
    UI/UICanvasTests.cpp
    UI/UITextTests.cpp
    UI/UIEventTests.cpp
    UI/UIButtonTests.cpp
    UI/UICanvasFileTests.cpp
    UI/UIMaskTests.cpp
    UI/UISliceTests.cpp
    UI/UISafeAreaTests.cpp
    UI/UIStackTests.cpp
    UI/UIBindingTests.cpp
    UI/UIImageSourceTests.cpp

    # Editor logic that needs no ImGui (header-only editor types)
    Editor/EditorGizmoTests.cpp
    Editor/EditorRectGeometryTests.cpp
    Editor/UIWidgetDrawerTests.cpp
    Editor/DrawerRegistryTests.cpp
    Editor/ResourceDragPayloadTests.cpp
    Editor/UIPreviewAspectTests.cpp
    Editor/LogFilterTests.cpp
    Editor/ConfigChangeTrackerTests.cpp

    # Micro-benchmarks: doctest suite "perf", skipped by default. Run them in Release with
    # OpaaxTests --test-suite=perf --no-skip.
    Perf/EcsPerf.cpp
    Perf/MapCapturePerf.cpp
)

add_executable(OpaaxTests ${OPAAX_TEST_SOURCES})
opaax_configure_target(OpaaxTests)

target_link_libraries(OpaaxTests PRIVATE OpaaxEngine)

target_include_directories(OpaaxTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Vendors/doctest
    # Header-only editor types only: nothing here may pull in ImGui.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Editor/Source
)

# doctest includes the real standard headers instead of forward-declaring std types
# (recent MSVC rejects re-declaring standard templates).
target_compile_definitions(OpaaxTests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)

add_test(NAME OpaaxTests COMMAND OpaaxTests)
