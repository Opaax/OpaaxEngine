cmake_minimum_required(VERSION 3.16)
project(OpaaxCreator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(OpaaxCreator OpaaxCreator.cpp)

# Sur Windows
if(WIN32)
    set_target_properties(OpaaxCreator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()