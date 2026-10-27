# =============================================================================
# Compiler settings shared by every target (vendors included), plus opaax_configure_target(),
# which gives them, and the warnings, to the engine's, the editor's and the games' own targets.
# =============================================================================

# Static C runtime on Windows: a shipped game needs no Visual C++ redistributable.
# Set before any add_subdirectory so vendors use the same runtime.
set(OPAAX_MSVC_RUNTIME "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE INTERNAL "C runtime of every target")
set(CMAKE_MSVC_RUNTIME_LIBRARY "${OPAAX_MSVC_RUNTIME}")

if(MSVC)
    set(OPAAX_MSVC_COMPILE_OPTIONS
        # One source encoding for every file, and the same execution charset.
        /utf-8
        # Large translation units (template-heavy registration code) exceed the default section count.
        /bigobj
        # Compile the files of one project in parallel.
        /MP
        # __cplusplus reports the real standard version.
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
        # Release keeps symbols in a separate .pdb so a crash dump names functions and lines.
        $<$<CONFIG:Release>:/Zi>
        CACHE INTERNAL "MSVC compile options of every target")

    set(OPAAX_MSVC_LINK_OPTIONS
        # /DEBUG turns off /OPT:REF and /OPT:ICF by default; they are turned back on.
        $<$<CONFIG:Release>:/DEBUG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
        # No incremental links: executables link the engine and game modules whole, and an incremental
        # link can keep an old list of static initializers, so a type a header registers goes missing
        # until the next full link.
        $<$<CONFIG:Debug>:/INCREMENTAL:NO>
        CACHE INTERNAL "MSVC link options of every target")

    add_compile_options(${OPAAX_MSVC_COMPILE_OPTIONS})
    add_link_options(${OPAAX_MSVC_LINK_OPTIONS})
endif()

# Threads (job system, audio) for every platform.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# =============================================================================
# opaax_configure_target(<target>)
#   Language level, C runtime, options and warnings for engine, editor, game and test targets. Not
#   for vendors. Set on the target and not only for the folder: the settings above reach the
#   engine's folders, but a game repository that adds the engine as a subdirectory makes its games
#   in folders of its own.
# =============================================================================
function(opaax_configure_target InTarget)
    set_target_properties(${InTarget} PROPERTIES
        CXX_STANDARD          20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS        OFF
        MSVC_RUNTIME_LIBRARY  "${OPAAX_MSVC_RUNTIME}")

    if(MSVC)
        target_compile_options(${InTarget} PRIVATE /W3 ${OPAAX_MSVC_COMPILE_OPTIONS})
        target_link_options(${InTarget} PRIVATE ${OPAAX_MSVC_LINK_OPTIONS})
    else()
        target_compile_options(${InTarget} PRIVATE
            -Wall
            -Wextra
            -Wno-unused-parameter
            -Wno-missing-field-initializers)
    endif()
endfunction()
