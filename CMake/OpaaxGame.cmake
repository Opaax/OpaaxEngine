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
# folder that runs on another machine.
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

    # --- Runtime executable ----------------------------------------------------------------------
    file(GLOB_RECURSE lRuntimeSources CONFIGURE_DEPENDS
        "${lRoot}/Source/${InName}Runtime/*.cpp"
        "${lRoot}/Source/${InName}Runtime/*.h")

    add_executable(${InName} ${lRuntimeSources})
    opaax_configure_target(${InName})
    target_link_libraries(${InName} PRIVATE ${InName}Module)
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
        target_link_libraries(${InName}Editor PRIVATE OpaaxEditorLib ${InName}Module)
        target_include_directories(${InName}Editor PRIVATE "${lRoot}/Editor/Source/${InName}Editor")
        set_target_properties(${InName}Editor PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${InName}Editor>")
    endif()
endfunction()

# Copies the project and the engine content next to the executable after each build.
function(_opaax_deploy_game_content InName InRoot)
    set(lProjectDest "$<TARGET_FILE_DIR:${InName}>/${InName}")

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
    install(FILES "${InRoot}/${InName}.opaaxproj" DESTINATION ${InName} COMPONENT ${InName})

    foreach(lDir Assets Configs)
        if(EXISTS "${InRoot}/${lDir}")
            install(DIRECTORY "${InRoot}/${lDir}" DESTINATION ${InName} COMPONENT ${InName})
        endif()
    endforeach()

    install(DIRECTORY "${OPAAX_ENGINE_ASSETS_DIR}/" DESTINATION Engine/Assets COMPONENT ${InName})
endfunction()
