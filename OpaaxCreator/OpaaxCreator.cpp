#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: OpaaxCreator <project-name>" << std::endl;
        return 1;
    }

    std::string projectName = argv[1];
    std::string currentDir = fs::current_path().string();
    std::string currentDirCorrect = currentDir;
    std::replace(currentDirCorrect.begin(), currentDirCorrect.end(), '\\', '/');
    
    std::string projectPath = currentDirCorrect + "/" + projectName;
    

    if (fs::exists(projectPath)) {
        std::cerr << "? Error: Project '" << projectName << "' already exists!" << std::endl;
        return 1;
    }

    try {
        //Create Directory
        fs::create_directory(projectPath);
        fs::create_directory(projectPath + "/Assets");
        fs::create_directory(projectPath + "/Source");
        fs::create_directory(projectPath + "/Source/Public");
        fs::create_directory(projectPath + "/Source/Private");
        

        // .opaaxproj
        std::ofstream projFile(projectPath + "/" + projectName + ".opaaxproj");
        projFile << R"({
    "version": "0.0.1",
    "name": ")"             << projectName << R"(",
    "assetsRoot": ")"       << projectName << R"(/Assets",
    "assetsManifest": ")"   << projectName << R"(/Assets/AssetManifest.json",
    "rootPath": ")"         << projectPath << R"(",
    "sourcePath": ")"       << projectName << R"(/Source"
})" << std::endl;
        projFile.close();

        // main.cpp
        std::ofstream mainFile(projectPath + "/Source/main.cpp");
        mainFile << R"(
#include ")" << projectName << R"(.h"
#include "Core/OpaaxEntryPoint.h"

        OPAAX_IMPLEMENT_APP()" << projectName << R"()
)" << std::endl;
        mainFile.close();
        
        // Project .h
        std::ofstream ProjectH(projectPath + "/Source/" + projectName + ".h");
        ProjectH << R"(
#include "Core/CoreEngineApp.h"

class )" << projectName << R"( : public Opaax::CoreEngineApp
{
public:
    )" << projectName << R"((int InArgc, char** InArgv);
};
)" << std::endl;
        ProjectH.close();
        
        // Project .cpp
        std::ofstream ProjectCPP(projectPath + "/Source/" + projectName + ".cpp");
        ProjectCPP << R"(
#include ")" << projectName << R"(.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/OpaaxTypes.h"

)" << projectName << R"(::)" << projectName << R"((int InArgc, char** InArgv) : Opaax::CoreEngineApp(InArgc, InArgv)
{
    OPAAX_TRACE("==================================");
    OPAAX_TRACE("Opaax Engine - )" << projectName << R"( Start");
    OPAAX_TRACE("==================================");
}
)" << std::endl;
        ProjectCPP.close();
        
        //Asset manifest
        std::ofstream assetManifest(projectPath + "/Assets/AssetManifest.json");
        assetManifest << R"({
    "assets": []
})" << std::endl;
        assetManifest.close();
        
        
        // .gitignore
        std::ofstream gitignore(projectPath + "/.gitignore");
        gitignore << R"(build/
*.o
*.exe
*.dll
*.lib
.vs/
.vscode/
*.user
)" << std::endl;
        gitignore.close();
        
        std::string ProjectNameALLCAP;
        // Allocate memory beforehand for efficiency
        ProjectNameALLCAP.reserve(projectName.size()); 
    
        std::transform(projectName.begin(), projectName.end(), std::back_inserter(ProjectNameALLCAP), [](unsigned char c) {
            return std::toupper(c);
        });

        // CMakeLists.txt template
        std::ofstream cmake(projectPath + "/CMakeLists.txt");
        cmake << R"(
file(GLOB_RECURSE )" << ProjectNameALLCAP << R"(_SOURCES
    "Source/*.cpp"
    "Source/*.h"
)

add_executable()" << projectName << R"( ${)" << ProjectNameALLCAP << R"(_SOURCES})

set_target_properties()" << projectName << R"( PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")

# Link Engine
target_link_libraries()" << projectName << R"( PRIVATE OpaaxEngine)

# Include dirs
target_include_directories()" << projectName << R"( PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Core
)

# COPY Dll
add_custom_command(TARGET )" << projectName << R"( POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:OpaaxEngine>
        $<TARGET_FILE_DIR:)" << projectName << R"(>
    COMMENT "Copying OpaaxEngine.dll to )" << projectName << R"( directory..."
)

# Copy Asset
set()" << ProjectNameALLCAP << R"(_ASSETS_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets)
set()" << ProjectNameALLCAP << R"(_ASSETS_DEST ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/)" << projectName << R"(/Assets)

add_custom_command(TARGET )" << projectName << R"( POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${)" << ProjectNameALLCAP << R"(_ASSETS_SOURCE}
        ${)" << ProjectNameALLCAP << R"(_ASSETS_DEST}
    COMMENT "Copying )" << projectName << R"( assets..."
)

set()" << ProjectNameALLCAP << R"(_MANIFEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Assets/AssetManifest.json)

if(NOT EXISTS ${)" << ProjectNameALLCAP << R"(_MANIFEST_SOURCE})
    message(STATUS "[Opaax] )" << projectName << R"( manifest not found — generating empty manifest.")
    file(WRITE ${)" << ProjectNameALLCAP << R"(_MANIFEST_SOURCE}
"{\n    \"assets\": []\n}\n"
    )
    message(STATUS "[Opaax] Generated: ${)" << projectName << R"(_MANIFEST_SOURCE}")
endif()

add_custom_command(TARGET )" << projectName << R"( POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${)" << ProjectNameALLCAP << R"(_ASSETS_SOURCE}/AssetManifest.json
        ${)" << ProjectNameALLCAP << R"(_ASSETS_DEST}/AssetManifest.json
    COMMENT "Copying )" << projectName << R"( asset manifest..."
)

# Copy project file next to the exe's project subfolder so adjacent-to-binary
# resolution in ResolveProjectPath finds it without a CLI flag.
add_custom_command(TARGET )" << projectName << R"( POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/)" << projectName << R"(.opaaxproj
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/)" << projectName << R"(/)" << projectName << R"(.opaaxproj
    COMMENT "Copying )" << projectName << R"(.opaaxproj..."
)

)" << std::endl;
        cmake.close();

        std::cout << "? Project created successfully!" << std::endl;
        std::cout << "?? Location: " << projectPath << std::endl;
        std::cout << "\nNext steps:" << std::endl;
        std::cout << "  1. cd " << projectName << std::endl;
        std::cout << "  2. Edit src/main.cpp" << std::endl;
        std::cout << "  3. Build with CMake or your build tool" << std::endl;

        return 0;

    }
    catch (const std::exception& e) {
        std::cerr << "? Error: " << e.what() << std::endl;
        return 1;
    }
}