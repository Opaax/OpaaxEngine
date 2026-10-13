# =============================================================================
# CheckStdUse — the standard library stays behind the core types (AGENTS.md, Docs/Architecture.md).
#   Lists every `std::` and every standard header include in the engine, the editor, the games and
#   the tests, outside the files below that wrap it on purpose, as file:line: text; fails when there
#   is one. Comments are not read.
#
#   CTest runs it (StdUse). By hand: cmake -P CMake/CheckStdUse.cmake
# =============================================================================
cmake_minimum_required(VERSION 3.20)

if(NOT OPAAX_ROOT)
    get_filename_component(OPAAX_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

# Files and folders where std is the point: each is the one place a std facility is wrapped.
set(OPAAX_STD_ALLOWED
    # The compiler and platform switches (a debug break on GCC raises SIGTRAP).
    Engine/Source/Core/EngineAPI.h
    # Core: the aliases, traits, algorithms and functions everything else uses.
    Engine/Source/Core/OpaaxTypes.h
    Engine/Source/Core/Templates/TypeTraits.h
    Engine/Source/Core/Algo/Algo.h
    Engine/Source/Core/Memory/Memory.h
    Engine/Source/Core/Maths/Maths.h
    Engine/Source/Core/Maths/Maths.cpp
    Engine/Source/Core/Maths/Random.cpp
    Engine/Source/Core/Time/Clock.cpp
    Engine/Source/Core/Time/DateTime.cpp
    Engine/Source/Core/IO/Console.cpp
    Engine/Source/Core/IO/FileIO.cpp
    Engine/Source/Core/IO/FileSystem.cpp
    # The string types, where they meet std::string_view (vendor APIs) and streams (doctest).
    Engine/Source/Core/String/CString.h
    Engine/Source/Core/String/OpaaxString.hpp
    Engine/Source/Core/String/OpaaxStringView.hpp
    Engine/Source/Core/String/OpaaxPathString.cpp
    Engine/Source/Core/String/OpaaxUtf8.h
    Engine/Source/Core/String/OpaaxUtf8.cpp
    # The operating system's side.
    Engine/Source/Platform/Process.cpp
    Engine/Source/Platform/Posix/
    Engine/Source/Platform/Windows/
    # Tests of that boundary.
    Engine/Tests/Core/StdInteropTests.cpp
    Engine/Tests/Application/Utf8PathTests.cpp
)

# Top-level folders that are not built: old code, and a standalone tool that uses std by design.
set(OPAAX_STD_SKIPPED_ROOTS Archive OpaaxCreator OldProject)

# C headers of the standard library (the C++ ones have no extension).
set(OPAAX_C_HEADERS assert.h ctype.h errno.h fenv.h float.h inttypes.h limits.h locale.h math.h setjmp.h
                    signal.h stdarg.h stddef.h stdint.h stdio.h stdlib.h string.h time.h uchar.h wchar.h wctype.h)

# The folders scanned: the engine, the editor, and every built top-level folder (a game has a CMakeLists.txt).
set(lRoots Engine Editor)
file(GLOB lTopLevel LIST_DIRECTORIES true RELATIVE "${OPAAX_ROOT}" "${OPAAX_ROOT}/*")
foreach(lDir IN LISTS lTopLevel)
    if(IS_DIRECTORY "${OPAAX_ROOT}/${lDir}" AND EXISTS "${OPAAX_ROOT}/${lDir}/CMakeLists.txt"
       AND NOT lDir IN_LIST OPAAX_STD_SKIPPED_ROOTS AND NOT lDir IN_LIST lRoots)
        list(APPEND lRoots "${lDir}")
    endif()
endforeach()

set(lFiles "")
foreach(lRoot IN LISTS lRoots)
    file(GLOB_RECURSE lFound RELATIVE "${OPAAX_ROOT}"
         "${OPAAX_ROOT}/${lRoot}/*.h" "${OPAAX_ROOT}/${lRoot}/*.hpp"
         "${OPAAX_ROOT}/${lRoot}/*.inl" "${OPAAX_ROOT}/${lRoot}/*.cpp")
    list(APPEND lFiles ${lFound})
endforeach()
list(FILTER lFiles EXCLUDE REGEX "(^|/)(Vendors|build|Scratch|Save)/")

set(lReport "")
set(lCount 0)
set(lScanned 0)
foreach(lFile IN LISTS lFiles)
    set(bAllowed FALSE)
    foreach(lAllowed IN LISTS OPAAX_STD_ALLOWED)
        string(FIND "${lFile}" "${lAllowed}" lAt)
        if(lAt EQUAL 0)
            set(bAllowed TRUE)
            break()
        endif()
    endforeach()
    if(bAllowed)
        continue()
    endif()
    math(EXPR lScanned "${lScanned} + 1")

    # One list item per line: what a CMake list reads specially is set aside first, and put back
    # in a line that is reported.
    file(READ "${OPAAX_ROOT}/${lFile}" lText)
    string(REPLACE "\\" "<bs>" lText "${lText}")
    string(REPLACE ";" "<sc>" lText "${lText}")
    string(REPLACE "[" "<lb>" lText "${lText}")
    string(REPLACE "]" "<rb>" lText "${lText}")
    string(REPLACE "\r" "" lText "${lText}")
    string(REPLACE "\n" ";" lLines "${lText}")

    set(lNumber 0)
    set(bInComment FALSE)
    foreach(lLine IN LISTS lLines)
        math(EXPR lNumber "${lNumber} + 1")

        # What a comment says is not code: block comments, then line comments.
        set(lCode "${lLine}")
        if(bInComment)
            string(FIND "${lCode}" "*/" lEnd)
            if(lEnd EQUAL -1)
                continue()
            endif()
            math(EXPR lEnd "${lEnd} + 2")
            string(SUBSTRING "${lCode}" ${lEnd} -1 lCode)
            set(bInComment FALSE)
        endif()
        string(REGEX REPLACE "/\\*.*\\*/" "" lCode "${lCode}")
        string(FIND "${lCode}" "/*" lStart)
        if(NOT lStart EQUAL -1)
            string(SUBSTRING "${lCode}" 0 ${lStart} lCode)
            set(bInComment TRUE)
        endif()
        string(REGEX REPLACE "//.*$" "" lCode "${lCode}")

        set(bFound FALSE)
        if(lCode MATCHES "(^|[^A-Za-z0-9_])std::")
            set(bFound TRUE)
        elseif(lCode MATCHES "^[ \t]*#[ \t]*include[ \t]*<([^>]+)>")
            set(lHeader "${CMAKE_MATCH_1}")
            if((NOT lHeader MATCHES "[/.]") OR (lHeader IN_LIST OPAAX_C_HEADERS))
                set(bFound TRUE)
            endif()
        endif()

        if(bFound)
            string(STRIP "${lLine}" lShown)
            string(REPLACE "<bs>" "\\" lShown "${lShown}")
            string(REPLACE "<sc>" ";" lShown "${lShown}")
            string(REPLACE "<lb>" "[" lShown "${lShown}")
            string(REPLACE "<rb>" "]" lShown "${lShown}")
            string(APPEND lReport "${lFile}:${lNumber}: ${lShown}\n")
            math(EXPR lCount "${lCount} + 1")
        endif()
    endforeach()
endforeach()

if(lCount GREATER 0)
    message("${lReport}")
    message(FATAL_ERROR "${lCount} use(s) of the standard library outside its wrapper files. Use the core "
                        "type or function (Core/OpaaxTypes.h and Core/), or add one there: the std use goes "
                        "in Core, not at the call site.")
endif()

message(STATUS "No std use in ${lScanned} files (the wrapper files listed in CheckStdUse.cmake excepted).")
