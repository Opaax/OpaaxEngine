// Suite: IPaths robustness. The resolution is a PURE function (ResolveProjectLayout)
// driven with controlled inputs — both the editor (source-workspace) and release
// (exe-dir) branches — with zero dependency on build-time defines. The Paths ctor is
// then smoke-tested with an absolute --project (workspace-independent, so deterministic
// regardless of the suite's own OPAAX_WORKSPACE_DIR).
#include <doctest.h>

#include <string>
#include <utility>

#include "Core/Application/Services/IPaths.h"
#include "Core/Application/Services/IPlatform.h"
#include "Core/Application/Services/AppServiceLocator.h"

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
