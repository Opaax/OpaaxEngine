# =============================================================================
# Online store SDKs (Steamworks...). Optional: a build without one leaves its backend out, and a game
# never needs one to start (the engine loads the store's library while running, it links nothing).
# An SDK is never committed: the repository is public and each SDK's agreement forbids publishing it.
#
#   opaax_add_online_sdk(<Name>
#       CACHE <var> ENV <var> FOLDER <folder under Engine/Vendors>
#       HEADER <file under the SDK that proves it> INCLUDE <include folder under the SDK>
#       RUNTIME_WINDOWS <file> RUNTIME_LINUX <file> RUNTIME_MACOS <file>)
#
# The SDK is looked for in the cache variable, then the environment variable, then
# Engine/Vendors/<folder>/sdk. Found: the engine compiles Source/Online/<Name>/ with
# OPAAX_ONLINE_<NAME>, and opaax_add_game copies the store's library beside each game. Not found:
# Source/Online/<Name>/ leaves the build.
# =============================================================================

# Refuses to configure while a file of InFolder is tracked by git: an SDK must never be pushed.
function(_opaax_refuse_tracked_sdk InName InFolder)
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        return()
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" ls-files -- "${InFolder}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE lTracked
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)

    if(lTracked)
        message(FATAL_ERROR "[Opaax] The ${InName} SDK is tracked by git (${InFolder}): it must never be "
                            "committed. Untrack it with: git rm -r --cached \"${InFolder}\"")
    endif()
endfunction()

function(opaax_add_online_sdk InName)
    cmake_parse_arguments(ARG "" "CACHE;ENV;FOLDER;HEADER;INCLUDE;RUNTIME_WINDOWS;RUNTIME_LINUX;RUNTIME_MACOS" "" ${ARGN})

    set(lVendor "${CMAKE_CURRENT_SOURCE_DIR}/Vendors/${ARG_FOLDER}")
    _opaax_refuse_tracked_sdk(${InName} "${lVendor}")

    set(${ARG_CACHE} "" CACHE PATH "The ${InName} SDK folder (empty: \$ENV{${ARG_ENV}}, then Engine/Vendors/${ARG_FOLDER}/sdk)")

    set(lSdk "${${ARG_CACHE}}")
    if(NOT lSdk AND DEFINED ENV{${ARG_ENV}})
        set(lSdk "$ENV{${ARG_ENV}}")
    endif()
    if(NOT lSdk)
        set(lSdk "${lVendor}/sdk")
    endif()
    file(TO_CMAKE_PATH "${lSdk}" lSdk)

    if(WIN32)
        set(lRuntime "${lSdk}/${ARG_RUNTIME_WINDOWS}")
    elseif(APPLE)
        set(lRuntime "${lSdk}/${ARG_RUNTIME_MACOS}")
    else()
        set(lRuntime "${lSdk}/${ARG_RUNTIME_LINUX}")
    endif()

    if(NOT EXISTS "${lSdk}/${ARG_HEADER}" OR NOT EXISTS "${lRuntime}")
        message(STATUS "[Opaax] Online SDK ${InName}: not found in ${lSdk}, its backend is left out "
                       "(set ${ARG_CACHE} or ${ARG_ENV} to the SDK folder)")
        set_property(GLOBAL APPEND PROPERTY OPAAX_ONLINE_SDKS_MISSING ${InName})
        return()
    endif()

    message(STATUS "[Opaax] Online SDK ${InName}: ${lSdk}")
    set_property(GLOBAL APPEND PROPERTY OPAAX_ONLINE_SDKS_FOUND ${InName})
    set_property(GLOBAL PROPERTY OPAAX_ONLINE_${InName}_INCLUDE "${lSdk}/${ARG_INCLUDE}")
    set_property(GLOBAL APPEND PROPERTY OPAAX_ONLINE_RUNTIMES "${lRuntime}")
endfunction()

# Takes the folders of the stores whose SDK is missing out of a source list.
function(opaax_filter_online_sources InSourcesVar)
    get_property(lMissing GLOBAL PROPERTY OPAAX_ONLINE_SDKS_MISSING)
    set(lSources ${${InSourcesVar}})
    foreach(lName IN LISTS lMissing)
        list(FILTER lSources EXCLUDE REGEX "/Source/Online/${lName}/")
    endforeach()
    set(${InSourcesVar} ${lSources} PARENT_SCOPE)
endfunction()

# Gives InTarget the headers of the SDKs found and their OPAAX_ONLINE_<NAME> definitions.
function(opaax_use_online_sdks InTarget)
    get_property(lFound GLOBAL PROPERTY OPAAX_ONLINE_SDKS_FOUND)
    foreach(lName IN LISTS lFound)
        string(TOUPPER "${lName}" lUpper)
        get_property(lInclude GLOBAL PROPERTY OPAAX_ONLINE_${lName}_INCLUDE)
        target_include_directories(${InTarget} SYSTEM PRIVATE "${lInclude}")
        target_compile_definitions(${InTarget} PRIVATE OPAAX_ONLINE_${lUpper})
    endforeach()
endfunction()

# Copies the stores' libraries beside InTarget after each build, and installs them with it.
function(opaax_deploy_online_runtimes InTarget InInstall)
    get_property(lRuntimes GLOBAL PROPERTY OPAAX_ONLINE_RUNTIMES)
    foreach(lRuntime IN LISTS lRuntimes)
        add_custom_command(TARGET ${InTarget} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${lRuntime}" "$<TARGET_FILE_DIR:${InTarget}>/"
            COMMENT "Deploying ${lRuntime} beside ${InTarget}")
        if(InInstall)
            install(FILES "${lRuntime}" DESTINATION . COMPONENT ${InTarget})
        endif()
    endforeach()
endfunction()
