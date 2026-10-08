// Suite: GameExportPlan — the commands that export a game (configure the release preset, build the
// game, install its component into a folder), and the checks on what goes into them. Text only:
// the export itself runs CMake, which the CI release jobs exercise through the same install rules.
#include <doctest.h>

#include <string>

#include "Editor/Export/GameExportPlan.hpp"

using namespace Opaax;
using namespace Opaax::Editor;

TEST_CASE("GameExportPlan: configure the release preset, build the game, install it into the folder")
{
    const TDynArray<ExportStep> lSteps = GameExportPlan::Make("TestWorld", "C:/Exports/My Game");
    REQUIRE(lSteps.size() == 3);

    CHECK(lSteps[0].Command == "cmake --preset release");
    CHECK(lSteps[1].Command == "cmake --build build/release --config Release --target TestWorld");
    CHECK(lSteps[2].Command
          == "cmake --install build/release --config Release --component TestWorld --prefix \"C:/Exports/My Game\"");
    CHECK(lSteps[2].Description.find("C:/Exports/My Game") != std::string::npos);
}

TEST_CASE("GameExportPlan: a game name is an identifier, a path holds no quote: anything else makes no steps")
{
    CHECK(GameExportPlan::IsValidGameName("Sandbox"));
    CHECK(GameExportPlan::IsValidGameName("_My_Game2"));
    CHECK_FALSE(GameExportPlan::IsValidGameName(""));
    CHECK_FALSE(GameExportPlan::IsValidGameName("2Fast"));
    CHECK_FALSE(GameExportPlan::IsValidGameName("Game && del *"));
    CHECK_FALSE(GameExportPlan::IsValidGameName("My-Game"));

    CHECK(GameExportPlan::IsSafePath("/home/me/exports"));
    CHECK_FALSE(GameExportPlan::IsSafePath(""));
    CHECK_FALSE(GameExportPlan::IsSafePath("C:/out\" & calc & \""));
    CHECK_FALSE(GameExportPlan::IsSafePath("C:/out\nrm -rf /"));

    CHECK(GameExportPlan::Make("Bad Name", "C:/out").empty());
    CHECK(GameExportPlan::Make("Game", "C:/\"out\"").empty());
}

TEST_CASE("GameExportPlan: a step runs from the workspace, its output added to the log")
{
    const std::string lPosix = GameExportPlan::ShellLine("cmake --preset release", "/src/opaax", "/tmp/export.log", false);
    CHECK(lPosix == "cd \"/src/opaax\" && cmake --preset release >> \"/tmp/export.log\" 2>&1");

    // cmd.exe /c strips one pair of quotes around the whole line.
    const std::string lWindows = GameExportPlan::ShellLine("cmake --preset release", "W:/opaax", "W:/export.log", true);
    CHECK(lWindows == "\"cd /d \"W:/opaax\" && cmake --preset release >> \"W:/export.log\" 2>&1\"");
}
