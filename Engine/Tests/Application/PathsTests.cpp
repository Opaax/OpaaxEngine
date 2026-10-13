// Suite: IPaths robustness. The resolution is a PURE function (ResolveProjectLayout)
// driven with controlled inputs — both the editor (source-workspace) and release
// (exe-dir) branches — with zero dependency on build-time defines. The Paths ctor is
// then smoke-tested with an absolute --project (workspace-independent, so deterministic
// regardless of the suite's own OPAAX_WORKSPACE_DIR).
// Fixture paths are built with Abs(): "W:/repo" is absolute on Windows only.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileSystem.h"
#include "Core/OpaaxTypes.h"

#include "Application/Services/IPaths.h"
#include "Platform/CommandLine.h"
#include "Platform/IPlatform.h"
#include "Platform/IFileSystem.h"   // StubPlatform hands out the null one
#include "Application/Services/AppServiceLocator.h"

using namespace Opaax;

namespace
{
    class StubPlatform final : public IPlatform
    {
    public:
        explicit StubPlatform(OpaaxString InExe) : m_Exe(Move(InExe)) {}
        Uint32      GetLogicalCoreCount() const override { return 4; }
        double      GetTimeSeconds()      const override { return 0.0; }
        OpaaxString GetExecutablePath()   const override { return m_Exe; }
        OpaaxString GetPlatformName()     const override { return OpaaxString("Stub"); }
        const IFileSystem& GetFileSystem() const override { return IFileSystem::Null(); }  // unused here
    private:
        OpaaxString m_Exe;
    };

    /** An absolute path on this platform: "<InDrive>:/<InRest>" on Windows, "/<InRest>" elsewhere. */
    OpaaxString Abs(const char* InRest, [[maybe_unused]] const char InDrive = 'W')
    {
#ifdef OPAAX_PLATFORM_WINDOWS
        return OpaaxString::Format("{}:/{}", InDrive, InRest);
#else
        return OpaaxString::Format("/{}", InRest);
#endif
    }
}

// =============================================================================
// Pure resolver — robustness
// =============================================================================
TEST_CASE("ResolveProjectLayout: editor build resolves to the SOURCE workspace, not the binary")
{
    // Editor: workspace dir is baked (source tree); the binary lives in a build output dir.
    const ProjectLayout lLayout = ResolveProjectLayout(
        Abs("repo/build/bin/Debug/Game.exe"),
        Abs("repo"),              // OPAAX_WORKSPACE_DIR
        OpaaxString());           // no --project

    CHECK(lLayout.WorkspaceRoot == Abs("repo"));
    CHECK(lLayout.EngineRoot    == Abs("repo/Engine"));
    CHECK(lLayout.ProjectRoot   == Abs("repo/Game"));          // SOURCE folder, not .../build/bin/Debug
    CHECK(lLayout.ProjectFile   == Abs("repo/Game/Game.opaaxproj"));
    CHECK(lLayout.AssetsDir     == Abs("repo/Game/Assets"));
    CHECK(lLayout.ConfigsDir    == Abs("repo/Game/Configs"));
    CHECK(lLayout.SaveDir       == Abs("repo/Game/Save"));
}

TEST_CASE("ResolveProjectLayout: release build (no workspace) anchors on the executable dir")
{
    const ProjectLayout lLayout = ResolveProjectLayout(
        Abs("deploy/bin/Game.exe"),
        OpaaxString(),            // release -> empty
        OpaaxString());

    CHECK(lLayout.WorkspaceRoot == Abs("deploy/bin"));
    CHECK(lLayout.EngineRoot    == Abs("deploy/bin/Engine"));
    CHECK(lLayout.ProjectRoot   == Abs("deploy/bin/Game_Data"));
    CHECK(lLayout.ProjectFile   == Abs("deploy/bin/Game_Data/Game.opaaxproj"));
    CHECK(lLayout.AssetsDir     == Abs("deploy/bin/Game_Data/Assets"));
}

TEST_CASE("ResolveProjectLayout: a shipped executable without extension does not share its project folder's name")
{
    // Linux and macOS: the executable "Game" and a folder "Game" cannot sit in the same directory.
    const ProjectLayout lLayout = ResolveProjectLayout(
        Abs("deploy/bin/Game"),
        OpaaxString(),
        OpaaxString());

    CHECK(lLayout.ProjectRoot == Abs("deploy/bin/Game_Data"));
    CHECK(lLayout.ProjectFile == Abs("deploy/bin/Game_Data/Game.opaaxproj"));
}

TEST_CASE("ResolveProjectLayout: AppName tracks the executable stem")
{
    const ProjectLayout lLayout = ResolveProjectLayout(
        Abs("repo/build/bin/Debug/Sandbox.exe"),
        Abs("repo"),
        OpaaxString());

    CHECK(lLayout.ProjectRoot == Abs("repo/Sandbox"));
    CHECK(lLayout.ProjectFile == Abs("repo/Sandbox/Sandbox.opaaxproj"));
}

TEST_CASE("ResolveProjectLayout: relative --project resolves under the workspace root")
{
    const ProjectLayout lLayout = ResolveProjectLayout(
        Abs("repo/build/bin/Debug/Game.exe"),
        Abs("repo"),
        OpaaxString("Sandbox/Sandbox.opaaxproj")); // workspace-relative

    CHECK(lLayout.ProjectRoot == Abs("repo/Sandbox"));
    CHECK(lLayout.ProjectFile == Abs("repo/Sandbox/Sandbox.opaaxproj"));
    CHECK(lLayout.AssetsDir   == Abs("repo/Sandbox/Assets"));
}

TEST_CASE("ResolveProjectLayout: absolute --project overrides the workspace")
{
    const ProjectLayout lLayout = ResolveProjectLayout(
        Abs("repo/build/bin/Debug/Game.exe"),
        Abs("repo"),
        Abs("external/Cool/Cool.opaaxproj", 'D')); // absolute -> wins

    CHECK(lLayout.ProjectRoot == Abs("external/Cool", 'D'));
    CHECK(lLayout.ProjectFile == Abs("external/Cool/Cool.opaaxproj", 'D'));
    CHECK(lLayout.SaveDir     == Abs("external/Cool/Save", 'D'));
}

// =============================================================================
// Paths ctor + resolvers — deterministic via absolute --project
// =============================================================================
TEST_CASE("Paths: ctor + resolvers (absolute --project is workspace-independent)")
{
    StubPlatform      lPlatform(Abs("deploy/bin/Game.exe"));
    const CommandLine lCommandLine({ "Game.exe", "--project", Abs("proj/MyGame/MyGame.opaaxproj") });

    const Paths lPaths(lPlatform, lCommandLine);

    CHECK(lPaths.ProjectRoot() == Abs("proj/MyGame"));
    CHECK(lPaths.AssetsDir()   == Abs("proj/MyGame/Assets"));
    CHECK(lPaths.AssetToAbsolute(OpaaxString("Scenes/Main.opaaxscene"))
          == Abs("proj/MyGame/Assets/Scenes/Main.opaaxscene"));
    CHECK(lPaths.ProjectToAbsolute(OpaaxString("Source/Player.cpp"))
          == Abs("proj/MyGame/Source/Player.cpp"));

    // EngineRoot depends on the suite's own workspace (editor define) — assert shape only.
    const OpaaxString lEngineRel = lPaths.EngineToAbsolute(OpaaxString("Assets/Shaders"));
    CHECK(lEngineRel.Contains("/Engine/Assets/Shaders"));
}

TEST_CASE("Paths: a non-ASCII --project reaches every path whole")
{
    // An accented folder name in UTF-8, as CommandLine hands it over on every platform.
    StubPlatform      lPlatform(Abs("deploy/bin/Game.exe"));
    const CommandLine lCommandLine({ "Game.exe", "--project", Abs("Zo\xC3\xA9/MyGame/MyGame.opaaxproj") });

    const Paths lPaths(lPlatform, lCommandLine);

    CHECK(lPaths.ProjectRoot() == Abs("Zo\xC3\xA9/MyGame"));
    CHECK(lPaths.AssetsDir()   == Abs("Zo\xC3\xA9/MyGame/Assets"));
    CHECK(lPaths.ProjectFile() == Abs("Zo\xC3\xA9/MyGame/MyGame.opaaxproj"));
}

TEST_CASE("Paths: AbsoluteToAsset is the inverse of AssetToAbsolute")
{
    // Against a REAL directory: AbsoluteToAsset canonicalises BOTH sides, so a fabricated path
    // would be testing how something that does not exist canonicalises rather than the rule itself.
    const TestTempDir lRoot("PathsAsset");
    FileSystem::CreateDirectories(lRoot.Sub("Assets/Maps"));

    StubPlatform      lPlatform(Abs("deploy/bin/Game.exe"));
    const CommandLine lCommandLine({ "Game.exe", "--project", lRoot.Sub("MyGame.opaaxproj") });

    const Paths lPaths(lPlatform, lCommandLine);

    const OpaaxString lAbs = lPaths.AssetToAbsolute(OpaaxString("Maps/Main.opaaxmap"));
    CHECK(lPaths.AbsoluteToAsset(lAbs) == OpaaxString("Maps/Main.opaaxmap"));

#ifdef OPAAX_PLATFORM_WINDOWS
    SUBCASE("a NATIVE path with backslashes lands on the same answer")
    {
        // The case the function exists for: a file dialog answers `C:\...\Maps\Main.opaaxmap`
        // while AssetsDir was built with forward slashes.
        OpaaxString lNative = lAbs;
        lNative.ReplaceCharInline('/', '\\');

        CHECK(lPaths.AbsoluteToAsset(lNative) == OpaaxString("Maps/Main.opaaxmap"));
    }
#endif

    SUBCASE("a file outside the assets dir has NO asset name")
    {
        // Empty is the real answer, not a failure: such a file cannot be named by a manifest.
        const OpaaxString lOutside = lRoot.Sub("Elsewhere.opaaxmap");
        CHECK(lPaths.AbsoluteToAsset(lOutside).IsEmpty());
    }

    SUBCASE("an empty path is refused rather than resolving to the assets dir itself")
    {
        CHECK(lPaths.AbsoluteToAsset(OpaaxString()).IsEmpty());
    }
}

// =============================================================================
// The /Engine/ mount — engine-shipped content is referenceable, project content is not rewritten
// =============================================================================
TEST_CASE("Paths: ENGINE_MOUNT resolves against the engine's assets, not the project's")
{
    StubPlatform      lPlatform(Abs("deploy/bin/Game.exe"));
    const CommandLine lCommandLine({ "Game.exe", "--project", Abs("proj/MyGame/MyGame.opaaxproj") });

    const Paths lPaths(lPlatform, lCommandLine);

    // EngineRoot follows the suite's own workspace (the editor define), so assert the SHAPE — the
    // claim under test is "which root", not "which machine".
    const OpaaxString lMounted = lPaths.AssetToAbsolute(OpaaxString("/Engine/Textures/T_Checker_64.png"));
    CHECK(lMounted.Contains("/Engine/Assets/Textures/T_Checker_64.png"));
    CHECK_FALSE(lMounted.Contains("/MyGame/Assets"));

    CHECK(lPaths.EngineAssetsDir() == lPaths.EngineToAbsolute(OpaaxString("Assets")));

    SUBCASE("an UNPREFIXED path is untouched — every existing .opaaxmap stays valid")
    {
        CHECK(lPaths.AssetToAbsolute(OpaaxString("Textures/Hero.png"))
              == Abs("proj/MyGame/Assets/Textures/Hero.png"));
    }

    SUBCASE("a path merely CONTAINING the mount word is project content")
    {
        // The discriminator is the leading '/', not the word: a project folder may be called Engine.
        CHECK(lPaths.AssetToAbsolute(OpaaxString("Engine/Notes.png"))
              == Abs("proj/MyGame/Assets/Engine/Notes.png"));
    }
}

TEST_CASE("Paths: AbsoluteToAsset names engine content by its mount")
{
    // A real project root under temp, as the inverse test above does. The ENGINE root is NOT
    // fabricated: the ctor bakes it from the suite's own workspace, so the test takes
    // EngineAssetsDir() as given and asserts the ROUND TRIP, which is the rule under test.
    const TestTempDir lRoot("PathsMount");
    FileSystem::CreateDirectories(lRoot.Sub("Assets/Textures"));

    StubPlatform      lPlatform(Abs("deploy/bin/Game.exe"));
    const CommandLine lCommandLine({ "Game.exe", "--project", lRoot.Sub("MyGame.opaaxproj") });

    const Paths lPaths(lPlatform, lCommandLine);

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
        const OpaaxString lOutside = lRoot.Sub("Elsewhere.png");
        CHECK(lPaths.AbsoluteToAsset(lOutside).IsEmpty());
    }
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
