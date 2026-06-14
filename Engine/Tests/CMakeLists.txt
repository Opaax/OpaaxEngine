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
    Renderer/SortKeyTests.cpp
    Core/StringTests.cpp
    Physics/CollisionProfileTests.cpp
)

add_executable(OpaaxTests ${OPAAX_TEST_SOURCES})

target_link_libraries(OpaaxTests PRIVATE OpaaxEngine)

target_include_directories(OpaaxTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Vendors/doctest
)

# OpaaxEngine.dll already lands in the shared CMAKE_RUNTIME_OUTPUT_DIRECTORY bin
# dir alongside this exe (same as Game.exe), so no DLL copy step is needed.

add_test(NAME OpaaxTests COMMAND OpaaxTests)

message(STATUS "[Opaax] OpaaxTests: unit-test target created (doctest)")
