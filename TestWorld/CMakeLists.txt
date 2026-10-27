# The TestWorld game: module, runtime and editor executables (see CMake/OpaaxGame.cmake).
# One level per engine feature, each checked by a script in Tests/ (Docs/Automation.md).
opaax_add_game(TestWorld)

# A test per script: the runtime plays the feature's level and checks it, writing its answers and
# screenshot in <build>/TestWorldResults. They draw, so they need a display: labelled "feature"
# (CI runs them on a virtual screen; `ctest -LE feature` leaves them out).
opaax_add_game_feature_tests(TestWorld Tests)

# The editor driven by a script (EditorTests/*.json), when this build has the editor.
if(OPAAX_BUILD_TESTS AND TARGET TestWorldEditor)
    file(GLOB TESTWORLD_EDITOR_SCRIPTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/EditorTests/*.json")
    set(TESTWORLD_RESULTS "${CMAKE_BINARY_DIR}/TestWorldResults")

    # The files the editor tests write (Assets/Scratch) are removed first: each run makes them anew.
    add_test(NAME TestWorldEditor.ClearScratch
             COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Scratch")
    set_tests_properties(TestWorldEditor.ClearScratch PROPERTIES LABELS feature FIXTURES_SETUP TestWorldScratch)

    foreach(TESTWORLD_SCRIPT IN LISTS TESTWORLD_EDITOR_SCRIPTS)
        get_filename_component(TESTWORLD_FEATURE "${TESTWORLD_SCRIPT}" NAME_WE)
        add_test(NAME TestWorldEditor.${TESTWORLD_FEATURE}
                 COMMAND TestWorldEditor --exec "${TESTWORLD_SCRIPT}"
                                         --exec-out "${TESTWORLD_RESULTS}/Editor${TESTWORLD_FEATURE}.out.json"
                 WORKING_DIRECTORY "${TESTWORLD_RESULTS}")
        set_tests_properties(TestWorldEditor.${TESTWORLD_FEATURE} PROPERTIES LABELS feature TIMEOUT 180
                             FIXTURES_REQUIRED TestWorldScratch)
    endforeach()
endif()
