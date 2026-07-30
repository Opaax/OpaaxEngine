// =============================================================================
// OpaaxCreator — generates a new Opaax game project matching the CURRENT engine:
//
//     <Name>/
//         <Name>.opaaxproj              identity only (name/id/engineVersion/startupLevel)
//         .gitignore
//         CMakeLists.txt                <Name>Module (static) + <Name>.exe + <Name>Editor.exe
//         Assets/                       project content (layout derived by convention — IPaths)
//         Configs/                      Engine.config / Renderer.config
//         Source/<Name>/                the game module (IRuntimeModule) — linked by BOTH exes
//         Source/<Name>Runtime/         runtime host (<Name>App : OpaaxApplication)
//         Editor/Source/<Name>Editor/   editor host (<Name>EditorApp : EditorApplication
//                                       + <Name>EditorModule : IEditorModule)
//
// The skeleton lives as REAL FILES in OpaaxCreator/Templates/ — this tool only walks
// that tree substituting tokens (__NAME__, __NAME_UPPER__, __UUID__) in file names and
// contents. When the engine architecture moves again, update the templates, not this file.
//
// The project is created at the WORKSPACE ROOT (next to Sandbox/): add_subdirectory needs
// it there, and ResolveProjectLayout's default is <workspace>/<exeStem>/<exeStem>.opaaxproj.
// The tool also registers the project in the root CMakeLists.txt (idempotent append).
//
// Usage: OpaaxCreator <ProjectName> [WorkspaceRoot]     (MakeOpaax.bat passes both)
// =============================================================================

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
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

    // Names that collide with existing targets/directories of the workspace.
    bool IsReservedName(const std::string& InName)
    {
        static const char* lReserved[] = {
            "Engine", "Editor", "Sandbox", "SandboxModule", "SandboxEditor",
            "OpaaxEngine", "OpaaxEditorLib", "OpaaxTests", "OpaaxCreator",
            "Legacy", "Docs", "build", "ALL_BUILD", "ZERO_CHECK"
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
    // Append add_subdirectory(<Name>) to the workspace CMakeLists.txt, unless an
    // UNCOMMENTED one is already there (a commented "#add_subdirectory(X)" must not mask it).
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

        std::ofstream lAppend(InRootCMake, std::ios::app);
        lAppend << "\nadd_subdirectory(" << InName << ") # added by OpaaxCreator\n";
        return true;
    }

    // ------------------------------------------------------------------------- generation
    void InstantiateTemplates(const fs::path& InTemplates, const fs::path& InDest,
                              const std::string& InName, const std::string& InUuid)
    {
        const std::string lUpper = ToUpper(InName);

        for (const auto& lEntry : fs::recursive_directory_iterator(InTemplates))
        {
            // Substitute tokens in the RELATIVE path too — template names carry __NAME__.
            std::string lRel = lEntry.path().lexically_relative(InTemplates).generic_string();
            ReplaceAll(lRel, "__NAME_UPPER__", lUpper);
            ReplaceAll(lRel, "__NAME__", InName);
            if (lRel == "gitignore") { lRel = ".gitignore"; } // stored dot-less so it can't ignore-shadow Templates/

            const fs::path lTarget = InDest / lRel;
            if (lEntry.is_directory())
            {
                fs::create_directories(lTarget);
                continue;
            }

            std::string lText = ReadAllText(lEntry.path());
            ReplaceAll(lText, "__NAME_UPPER__", lUpper);
            ReplaceAll(lText, "__NAME__", InName);
            ReplaceAll(lText, "__UUID__", InUuid);

            fs::create_directories(lTarget.parent_path());
            WriteAllText(lTarget, lText);
        }

        // Empty by design, so not in Templates/ (git would not keep an empty dir anyway).
        // Editor/Assets is a resource-browser root — the editor warns when it is missing.
        fs::create_directories(InDest / "Assets");
        fs::create_directories(InDest / "Editor" / "Assets");
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: OpaaxCreator <ProjectName> [WorkspaceRoot]" << std::endl;
        return 1;
    }

    const std::string lName = argv[1];
    const fs::path lWorkspace = fs::absolute(argc > 2 ? fs::path(argv[2]) : fs::current_path())
                                    .lexically_normal();

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

    // ----- validate the workspace --------------------------------------------
    const fs::path lRootCMake  = lWorkspace / "CMakeLists.txt";
    const fs::path lTemplates  = lWorkspace / "OpaaxCreator" / "Templates";
    if (!fs::exists(lRootCMake) || !fs::exists(lWorkspace / "Engine"))
    {
        std::cerr << "[Opaax] Error: '" << lWorkspace.string()
                  << "' is not an Opaax workspace (no CMakeLists.txt + Engine/)." << std::endl;
        return 1;
    }
    if (!fs::exists(lTemplates))
    {
        std::cerr << "[Opaax] Error: templates not found at '" << lTemplates.string() << "'." << std::endl;
        return 1;
    }

    const fs::path lDest = lWorkspace / lName;
    if (fs::exists(lDest))
    {
        std::cerr << "[Opaax] Error: '" << lDest.string() << "' already exists." << std::endl;
        return 1;
    }

    // ----- generate -----------------------------------------------------------
    try
    {
        InstantiateTemplates(lTemplates, lDest, lName, MakeUuid4());
    }
    catch (const std::exception& lError)
    {
        std::cerr << "[Opaax] Error: " << lError.what() << std::endl;
        std::error_code lIgnored;
        fs::remove_all(lDest, lIgnored); // roll back the partial project — this run created it
        return 1;
    }

    const bool lAdded = RegisterInRootCMake(lRootCMake, lName);

    // ----- report -------------------------------------------------------------
    std::cout << "[Opaax] Project '" << lName << "' created at " << lDest.string() << "\n";
    std::cout << (lAdded
        ? "[Opaax] Registered in CMakeLists.txt: add_subdirectory(" + lName + ")\n"
        : "[Opaax] CMakeLists.txt already registers this project — left as is.\n");
    std::cout << "\nNext steps:\n"
              << "  build.bat                ->  " << lName << "Editor.exe  (Debug + Editor)\n"
              << "  build.bat release        ->  " << lName << ".exe        (runtime only)\n"
              << "  Game code    : " << lName << "/Source/" << lName << "/\n"
              << "  Editor code  : " << lName << "/Editor/Source/" << lName << "Editor/" << std::endl;
    return 0;
}
