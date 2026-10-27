// =============================================================================
// OpaaxCreator — generates a new Opaax game project matching the CURRENT engine:
//
//     <Name>/
//         <Name>.opaaxproj              identity only (name/id/engineVersion/startupLevel)
//         .gitignore
//         CMakeLists.txt                <Name>Module (static) + <Name>.exe + <Name>Editor.exe + tests
//         Assets/                       project content (layout derived by convention — IPaths)
//         Configs/                      Engine.config / Renderer.config
//         Source/<Name>/                the game module (types register themselves) — linked by BOTH exes
//         Source/<Name>Runtime/         runtime host (<Name>App : OpaaxApplication)
//         Editor/Source/<Name>Editor/   editor host (<Name>EditorApp : EditorApplication
//                                       + <Name>EditorModule : IEditorModule)
//         Tests/                        feature test scripts; Tests/Unit/: the unit tests (<Name>Tests.exe)
//
// The skeleton is real files in OpaaxCreator/Templates/; this tool copies that tree, replacing
// tokens (__NAME__, __NAME_UPPER__, __UUID__) in file names and contents. Update the templates,
// not this file, when the engine changes.
//
// The project is created at the workspace root and added to the workspace's CMakeLists.txt (only
// once). The workspace is the engine's repository (next to Sandbox/) or a game repository of its
// own: one without a CMakeLists.txt yet gets the files of OpaaxCreator/Workspace/ (a top-level
// CMakeLists.txt that adds the engine as a subdirectory, its presets, a .gitignore).
//
// Usage: OpaaxCreator <ProjectName> [WorkspaceRoot] [EngineRoot]     (MakeOpaax passes all three)
//   EngineRoot is the engine's repository, where the templates are; it defaults to WorkspaceRoot.
// =============================================================================

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    // ------------------------------------------------------------------------- helpers
    std::string ReadAllText(const fs::path& InFile)
    {
        std::ifstream lStream(InFile, std::ios::binary);
        std::ostringstream lOut;
        lOut << lStream.rdbuf();
        return lOut.str();
    }

    void WriteAllText(const fs::path& InFile, const std::string& InText)
    {
        std::ofstream lStream(InFile, std::ios::binary);
        lStream << InText;
    }

    void ReplaceAll(std::string& InOutText, const std::string& InFrom, const std::string& InTo)
    {
        for (size_t lPos = InOutText.find(InFrom); lPos != std::string::npos;
             lPos = InOutText.find(InFrom, lPos + InTo.size()))
        {
            InOutText.replace(lPos, InFrom.size(), InTo);
        }
    }

    std::string ToUpper(const std::string& InText)
    {
        std::string lOut = InText;
        for (char& lChar : lOut) { lChar = static_cast<char>(std::toupper(static_cast<unsigned char>(lChar))); }
        return lOut;
    }

    // UUIDv4 for the .opaaxproj "id" — random hex, version/variant bits set.
    std::string MakeUuid4()
    {
        std::random_device lSeed;
        std::mt19937_64 lRng(static_cast<std::uint64_t>(lSeed()) << 32 ^ lSeed());
        std::uniform_int_distribution<int> lHex(0, 15);

        static const char* lDigits = "0123456789abcdef";
        std::string lOut = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
        for (char& lChar : lOut)
        {
            if (lChar == 'x')      { lChar = lDigits[lHex(lRng)]; }
            else if (lChar == 'y') { lChar = lDigits[8 + lHex(lRng) % 4]; } // variant: 8..b
        }
        return lOut;
    }

    // The name becomes C++ class names + CMake targets — it must be an identifier.
    bool IsValidProjectName(const std::string& InName)
    {
        if (InName.empty() || InName.size() > 64) { return false; }
        if (!std::isalpha(static_cast<unsigned char>(InName[0])) && InName[0] != '_') { return false; }
        for (const char lChar : InName)
        {
            if (!std::isalnum(static_cast<unsigned char>(lChar)) && lChar != '_') { return false; }
        }
        return true;
    }

    // Names that collide with existing targets/directories of the workspace. A game also makes
    // <Name>Module, <Name>Editor and <Name>Tests: "Opaax" would make OpaaxTests.
    bool IsReservedName(const std::string& InName)
    {
        static const char* lReserved[] = {
            "Engine", "Editor", "Sandbox", "SandboxModule", "SandboxEditor",
            "TestWorld", "TestWorldModule", "TestWorldEditor", "Opaax",
            "OpaaxEngine", "OpaaxEngineLib", "OpaaxEditorLib", "OpaaxTests", "OpaaxTestMain", "OpaaxCreator",
            "glad", "glfw", "glm", "stb", "entt", "nlohmann_json", "spdlog", "box2d", "miniaudio",
            "imgui", "imguizmo", "tinyfiledialogs",
            "Legacy", "Docs", "build", "ALL_BUILD", "ZERO_CHECK", "RUN_TESTS", "INSTALL"
        };
        const std::string lLower = [&] {
            std::string s = InName;
            for (char& c : s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
            return s;
        }();
        for (const char* lName : lReserved)
        {
            std::string lRes = lName;
            for (char& c : lRes) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
            if (lLower == lRes) { return true; }
        }
        return false;
    }

    // ------------------------------------------------------------------------- root CMakeLists
    // Appends add_subdirectory(<Name>) to the workspace CMakeLists.txt, unless an uncommented one is
    // already there.
    bool RegisterInRootCMake(const fs::path& InRootCMake, const std::string& InName)
    {
        const std::string lNeedle = "add_subdirectory(" + InName + ")";

        std::istringstream lLines(ReadAllText(InRootCMake));
        std::string lLine;
        while (std::getline(lLines, lLine))
        {
            const size_t lStart = lLine.find_first_not_of(" \t");
            if (lStart == std::string::npos || lLine[lStart] == '#') { continue; }
            if (lLine.find(lNeedle, lStart) != std::string::npos) { return false; } // already registered
        }

        // Binary: the file keeps its own line endings on every platform.
        std::ofstream lAppend(InRootCMake, std::ios::app | std::ios::binary);
        lAppend << "\nadd_subdirectory(" << InName << ") # added by OpaaxCreator\n";
        return true;
    }

    // ------------------------------------------------------------------------- paths
    // An absolute folder without "." or ".." and without a trailing separator ("C:\Games\" -> "C:\Games").
    fs::path NormalizeDir(const fs::path& InPath)
    {
        fs::path lOut = fs::absolute(InPath).lexically_normal();
        if (!lOut.has_filename() && lOut.has_relative_path())
        {
            lOut = lOut.parent_path();
        }
        return lOut;
    }

    // A CMake project name from the workspace folder's name: letters, digits and underscores.
    std::string ProjectNameOf(const fs::path& InWorkspace)
    {
        std::string lOut;
        for (const char8_t lByte : InWorkspace.filename().u8string())
        {
            const unsigned char lChar = static_cast<unsigned char>(lByte);
            lOut += (lChar < 0x80 && std::isalnum(lChar)) ? static_cast<char>(lChar) : '_';
        }
        return (lOut.empty() || std::isdigit(static_cast<unsigned char>(lOut[0]))) ? "Games" + lOut : lOut;
    }

    // The engine's repository as the workspace's CMakeLists.txt names it: relative to the workspace (a
    // submodule in it, a checkout beside it), so the repository works on any machine; absolute when
    // it is on another drive.
    std::string EnginePathFrom(const fs::path& InWorkspace, const fs::path& InEngine)
    {
        const fs::path lRelative = InEngine.lexically_relative(InWorkspace);
        return (lRelative.empty() ? InEngine : lRelative).generic_string();
    }

    // ------------------------------------------------------------------------- generation
    using TokenList = std::vector<std::pair<std::string, std::string>>;

    std::string ReplaceTokens(std::string InText, const TokenList& InTokens)
    {
        for (const auto& [lFrom, lTo] : InTokens) { ReplaceAll(InText, lFrom, lTo); }
        return InText;
    }

    // Copies a template tree into InDest, tokens replaced in file names and contents. With
    // bInKeepExisting, a file already there is left as it is (listed in OutKept). Each file written
    // is listed in OutWritten as it is written, so a failure can remove them.
    void InstantiateTree(const fs::path& InTemplates, const fs::path& InDest, const TokenList& InTokens,
                         const bool bInKeepExisting, std::vector<fs::path>& OutWritten, std::vector<fs::path>& OutKept)
    {
        for (const auto& lEntry : fs::recursive_directory_iterator(InTemplates))
        {
            // Tokens are replaced in the relative path too (template names contain __NAME__).
            std::string lRel = ReplaceTokens(lEntry.path().lexically_relative(InTemplates).generic_string(), InTokens);
            if (lRel == "gitignore") { lRel = ".gitignore"; } // stored without the dot so it cannot hide Templates/ files

            const fs::path lTarget = InDest / lRel;
            if (lEntry.is_directory())
            {
                fs::create_directories(lTarget);
                continue;
            }
            if (bInKeepExisting && fs::exists(lTarget))
            {
                OutKept.push_back(lTarget);
                continue;
            }

            fs::create_directories(lTarget.parent_path());
            OutWritten.push_back(lTarget);
            WriteAllText(lTarget, ReplaceTokens(ReadAllText(lEntry.path()), InTokens));
        }
    }

    void InstantiateGame(const fs::path& InTemplates, const fs::path& InDest,
                         const std::string& InName, const std::string& InUuid)
    {
        std::vector<fs::path> lWritten;
        std::vector<fs::path> lKept;
        InstantiateTree(InTemplates, InDest,
                        { { "__NAME_UPPER__", ToUpper(InName) }, { "__NAME__", InName }, { "__UUID__", InUuid } },
                        false, lWritten, lKept);

        // Empty, so not in Templates/ (git does not keep empty dirs). Editor/Assets is a resource-browser
        // root; the editor warns when it is missing.
        fs::create_directories(InDest / "Assets");
        fs::create_directories(InDest / "Editor" / "Assets");
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: OpaaxCreator <ProjectName> [WorkspaceRoot] [EngineRoot]" << std::endl;
        return 1;
    }

    const std::string lName      = argv[1];
    const fs::path    lWorkspace = NormalizeDir(argc > 2 ? fs::path(argv[2]) : fs::current_path());
    const fs::path    lEngine    = NormalizeDir(argc > 3 ? fs::path(argv[3]) : lWorkspace);

    // ----- validate the name --------------------------------------------------
    if (!IsValidProjectName(lName))
    {
        std::cerr << "[Opaax] Error: '" << lName << "' is not a valid project name.\n"
                  << "        It becomes C++ class names: letters/digits/underscore, no leading digit."
                  << std::endl;
        return 1;
    }
    if (IsReservedName(lName))
    {
        std::cerr << "[Opaax] Error: '" << lName << "' is reserved by the engine workspace." << std::endl;
        return 1;
    }

    // ----- validate the engine ------------------------------------------------
    const fs::path lTemplates          = lEngine / "OpaaxCreator" / "Templates";
    const fs::path lWorkspaceTemplates = lEngine / "OpaaxCreator" / "Workspace";
    if (!fs::exists(lEngine / "Engine" / "CMakeLists.txt") || !fs::exists(lTemplates) || !fs::exists(lWorkspaceTemplates))
    {
        std::cerr << "[Opaax] Error: '" << lEngine.string()
                  << "' is not the Opaax engine's repository (no Engine/ + OpaaxCreator/Templates/)." << std::endl;
        return 1;
    }

    // ----- validate the workspace --------------------------------------------
    // The engine's repository, or a game repository: one without a CMakeLists.txt is new.
    std::error_code lNoWorkspace;
    const bool     bEngineRepository = fs::equivalent(lWorkspace, lEngine, lNoWorkspace);
    const bool     bWorkspaceExisted = fs::exists(lWorkspace);
    const fs::path lRootCMake        = lWorkspace / "CMakeLists.txt";
    const bool     bNewWorkspace     = !fs::exists(lRootCMake);
    if (bEngineRepository && bNewWorkspace)
    {
        std::cerr << "[Opaax] Error: no CMakeLists.txt in the engine's repository '" << lEngine.string() << "'." << std::endl;
        return 1;
    }

    const fs::path lDest = lWorkspace / lName;
    if (fs::exists(lDest))
    {
        std::cerr << "[Opaax] Error: '" << lDest.string() << "' already exists." << std::endl;
        return 1;
    }

    // ----- generate -----------------------------------------------------------
    const std::string     lEnginePath = EnginePathFrom(lWorkspace, lEngine);
    std::vector<fs::path> lWorkspaceFiles;
    std::vector<fs::path> lKeptFiles;
    try
    {
        InstantiateGame(lTemplates, lDest, lName, MakeUuid4());
        if (bNewWorkspace)
        {
            InstantiateTree(lWorkspaceTemplates, lWorkspace,
                            { { "__WORKSPACE__", ProjectNameOf(lWorkspace) }, { "__ENGINE_PATH__", lEnginePath } },
                            true, lWorkspaceFiles, lKeptFiles);
        }
    }
    catch (const std::exception& lError)
    {
        // Roll back what this run created: the project, the workspace's files, the workspace itself.
        std::cerr << "[Opaax] Error: " << lError.what() << std::endl;
        std::error_code lIgnored;
        fs::remove_all(lDest, lIgnored);
        for (const fs::path& lFile : lWorkspaceFiles) { fs::remove(lFile, lIgnored); }
        if (!bWorkspaceExisted) { fs::remove(lWorkspace, lIgnored); }
        return 1;
    }

    const bool lAdded = RegisterInRootCMake(lRootCMake, lName);

    // ----- report -------------------------------------------------------------
    std::cout << "[Opaax] Project '" << lName << "' created at " << lDest.string() << "\n";
    for (const fs::path& lFile : lWorkspaceFiles)
    {
        std::cout << "[Opaax] Game repository file written: " << lFile.string() << "\n";
    }
    for (const fs::path& lFile : lKeptFiles)
    {
        std::cout << "[Opaax] Kept as it was: " << lFile.string() << " (the engine's is in OpaaxCreator/Workspace/)\n";
    }
    std::cout << (lAdded
        ? "[Opaax] Registered in CMakeLists.txt: add_subdirectory(" + lName + ")\n"
        : "[Opaax] CMakeLists.txt already registers this project — left as is.\n");

    if (bEngineRepository)
    {
        std::cout << "\nNext steps (build.bat on Windows, ./build.sh on Linux and macOS):\n"
                  << "  build                    ->  " << lName << "Editor  (Debug + Editor)\n"
                  << "  build release            ->  " << lName << "        (runtime only)\n"
                  << "  Game code    : " << lName << "/Source/" << lName << "/\n"
                  << "  Editor code  : " << lName << "/Editor/Source/" << lName << "Editor/" << std::endl;
        return 0;
    }

    std::cout << "\nNext steps, in " << lWorkspace.string() << ":\n"
              << "  cmake --preset debug-editor\n"
              << "  cmake --build build/debug-editor --config Debug  ->  " << lName << "Editor, " << lName << "\n"
              << "  ctest --test-dir build/debug-editor -C Debug     ->  the game's tests\n"
              << "  Game code    : " << lName << "/Source/" << lName << "/\n"
              << "  Editor code  : " << lName << "/Editor/Source/" << lName << "Editor/\n";
    if (bNewWorkspace)
    {
        std::cout << "The engine is added from '" << lEnginePath << "' (CMakeLists.txt, CMakePresets.json).\n";
    }
    std::cout << "A game repository is described in the engine's Docs/GameRepository.md." << std::endl;
    return 0;
}
