// Suite: IPaths robustness. The resolution is a PURE function (ResolveProjectLayout)
// driven with controlled inputs — both the editor (source-workspace) and release
// (exe-dir) branches — with zero dependency on build-time defines. The Paths ctor is
// then smoke-tested with an absolute --project (workspace-independent, so deterministic
// regardless of the suite's own OPAAX_WORKSPACE_DIR).
#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

#include "Application/Services/IPaths.h"
#include "Platform/IPlatform.h"
#include "Platform/IFileSystem.h"   // StubPlatform hands out the null one
#include "Application/Services/AppServiceLocator.h"

using namespace Opaax;

namespace
{
    class StubPlatform final : public IPlatform
    {
    public:
        explicit StubPlatform(OpaaxString InExe) : m_Exe(std::move(InExe)) {}
        Uint32      GetLogicalCoreCount() const override { return 4; }
        double      GetTimeSeconds()      const override { return 0.0; }
        OpaaxString GetExecutablePath()   const override { return m_Exe; }
        OpaaxString GetPlatformName()     const override { return OpaaxString("Stub"); }
        const IFileSystem& GetFileSystem() const override { return IFileSystem::Null(); }  // unused here
    private:
        OpaaxString m_Exe;
    };
}

// =============================================================================
// Pure resolver — robustness
// =============================================================================
TEST_CASE("ResolveProjectLayout: editor build resolves to the SOURCE workspace, not the binary")
{
    // Editor: workspace dir is baked (source tree); the binary lives in a build output dir.
    const ProjectLayout lLayout = ResolveProjectLayout(
        OpaaxString("W:/repo/build/bin/Debug/Game.exe"),
        OpaaxString("W:/repo"),   // OPAAX_WORKSPACE_DIR
        OpaaxString());           // no --project

    CHECK(lLayout.WorkspaceRoot == "W:/repo");
    CHECK(lLayout.EngineRoot    == "W:/repo/Engine");
    CHECK(lLayout.ProjectRoot   == "W:/repo/Game");          // SOURCE folder, not .../build/bin/Debug
    CHECK(lLayout.ProjectFile   == "W:/repo/Game/Game.opaaxproj");
    CHECK(lLayout.AssetsDir     == "W:/repo/Game/Assets");
    CHECK(lLayout.ConfigsDir    == "W:/repo/Game/Configs");
    CHECK(lLayout.SaveDir       == "W:/repo/Game/Save");
}

TEST_CASE("ResolveProjectLayout: release build (no workspace) anchors on the executable dir")
{
    const ProjectLayout lLayout = ResolveProjectLayout(
        OpaaxString("W:/deploy/bin/Game.exe"),
        OpaaxString(),            // release -> empty
        OpaaxString());

    CHECK(lLayout.WorkspaceRoot == "W:/deploy/bin");
    CHECK(lLayout.EngineRoot    == "W:/deploy/bin/Engine");
    CHECK(lLayout.ProjectRoot   == "W:/deploy/bin/Game");
    CHECK(lLayout.ProjectFile   == "W:/deploy/bin/Game/Game.opaaxproj");
}

TEST_CASE("ResolveProjectLayout: AppName tracks the executable stem")
{
    const ProjectLayout lLayout = ResolveProjectLayout(
        OpaaxString("W:/repo/build/bin/Debug/Sandbox.exe"),
        OpaaxString("W:/repo"),
        OpaaxString());

    CHECK(lLayout.ProjectRoot == "W:/repo/Sandbox");
    CHECK(lLayout.ProjectFile == "W:/repo/Sandbox/Sandbox.opaaxproj");
}

TEST_CASE("ResolveProjectLayout: relative --project resolves under the workspace root")
{
    const ProjectLayout lLayout = ResolveProjectLayout(
        OpaaxString("W:/repo/build/bin/Debug/Game.exe"),
        OpaaxString("W:/repo"),
        OpaaxString("Sandbox/Sandbox.opaaxproj")); // workspace-relative

    CHECK(lLayout.ProjectRoot == "W:/repo/Sandbox");
    CHECK(lLayout.ProjectFile == "W:/repo/Sandbox/Sandbox.opaaxproj");
    CHECK(lLayout.AssetsDir   == "W:/repo/Sandbox/Assets");
}

TEST_CASE("ResolveProjectLayout: absolute --project overrides the workspace")
{
    const ProjectLayout lLayout = ResolveProjectLayout(
        OpaaxString("W:/repo/build/bin/Debug/Game.exe"),
        OpaaxString("W:/repo"),
        OpaaxString("D:/external/Cool/Cool.opaaxproj")); // absolute -> wins

    CHECK(lLayout.ProjectRoot == "D:/external/Cool");
    CHECK(lLayout.ProjectFile == "D:/external/Cool/Cool.opaaxproj");
    CHECK(lLayout.SaveDir     == "D:/external/Cool/Save");
}

// =============================================================================
// Paths ctor + resolvers — deterministic via absolute --project
// =============================================================================
TEST_CASE("Paths: ctor + resolvers (absolute --project is workspace-independent)")
{
    StubPlatform lPlatform(OpaaxString("W:/deploy/bin/Game.exe"));
    char  lArg0[] = "Game.exe";
    char  lArg1[] = "--project";
    char  lArg2[] = "W:/proj/MyGame/MyGame.opaaxproj";
    char* lArgv[] = { lArg0, lArg1, lArg2 };

    const Paths lPaths(lPlatform, 3, lArgv);

    CHECK(lPaths.ProjectRoot() == "W:/proj/MyGame");
    CHECK(lPaths.AssetsDir()   == "W:/proj/MyGame/Assets");
    CHECK(lPaths.AssetToAbsolute(OpaaxString("Scenes/Main.opaaxscene"))
          == "W:/proj/MyGame/Assets/Scenes/Main.opaaxscene");
    CHECK(lPaths.ProjectToAbsolute(OpaaxString("Source/Player.cpp"))
          == "W:/proj/MyGame/Source/Player.cpp");

    // EngineRoot depends on the suite's own workspace (editor define) — assert shape only.
    const std::string lEngineRel = lPaths.EngineToAbsolute(OpaaxString("Assets/Shaders")).CStr();
    CHECK(lEngineRel.find("/Engine/Assets/Shaders") != std::string::npos);
}

TEST_CASE("Paths: AbsoluteToAsset is the inverse of AssetToAbsolute")
{
    // Against a REAL directory: AbsoluteToAsset canonicalises BOTH sides, so a fabricated path
    // would be testing what std::filesystem does with something that does not exist rather than
    // the rule itself.
    namespace fs = std::filesystem;

    const fs::path lRoot = fs::temp_directory_path() / "OpaaxPathsTests_asset";

    std::error_code lError;
    fs::remove_all(lRoot, lError);
    fs::create_directories(lRoot / "Assets" / "Maps", lError);

    StubPlatform lPlatform(OpaaxString("W:/deploy/bin/Game.exe"));
    std::string  lProject = (lRoot / "MyGame.opaaxproj").string();

    char  lArg0[] = "Game.exe";
    char  lArg1[] = "--project";
    char* lArgv[] = { lArg0, lArg1, lProject.data() };

    const Paths lPaths(lPlatform, 3, lArgv);

    const OpaaxString lAbs = lPaths.AssetToAbsolute(OpaaxString("Maps/Main.opaaxmap"));
    CHECK(lPaths.AbsoluteToAsset(lAbs) == OpaaxString("Maps/Main.opaaxmap"));

#ifdef OPAAX_PLATFORM_WINDOWS
    SUBCASE("a NATIVE path with backslashes lands on the same answer")
    {
        // The case the function exists for: a file dialog answers `C:\...\Maps\Main.opaaxmap`
        // while AssetsDir was built with forward slashes.
        std::string lNative(lAbs.CStr());
        std::replace(lNative.begin(), lNative.end(), '/', '\\');

        CHECK(lPaths.AbsoluteToAsset(OpaaxString(lNative.c_str())) == OpaaxString("Maps/Main.opaaxmap"));
    }
#endif

    SUBCASE("a file outside the assets dir has NO asset name")
    {
        // Empty is the real answer, not a failure: such a file cannot be named by a manifest.
        const OpaaxString lOutside((lRoot / "Elsewhere.opaaxmap").generic_string().c_str());
        CHECK(lPaths.AbsoluteToAsset(lOutside).IsEmpty());
    }

    SUBCASE("an empty path is refused rather than resolving to the assets dir itself")
    {
        CHECK(lPaths.AbsoluteToAsset(OpaaxString()).IsEmpty());
    }

    fs::remove_all(lRoot, lError);
}

// =============================================================================
// The /Engine/ mount — engine-shipped content is referenceable, project content is not rewritten
// =============================================================================
TEST_CASE("Paths: ENGINE_MOUNT resolves against the engine's assets, not the project's")
{
    StubPlatform lPlatform(OpaaxString("W:/deploy/bin/Game.exe"));
    char  lArg0[] = "Game.exe";
    char  lArg1[] = "--project";
    char  lArg2[] = "W:/proj/MyGame/MyGame.opaaxproj";
    char* lArgv[] = { lArg0, lArg1, lArg2 };

    const Paths lPaths(lPlatform, 3, lArgv);

    // EngineRoot follows the suite's own workspace (the editor define), so assert the SHAPE — the
    // claim under test is "which root", not "which machine".
    const std::string lMounted = lPaths.AssetToAbsolute(OpaaxString("/Engine/Textures/T_Checker_64.png")).CStr();
    CHECK(lMounted.find("/Engine/Assets/Textures/T_Checker_64.png") != std::string::npos);
    CHECK(lMounted.find("/MyGame/Assets") == std::string::npos);

    CHECK(lPaths.EngineAssetsDir() == lPaths.EngineToAbsolute(OpaaxString("Assets")));

    SUBCASE("an UNPREFIXED path is untouched — every existing .opaaxmap stays valid")
    {
        CHECK(lPaths.AssetToAbsolute(OpaaxString("Textures/Hero.png"))
              == "W:/proj/MyGame/Assets/Textures/Hero.png");
    }

    SUBCASE("a path merely CONTAINING the mount word is project content")
    {
        // The discriminator is the leading '/', not the word: a project folder may be called Engine.
        CHECK(lPaths.AssetToAbsolute(OpaaxString("Engine/Notes.png"))
              == "W:/proj/MyGame/Assets/Engine/Notes.png");
    }
}

TEST_CASE("Paths: AbsoluteToAsset names engine content by its mount")
{
    // A real project root under temp, as the inverse test above does. The ENGINE root is NOT
    // fabricated: the ctor bakes it from the suite's own workspace, so the test takes
    // EngineAssetsDir() as given and asserts the ROUND TRIP, which is the rule under test.
    namespace fs = std::filesystem;

    const fs::path lRoot = fs::temp_directory_path() / "OpaaxPathsTests_mount";

    std::error_code lError;
    fs::remove_all(lRoot, lError);
    fs::create_directories(lRoot / "Assets" / "Textures", lError);

    StubPlatform lPlatform(OpaaxString("W:/deploy/bin/Game.exe"));
    std::string  lProject = (lRoot / "MyGame.opaaxproj").string();

    char  lArg0[] = "Game.exe";
    char  lArg1[] = "--project";
    char* lArgv[] = { lArg0, lArg1, lProject.data() };

    const Paths lPaths(lPlatform, 3, lArgv);

    SUBCASE("an engine file round-trips through its mount")
    {
        const OpaaxString lAbs = lPaths.AssetToAbsolute(OpaaxString("/Engine/Textures/T_Checker_64.png"));
        CHECK(lPaths.AbsoluteToAsset(lAbs) == OpaaxString("/Engine/Textures/T_Checker_64.png"));
    }

    SUBCASE("a project file still round-trips UNPREFIXED — the project is tried first")
    {
        const OpaaxString lAbs = lPaths.AssetToAbsolute(OpaaxString("Textures/Hero.png"));
        CHECK(lPaths.AbsoluteToAsset(lAbs) == OpaaxString("Textures/Hero.png"));
    }

    SUBCASE("a file under NEITHER root still has no asset name")
    {
        const OpaaxString lOutside((lRoot / "Elsewhere.png").generic_string().c_str());
        CHECK(lPaths.AbsoluteToAsset(lOutside).IsEmpty());
    }

    fs::remove_all(lRoot, lError);
}

// =============================================================================
// Null object
// =============================================================================
TEST_CASE("IPaths: the null object is empty and safe via the locator")
{
    AppServiceLocator lLocator;
    IPaths& lPaths = lLocator.Get<IPaths>(); // never provided

    CHECK(lPaths.IsNull());
    CHECK(lPaths.WorkspaceRoot().IsEmpty());
    CHECK(lPaths.EngineRoot().IsEmpty());
    CHECK(lPaths.ProjectRoot().IsEmpty());
    CHECK(lPaths.AssetToAbsolute(OpaaxString("x")).IsEmpty());
    CHECK(lPaths.EngineToAbsolute(OpaaxString("x")).IsEmpty());
    CHECK(&lPaths == &IPaths::Null());
}
