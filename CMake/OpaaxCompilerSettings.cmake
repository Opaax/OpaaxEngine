# =============================================================================
# Compiler settings shared by every target (vendors included), plus opaax_configure_target()
# for the engine's own targets (warnings).
# =============================================================================

# Static C runtime on Windows: a shipped game needs no Visual C++ redistributable.
# Set before any add_subdirectory so vendors use the same runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    # One source encoding for every file, and the same execution charset.
    add_compile_options(/utf-8)
    # Large translation units (template-heavy registration code) exceed the default section count.
    add_compile_options(/bigobj)
    # Compile the files of one project in parallel.
    add_compile_options(/MP)
    # __cplusplus reports the real standard version.
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)

    # Release keeps symbols in a separate .pdb so a crash dump names functions and lines.
    # /DEBUG turns off /OPT:REF and /OPT:ICF by default; they are turned back on.
    add_compile_options("$<$<CONFIG:Release>:/Zi>")
    add_link_options("$<$<CONFIG:Release>:/DEBUG;/OPT:REF;/OPT:ICF>")
endif()

# Threads (job system, audio) for every platform.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# =============================================================================
# opaax_configure_target(<target>)
#   Warnings and definitions for engine, editor, game and test targets. Not for vendors.
# =============================================================================
function(opaax_configure_target InTarget)
    if(MSVC)
        target_compile_options(${InTarget} PRIVATE /W3)
    else()
        target_compile_options(${InTarget} PRIVATE
            -Wall
            -Wextra
            -Wno-unused-parameter
            -Wno-missing-field-initializers)
    endif()
endfunction()
