# =============================================================================
# opaax_add_game(<Name>)
#
# Declares a game project from the standard layout of the calling folder:
#
#   <Name>.opaaxproj                  project file (startup level, ...)
#   Assets/  Configs/                 content
#   Source/<Name>/                    game module      -> static library <Name>Module
#   Source/<Name>Runtime/             runtime host     -> executable <Name>
#   Editor/Source/<Name>Editor/       editor host      -> executable <Name>Editor (editor builds only)
#
# Ship builds (OPAAX_DEV_BUILD=OFF) copy the project and engine content next to the executable and
# declare install rules, so `cmake --install <build> --component <Name> --prefix <dir>` produces a
# folder that runs on another machine:
#
#   <Name>(.exe)                      the game
#   <Name>_Data/                      <Name>.opaaxproj, Assets/, Configs/
#   Engine/Assets/                    engine content
#
# The game's tests are declared after it: opaax_add_game_tests, opaax_add_game_feature_tests.
# =============================================================================
function(opaax_add_game InName)
    set(lRoot "${CMAKE_CURRENT_SOURCE_DIR}")

    if(NOT EXISTS "${lRoot}/${InName}.opaaxproj")
        message(FATAL_ERROR "opaax_add_game(${InName}): ${lRoot}/${InName}.opaaxproj not found.")
    endif()

    # --- Game module -----------------------------------------------------------------------------
    file(GLOB_RECURSE lModuleSources CONFIGURE_DEPENDS
        "${lRoot}/Source/${InName}/*.cpp"
        "${lRoot}/Source/${InName}/*.h"
        "${lRoot}/Source/${InName}/*.hpp")

    add_library(${InName}Module STATIC ${lModuleSources})
    opaax_configure_target(${InName}Module)
    target_link_libraries(${InName}Module PUBLIC OpaaxEngine)
    target_include_directories(${InName}Module PUBLIC "${lRoot}/Source/${InName}")

    # A type that registers itself in its header (OPAAX_REGISTER_*) must be compiled somewhere,
    # even when no .cpp uses it (a component only placed in maps). This generated file includes
    # every header of the module, so each one also has to compile on its own.
    _opaax_generate_module_types(${InName} "${lRoot}/Source/${InName}")

    # --- Runtime executable ----------------------------------------------------------------------
    file(GLOB_RECURSE lRuntimeSources CONFIGURE_DEPENDS
        "${lRoot}/Source/${InName}Runtime/*.cpp"
        "${lRoot}/Source/${InName}Runtime/*.h")

    add_executable(${InName} ${lRuntimeSources})
    opaax_configure_target(${InName})
    # Linked whole: the module's types register themselves from files nothing else references.
    target_link_libraries(${InName} PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,${InName}Module>")
    target_include_directories(${InName} PRIVATE "${lRoot}/Source/${InName}Runtime")
    set_target_properties(${InName} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${InName}>")

    if(NOT OPAAX_DEV_BUILD)
        # A shipped game opens no console window on Windows.
        if(WIN32)
            set_target_properties(${InName} PROPERTIES WIN32_EXECUTABLE ON)
            if(MSVC)
                target_link_options(${InName} PRIVATE /ENTRY:mainCRTStartup)
            endif()
        endif()

        _opaax_deploy_game_content(${InName} "${lRoot}")
        _opaax_install_game(${InName} "${lRoot}")
    endif()

    # --- Editor executable -----------------------------------------------------------------------
    if(OPAAX_EDITOR_SUPPORT AND EXISTS "${lRoot}/Editor/Source/${InName}Editor")
        file(GLOB_RECURSE lEditorSources CONFIGURE_DEPENDS
            "${lRoot}/Editor/Source/${InName}Editor/*.cpp"
            "${lRoot}/Editor/Source/${InName}Editor/*.h")

        add_executable(${InName}Editor ${lEditorSources})
        opaax_configure_target(${InName}Editor)
        target_link_libraries(${InName}Editor PRIVATE OpaaxEditorLib "$<LINK_LIBRARY:WHOLE_ARCHIVE,${InName}Module>")
        target_include_directories(${InName}Editor PRIVATE "${lRoot}/Editor/Source/${InName}Editor")
        set_target_properties(${InName}Editor PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${InName}Editor>")
    endif()
endfunction()

# =============================================================================
# opaax_add_game_tests(<Name> <sources...>)
#
# The game's unit tests: one executable, <Name>Tests, of doctest cases from the sources (relative to
# the calling folder, listed so that a new suite is a deliberate edit). It links the game module
# whole, as the game does, so every type the module registers is there. Registered in CTest as
# <Name>Tests; built with OPAAX_BUILD_TESTS.
# =============================================================================
function(opaax_add_game_tests InName)
    if(NOT OPAAX_BUILD_TESTS)
        return()
    endif()
    if(NOT TARGET ${InName}Module)
        message(FATAL_ERROR "opaax_add_game_tests(${InName}): declare the game first, with opaax_add_game(${InName}).")
    endif()
    if(NOT ARGN)
        message(FATAL_ERROR "opaax_add_game_tests(${InName}): no test sources given.")
    endif()

    add_executable(${InName}Tests ${ARGN})
    opaax_configure_target(${InName}Tests)
    target_link_libraries(${InName}Tests PRIVATE OpaaxTestMain "$<LINK_LIBRARY:WHOLE_ARCHIVE,${InName}Module>")

    add_test(NAME ${InName}Tests COMMAND ${InName}Tests)
endfunction()

# =============================================================================
# opaax_add_game_feature_tests(<Name> <dir>)
#
# A CTest test per automation script <dir>/*.json (relative to the calling folder), named
# <Name>.<Script>: the game's runtime plays it (--exec) and fails when a check fails
# (Docs/Automation.md). The answers and screenshots go to <build>/<Name>Results. They open a window,
# so they need a display: labelled "feature" (`ctest -LE feature` leaves them out). Built with
# OPAAX_BUILD_TESTS.
# =============================================================================
function(opaax_add_game_feature_tests InName InDir)
    if(NOT OPAAX_BUILD_TESTS)
        return()
    endif()
    if(NOT TARGET ${InName})
        message(FATAL_ERROR "opaax_add_game_feature_tests(${InName}): declare the game first, with opaax_add_game(${InName}).")
    endif()

    get_filename_component(lDir "${InDir}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    file(GLOB lScripts CONFIGURE_DEPENDS "${lDir}/*.json")

    set(lResults "${CMAKE_BINARY_DIR}/${InName}Results")
    file(MAKE_DIRECTORY "${lResults}")

    foreach(lScript IN LISTS lScripts)
        get_filename_component(lFeature "${lScript}" NAME_WE)
        add_test(NAME ${InName}.${lFeature}
                 COMMAND ${InName} --exec "${lScript}" --exec-out "${lResults}/${lFeature}.out.json"
                 WORKING_DIRECTORY "${lResults}")
        set_tests_properties(${InName}.${lFeature} PROPERTIES LABELS feature TIMEOUT 120)
    endforeach()
endfunction()

# Writes <Name>ModuleTypes.cpp, which includes every header of the module, and adds it to the module.
# Rewritten only when the header list changes, so it does not trigger rebuilds.
function(_opaax_generate_module_types InName InModuleDir)
    file(GLOB_RECURSE lHeaders CONFIGURE_DEPENDS "${InModuleDir}/*.h" "${InModuleDir}/*.hpp")
    list(SORT lHeaders)

    set(lContent "// Generated by opaax_add_game. Includes every header of the ${InName} module so the types\n")
    string(APPEND lContent "// they register with OPAAX_REGISTER_* macros are always linked. Do not edit.\n")
    foreach(lHeader ${lHeaders})
        file(RELATIVE_PATH lRelative "${InModuleDir}" "${lHeader}")
        string(APPEND lContent "#include \"${lRelative}\"\n")
    endforeach()

    set(lFile "${CMAKE_CURRENT_BINARY_DIR}/${InName}ModuleTypes.cpp")
    set(lPrevious "")
    if(EXISTS "${lFile}")
        file(READ "${lFile}" lPrevious)
    endif()
    if(NOT lPrevious STREQUAL lContent)
        file(WRITE "${lFile}" "${lContent}")
    endif()

    target_sources(${InName}Module PRIVATE "${lFile}")
endfunction()

# Copies the project and the engine content next to the executable after each build. The project
# goes in <Name>_Data (see SHIPPED_PROJECT_DIR_SUFFIX in IPaths.h): on Linux and macOS the
# executable has no extension, so a <Name> folder would collide with it.
function(_opaax_deploy_game_content InName InRoot)
    set(lProjectDest "$<TARGET_FILE_DIR:${InName}>/${InName}_Data")

    add_custom_command(TARGET ${InName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${lProjectDest}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${InRoot}/${InName}.opaaxproj" "${lProjectDest}/"
        COMMENT "Deploying ${InName} project file")

    # An empty folder is not tracked by git, so a fresh clone may lack one.
    foreach(lDir Assets Configs)
        if(EXISTS "${InRoot}/${lDir}")
            add_custom_command(TARGET ${InName} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${InRoot}/${lDir}" "${lProjectDest}/${lDir}"
                COMMENT "Deploying ${InName} ${lDir}")
        endif()
    endforeach()

    add_custom_command(TARGET ${InName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${OPAAX_ENGINE_ASSETS_DIR}" "$<TARGET_FILE_DIR:${InName}>/Engine/Assets"
        COMMENT "Deploying engine content for ${InName}")
endfunction()

# Install rules used by the export: one install component per game.
function(_opaax_install_game InName InRoot)
    install(TARGETS ${InName} RUNTIME DESTINATION . COMPONENT ${InName})
    install(FILES "${InRoot}/${InName}.opaaxproj" DESTINATION ${InName}_Data COMPONENT ${InName})

    foreach(lDir Assets Configs)
        if(EXISTS "${InRoot}/${lDir}")
            install(DIRECTORY "${InRoot}/${lDir}" DESTINATION ${InName}_Data COMPONENT ${InName})
        endif()
    endforeach()

    install(DIRECTORY "${OPAAX_ENGINE_ASSETS_DIR}/" DESTINATION Engine/Assets COMPONENT ${InName})
endfunction()
