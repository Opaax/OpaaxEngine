# The TestWorld game: module, runtime and editor executables (see CMake/OpaaxGame.cmake).
# One level per engine feature, each checked by a script in Tests/ (Docs/Automation.md).
opaax_add_game(TestWorld)

# A test per script: the runtime plays the feature's level and checks it, writing its answers and
# screenshot in <build>/TestWorldResults. They draw, so they need a display: labelled "feature"
# (CI runs them on a virtual screen; `ctest -LE feature` leaves them out).
if(OPAAX_BUILD_TESTS)
    file(GLOB TESTWORLD_SCRIPTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.json")
    set(TESTWORLD_RESULTS "${CMAKE_BINARY_DIR}/TestWorldResults")
    file(MAKE_DIRECTORY "${TESTWORLD_RESULTS}")

    foreach(TESTWORLD_SCRIPT IN LISTS TESTWORLD_SCRIPTS)
        get_filename_component(TESTWORLD_FEATURE "${TESTWORLD_SCRIPT}" NAME_WE)
        add_test(NAME TestWorld.${TESTWORLD_FEATURE}
                 COMMAND TestWorld --exec "${TESTWORLD_SCRIPT}"
                                   --exec-out "${TESTWORLD_RESULTS}/${TESTWORLD_FEATURE}.out.json"
                 WORKING_DIRECTORY "${TESTWORLD_RESULTS}")
        set_tests_properties(TestWorld.${TESTWORLD_FEATURE} PROPERTIES LABELS feature TIMEOUT 120)
    endforeach()
endif()
