# =============================================================================
# OpaaxTests — unit tests (doctest). Links the engine library like a game does.
# One folder per engine module. Sources are listed explicitly, so adding a suite is a
# deliberate edit.
# =============================================================================

set(OPAAX_TEST_SOURCES
    Main.cpp
    SmokeTest.cpp

    # Core
    Core/ColorTests.cpp
    Core/Config/EngineConfigDataTests.cpp
    Core/GuidTests.cpp
    Core/Log/LogHistoryTests.cpp
    Core/Log/LoggerTests.cpp
    Core/Maths/AngleTests.cpp
    Core/Maths/Bounds2DTests.cpp
    Core/PathStringTests.cpp
    Core/Profiling/StatsTests.cpp
    Core/Reflection/EnumTests.cpp
    Core/Reflection/PropertyJsonTests.cpp
    Core/Reflection/PropertyTests.cpp
    Core/Reflection/PropertyVisitorTests.cpp
    Core/StringIDTests.cpp
    Core/StringTests.cpp
    Core/StringViewTests.cpp
    Core/TagContainerTests.cpp
    Core/TagTests.cpp

    # Application services and platform
    Application/AppServiceLocatorTests.cpp
    Application/CrashHandlerTests.cpp
    Application/FileSystemTests.cpp
    Application/IConfigSystemTests.cpp
    Application/IJobSystemTests.cpp
    Application/IProjectManagerTests.cpp
    Application/IWindowManagerTests.cpp
    Application/OpaaxApplicationBootTests.cpp
    Application/PathsTests.cpp
    Application/Utf8PathTests.cpp

    # Engine
    Engine/AutoRegistrationTests.cpp
    Engine/EngineEventsTests.cpp
    Engine/GameInstanceTests.cpp
    Engine/ModuleRegistrarTests.cpp

    # Resources
    Resources/DataAssetTests.cpp
    Resources/ResourceFormatRegistryTests.cpp
    Resources/ResourceSystemTests.cpp

    # World
    World/BehaviourRuntimeTests.cpp
    World/BehaviourStorageTests.cpp
    World/ComponentIdentityTests.cpp
    World/ComponentRegistryTests.cpp
    World/EntityHierarchyTests.cpp
    World/EntityQueryTests.cpp
    World/HardReferencesTests.cpp
    World/LevelFileTests.cpp
    World/LevelTests.cpp
    World/MapFileTests.cpp
    World/MapJsonTests.cpp
    World/MapRestoreTests.cpp
    World/MapSnapshotTests.cpp
    World/PrefabNestingTests.cpp
    World/PrefabOverrideTests.cpp
    World/PrefabTests.cpp
    World/TransformInterpolationTests.cpp
    World/TypeNameStabilityTests.cpp
    World/WorldCloneTests.cpp
    World/WorldEntityTests.cpp
    World/WorldEventsTests.cpp
    World/WorldModeTests.cpp
    # Two TUs on purpose: they check "one tag per type", not "one tag per TU".
    World/WorldSubsystemIdentityTests.cpp
    World/WorldSubsystemIdentityTU2.cpp
    World/WorldSubsystemRegistryTests.cpp
    World/WorldTickGateTests.cpp

    # Input
    Input/InputManagerTests.cpp
    Input/InputMappingTests.cpp

    # Animation
    Animation/AnimationTests.cpp

    # Movement
    Movement/MoverAssetTests.cpp
    Movement/MoverModeTests.cpp

    # Physics (no GL context needed)
    Physics/PhysicsEventTests.cpp
    Physics/PhysicsMotionTests.cpp
    Physics/PhysicsQueryTests.cpp
    Physics/PhysicsSeamTests.cpp
    Physics/PhysicsSubsystemTests.cpp

    # Renderer (the parts that need no GL context)
    Renderer/BatchPlanTests.cpp
    Renderer/CameraResolveTests.cpp
    Renderer/CameraViewTests.cpp
    Renderer/DebugDrawTests.cpp
    Renderer/FontTests.cpp
    Renderer/RenderTargetTests.cpp
    Renderer/SortKeyTests.cpp
    Renderer/SpriteSheetTests.cpp
    Renderer/TextureResourceTests.cpp

    # Runtime UI (layout, events, serialization; no GL context needed)
    UI/UIBindingTests.cpp
    UI/UIButtonTests.cpp
    UI/UICanvasFileTests.cpp
    UI/UICanvasTests.cpp
    UI/UIEventTests.cpp
    UI/UIImageSourceTests.cpp
    UI/UIInputRouterTests.cpp
    UI/UIMaskTests.cpp
    UI/UIRectTests.cpp
    UI/UISafeAreaTests.cpp
    UI/UISliceTests.cpp
    UI/UIStackTests.cpp
    UI/UITextTests.cpp

    # Editor logic that needs no ImGui (header-only editor types)
    Editor/ConfigChangeTrackerTests.cpp
    Editor/DrawerRegistryTests.cpp
    Editor/EditorGizmoTests.cpp
    Editor/EditorRectGeometryTests.cpp
    Editor/LogFilterTests.cpp
    Editor/ResourceDragPayloadTests.cpp
    Editor/UIPreviewAspectTests.cpp
    Editor/UIWidgetDrawerTests.cpp

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
