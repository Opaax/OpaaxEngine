cmake_minimum_required(VERSION 3.20)
project(OpaaxEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(VULKAN_SDK $ENV{VULKAN_SDK})

find_package(Boost REQUIRED)
find_package(Vulkan REQUIRED)

file(GLOB_RECURSE ENGINE_HEADERS "Include/*.h")
file(GLOB_RECURSE ENGINE_SOURCES "Source/*.cpp")

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC ON CACHE BOOL "" FORCE)  # Vulkan support

add_subdirectory(ThirdParty/spdlog)
add_subdirectory(ThirdParty/GLFW)
add_subdirectory(ThirdParty/glm)

add_library(OpaaxEngine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})  # Include headers

target_include_directories(OpaaxEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/spdlog/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/GLFW/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/glm
    ${Boost_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(OpaaxEngine PRIVATE spdlog::spdlog glfw Vulkan::Vulkan ${BOOST_LIBRARYDIR})

# Sandbox (Test Game)
file(GLOB_RECURSE SANDBOX_HEADERS "Sandbox/*.h")
file(GLOB_RECURSE SANDBOX_SOURCES "Sandbox/*.cpp")
add_executable(Sandbox Sandbox/Main.cpp)

target_include_directories(Sandbox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Include)
target_link_libraries(Sandbox PRIVATE OpaaxEngine)

# Organize files into folders inside Visual Studio
source_group("Header Files" FILES ${ENGINE_HEADERS})
source_group("Source Files" FILES ${ENGINE_SOURCES})

# Third-Party Library Links
message(STATUS "Download dependencies manually if needed:")
message(STATUS "Boost: https://www.boost.org/")
message(STATUS "GLFW: https://github.com/glfw/glfw")
message(STATUS "GLM: https://github.com/g-truc/glm")
message(STATUS "spdlog: https://github.com/gabime/spdlog")
message(STATUS "Jolt Physics: https://github.com/jrouwe/JoltPhysics")

get_target_property(ENGINE_INCLUDE_DIRS OpaaxEngine INCLUDE_DIRECTORIES)
message(STATUS "Engine Include Dirs: ${ENGINE_INCLUDE_DIRS}")