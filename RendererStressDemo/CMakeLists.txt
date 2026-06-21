file(GLOB_RECURSE RENDERERSTRESSDEMO_SOURCES
    "Source/*.cpp"
    "Source/*.h"
)

# Exe
add_executable(RendererStressDemo ${RENDERERSTRESSDEMO_SOURCES})

# Launch from the exe's own dir so the CWD matches where assets are post-build-copied
# (relative asset paths miss otherwise when launched from the .sln). See Game/CMakeLists.txt.
set_target_properties(RendererStressDemo PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

# Link Engine
target_link_libraries(RendererStressDemo PRIVATE OpaaxEngine)

# Include dirs
target_include_directories(RendererStressDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
)

# COPY Dll
add_custom_command(TARGET RendererStressDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:RendererStressDemo>
    COMMENT "Copying OpaaxEngine.dll to RendererStressDemo directory..."
)

# Copy Asset
set(RENDERERSTRESSDEMO_ASSETS_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set(RENDERERSTRESSDEMO_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/RendererStressDemo/Assets)

add_custom_command(TARGET RendererStressDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${RENDERERSTRESSDEMO_ASSETS_SOURCE}
        ${RENDERERSTRESSDEMO_ASSETS_DEST}
    COMMENT "Copying RendererStressDemo assets..."
)

set(RENDERERSTRESSDEMO_MANIFEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets/AssetManifest.json)

if(NOT EXISTS ${RENDERERSTRESSDEMO_MANIFEST_SOURCE})
    message(STATUS "[Opaax] RendererStressDemo manifest not found — generating empty manifest.")
    file(WRITE ${RENDERERSTRESSDEMO_MANIFEST_SOURCE}
"{\n    \"assets\": []\n}\n"
    )
    message(STATUS "[Opaax] Generated: ${RENDERERSTRESSDEMO_MANIFEST_SOURCE}")
endif()

add_custom_command(TARGET RendererStressDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${RENDERERSTRESSDEMO_ASSETS_SOURCE}/AssetManifest.json
        ${RENDERERSTRESSDEMO_ASSETS_DEST}/AssetManifest.json
    COMMENT "Copying RendererStressDemo asset manifest..."
)

# Copy project file next to the exe's project subfolder so adjacent-to-binary
# resolution in ResolveProjectPath finds it without a CLI flag.
add_custom_command(TARGET RendererStressDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/RendererStressDemo.opaaxproj
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/RendererStressDemo/RendererStressDemo.opaaxproj
    COMMENT "Copying RendererStressDemo.opaaxproj..."
)
