cmake_minimum_required(VERSION 3.20)
project(OpaaxEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(Boost REQUIRED)
add_subdirectory(ThirdParty/spdlog)
add_subdirectory(ThirdParty/GLFW)
add_subdirectory(ThirdParty/glm)

# Engine Library
add_library(OpaaxEngine
    Source/Core/OPLogger.cpp
    Source/Platform/OpaaxWindow.cpp
    "SandBox/main.cpp"
    Source/Core/OPFileSystem.cpp
    Source/OpaaxEngine.cpp
)

target_include_directories(OpaaxEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/GLFW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/glm
    ${Boost_INCLUDE_DIR}
)

target_link_libraries(OpaaxEngine PRIVATE spdlog::spdlog glfw ${BOOST_LIBRARYDIR})

# Sandbox (Test Game)
add_executable(Sandbox Sandbox/Main.cpp)
target_link_libraries(Sandbox PRIVATE OpaaxEngine)

target_include_directories(Sandbox PRIVATE Include)

# Third-Party Library Links
message(STATUS "Download dependencies manually if needed:")
message(STATUS "Boost: https://www.boost.org/")
message(STATUS "GLFW: https://github.com/glfw/glfw")
message(STATUS "GLM: https://github.com/g-truc/glm")
message(STATUS "spdlog: https://github.com/gabime/spdlog")
message(STATUS "Jolt Physics: https://github.com/jrouwe/JoltPhysics")

get_target_property(ENGINE_INCLUDE_DIRS OpaaxEngine INCLUDE_DIRECTORIES)
message(STATUS "Engine Include Dirs: ${ENGINE_INCLUDE_DIRS}")