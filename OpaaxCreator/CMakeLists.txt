# =============================================================================
# OpaaxCreator — standalone project-generator tool. Deliberately NOT part of the
# engine build: it depends on nothing but the standard library, and MakeOpaax.bat
# builds it on demand (cmake -S OpaaxCreator -B OpaaxCreator/build).
# =============================================================================
cmake_minimum_required(VERSION 3.20)
project(OpaaxCreator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(OpaaxCreator OpaaxCreator.cpp)

set_target_properties(OpaaxCreator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
