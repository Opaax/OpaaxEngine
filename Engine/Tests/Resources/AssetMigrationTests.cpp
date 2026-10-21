// Suite: AssetMigration — old per-type asset files brought to .opaaxasset, and files of a class split
// into subclasses brought to their subclass: the text of one file, the paths that name them, and a
// whole run over a project and the engine's content.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/OpaaxPathString.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/Asset/AssetMigration.h"

using namespace Opaax;

namespace
{
    const AssetMigration::LegacyFormat& DataFormat()
    {
        const AssetMigration::LegacyFormat* lFormat = AssetMigration::FindLegacyFormat(OpaaxStringView("x.opaaxdata"));
        REQUIRE(lFormat != nullptr);
        return *lFormat;
    }

    /** Writes InText at InRelative under InRoot, folders included. */
    OpaaxString Write(const OpaaxString& InRoot, const char* InRelative, const char* InText)
    {
        const OpaaxString lPath = PathString::Join(InRoot, OpaaxStringView(InRelative));
        FileSystem::CreateDirectories(PathString::Parent(lPath));
        FileIO::WriteAllText(lPath, OpaaxString(InText));
        return lPath;
    }

    AssetFile::Contents ReadAsset(const OpaaxString& InPath)
    {
        AssetFile::Contents lContents;
        CHECK(AssetFile::Load(InPath, lContents));
        return lContents;
    }

    /** The file as written, without the upgrade AssetFile::Load applies. */
    AssetFile::Contents ParseAsset(const OpaaxString& InPath)
    {
        AssetFile::Contents lContents;
        CHECK(AssetFile::Parse(FileIO::ReadAllText(InPath), lContents));
        return lContents;
    }

    AssetFile::Contents MakeContents(const char* InClass, nlohmann::json InProperties)
    {
        AssetFile::Contents lContents;
        lContents.Class      = OpaaxStringID(InClass);
        lContents.Properties = Move(InProperties);
        return lContents;
    }
}

// =============================================================================
// One file
// =============================================================================

TEST_CASE("AssetMigration: an old format is found by its extension, in any case")
{
    CHECK(AssetMigration::FindLegacyFormat(OpaaxStringView("Data/Grunt.opaaxdata")) != nullptr);
    CHECK(AssetMigration::FindLegacyFormat(OpaaxStringView("Data/Grunt.OpaaxData")) != nullptr);
    CHECK(AssetMigration::FindLegacyFormat(OpaaxStringView("Data/Grunt.opaaxasset")) == nullptr);
    CHECK(AssetMigration::FindLegacyFormat(OpaaxStringView("Maps/Main.opaaxmap")) == nullptr);
    CHECK(AssetMigration::FindLegacyFormat(OpaaxStringView("opaaxdata")) == nullptr);

    for (const AssetMigration::LegacyFormat& lFormat : AssetMigration::GetLegacyFormats())
    {
        CHECK(OpaaxString(lFormat.Extension).StartsWith("."));
    }
}

TEST_CASE("AssetMigration: a path written in code finds the file its old one was converted to")
{
    CHECK(AssetMigration::ConvertedPath(OpaaxStringView("Input/Gameplay.opaaxinputmap")) == "Input/Gameplay.opaaxasset");
    CHECK(AssetMigration::ConvertedPath(OpaaxStringView("/Engine/Data/Grunt.OpaaxData")) == "/Engine/Data/Grunt.opaaxasset");

    // Not an old format: nothing to redirect.
    CHECK(AssetMigration::ConvertedPath(OpaaxStringView("Input/Gameplay.opaaxasset")).IsEmpty());
    CHECK(AssetMigration::ConvertedPath(OpaaxStringView("Maps/Main.opaaxmap")).IsEmpty());
    CHECK(AssetMigration::ConvertedPath(OpaaxStringView("")).IsEmpty());
}

TEST_CASE("AssetMigration: a data asset's type becomes the class, its data the properties")
{
    OpaaxString lAssetText;
    REQUIRE(AssetMigration::ConvertText(OpaaxString(R"({ "Type": "EnemyStats", "Data": { "Speed": 2.0 } })"),
                                        DataFormat(), lAssetText));

    AssetFile::Contents lContents;
    REQUIRE(AssetFile::Parse(lAssetText, lContents));
    CHECK(lContents.Class == OPAAX_ID("EnemyStats"));
    CHECK(lContents.Properties == nlohmann::json{ { "Speed", 2.0 } });
    CHECK(lAssetText == AssetFile::Serialize(OPAAX_ID("EnemyStats"), nlohmann::json{ { "Speed", 2.0 } }));

    OpaaxString lError;
    CHECK_FALSE(AssetMigration::ConvertText(OpaaxString("not json"), DataFormat(), lAssetText, &lError));
    CHECK_FALSE(lError.IsEmpty());
    CHECK_FALSE(AssetMigration::ConvertText(OpaaxString(R"({ "Data": {} })"), DataFormat(), lAssetText));
    CHECK_FALSE(AssetMigration::ConvertText(OpaaxString(R"({ "Type": "X", "Data": 3 })"), DataFormat(), lAssetText));
}

// =============================================================================
// A class split into subclasses
// =============================================================================

TEST_CASE("AssetMigration: a file of a split class names its subclass, and loses the property that named it")
{
    AssetFile::Contents lGround = MakeContents("MoveMode", { { "Mode", "GroundMove" }, { "MaxSpeed", 420.0 } });
    REQUIRE(AssetMigration::UpgradeSplitClass(lGround));
    CHECK(lGround.Class == OPAAX_ID("GroundMoveMode"));
    CHECK(lGround.Properties == nlohmann::json{ { "MaxSpeed", 420.0 } });

    // Already upgraded, another class, or no property saying which: left as it is.
    CHECK_FALSE(AssetMigration::UpgradeSplitClass(lGround));

    AssetFile::Contents lOther = MakeContents("Mover", { { "Mode", "GroundMove" } });
    CHECK_FALSE(AssetMigration::UpgradeSplitClass(lOther));
    CHECK(lOther.Class == OPAAX_ID("Mover"));

    AssetFile::Contents lUnnamed = MakeContents("MoveMode", { { "MaxSpeed", 1.0 } });
    CHECK_FALSE(AssetMigration::UpgradeSplitClass(lUnnamed));
    CHECK(lUnnamed.Class == OPAAX_ID("MoveMode"));

    AssetFile::Contents lNotText = MakeContents("MoveMode", { { "Mode", 3 } });
    CHECK_FALSE(AssetMigration::UpgradeSplitClass(lNotText));
}

TEST_CASE("AssetMigration: an old .opaaxmovemode file converts straight to its mode's class")
{
    const AssetMigration::LegacyFormat* lFormat = AssetMigration::FindLegacyFormat(OpaaxStringView("Hero.opaaxmovemode"));
    REQUIRE(lFormat != nullptr);

    OpaaxString lAssetText;
    REQUIRE(AssetMigration::ConvertText(OpaaxString(R"({ "Mode": "FlyMove", "MaxSpeed": 600.0 })"), *lFormat, lAssetText));

    AssetFile::Contents lContents;
    REQUIRE(AssetFile::Parse(lAssetText, lContents));
    CHECK(lContents.Class == OPAAX_ID("FlyMoveMode"));
    CHECK(lContents.Properties == nlohmann::json{ { "MaxSpeed", 600.0 } });
}

TEST_CASE("AssetMigration: loading reads an unconverted file as its subclass, and leaves the file alone")
{
    TestTempDir       lDir("AssetMigrationLoad");
    const OpaaxString lPath = Write(lDir.Path(), "Hero_Ground.opaaxasset",
                                    R"({ "Class": "MoveMode", "Properties": { "Mode": "GroundMove", "JumpSpeed": 610.0 } })");

    const AssetFile::Contents lRead = ReadAsset(lPath);
    CHECK(lRead.Class == OPAAX_ID("GroundMoveMode"));
    CHECK(lRead.Properties == nlohmann::json{ { "JumpSpeed", 610.0 } });

    CHECK(ParseAsset(lPath).Class == OPAAX_ID("MoveMode"));
}

// =============================================================================
// Paths
// =============================================================================

TEST_CASE("AssetMigration: only quoted old paths are offered, and the text around them is kept")
{
    OpaaxString lText(
        "{\n"
        "    \"Stats\": \"Data/Grunt.opaaxdata\",\n"
        "    \"Note\": \"say \\\"Data/Grunt.opaaxdata\\\"\",\n"
        "    \"Texture\": \"Textures/Hero.png\",\n"
        "    \"Other\": \"Data/Unknown.opaaxdata\"\n"
        "}");

    TDynArray<OpaaxString> lOffered;
    const Uint32 lChanged = AssetMigration::RewritePaths(lText, [&lOffered](const OpaaxStringView InPath)
    {
        lOffered.emplace_back(InPath.ToString());
        return InPath == OpaaxStringView("Data/Grunt.opaaxdata") ? OpaaxString("Data/Grunt.opaaxasset") : OpaaxString();
    });

    CHECK(lChanged == 1);
    CHECK(lText ==
        "{\n"
        "    \"Stats\": \"Data/Grunt.opaaxasset\",\n"
        "    \"Note\": \"say \\\"Data/Grunt.opaaxdata\\\"\",\n"
        "    \"Texture\": \"Textures/Hero.png\",\n"
        "    \"Other\": \"Data/Unknown.opaaxdata\"\n"
        "}");

    // The texture was never offered; the escaped one is part of a longer string.
    REQUIRE(lOffered.size() == 2);
    CHECK(lOffered[0] == "Data/Grunt.opaaxdata");
    CHECK(lOffered[1] == "Data/Unknown.opaaxdata");
}

// =============================================================================
// A whole run
// =============================================================================

TEST_CASE("AssetMigration: a project and the engine's content brought to .opaaxasset, paths and configs included")
{
    TestTempDir       lDir("AssetMigration");
    const OpaaxString lProject = lDir.Sub("Game/Assets");
    const OpaaxString lEngine  = lDir.Sub("Engine/Assets");
    const OpaaxString lConfigs = lDir.Sub("Game/Configs");

    Write(lProject, "Data/Grunt.opaaxdata", R"({ "Type": "EnemyStats", "Data": { "Speed": 3.0 } })");
    Write(lProject, "Data/Boss.opaaxdata", R"({ "Type": "EnemyStats", "Data": { "Speed": 9.0 } })");
    Write(lEngine, "Data/Shared.opaaxdata", R"({ "Type": "Material2D", "Data": { "bLit": true } })");

    // A name already taken: the converted file gets its class appended.
    Write(lProject, "Data/Boss.opaaxasset", R"({ "Class": "SomethingElse" })");

    const OpaaxString lMap = Write(lProject, "Maps/Main.opaaxmap",
        R"({ "entities": [ { "components": { "Enemy": { "Stats": "Data/Grunt.opaaxdata", "Boss": "Data/Boss.opaaxdata" },)"
        R"( "Sprite": { "Material": "/Engine/Data/Shared.opaaxdata", "Texture": "Textures/Hero.png" } } } ] })");
    const OpaaxString lConfig = Write(lConfigs, "Engine.config", R"({ "Audio": { "GlobalBank": "Data/Grunt.opaaxdata" } })");
    const OpaaxString lBroken = Write(lProject, "Data/Broken.opaaxdata", "{");

    const TDynArray<AssetMigration::AssetRoot> lRoots = { { lProject, OpaaxString() }, { lEngine, OpaaxString("/Engine/") } };

    const AssetMigration::Report lReport = AssetMigration::Migrate(lRoots, { lConfigs });

    // Converted: each old file is gone, its asset file holds its class and values.
    REQUIRE(lReport.Converted.size() == 3);
    CHECK_FALSE(FileSystem::Exists(PathString::Join(lProject, OpaaxStringView("Data/Grunt.opaaxdata"))));
    CHECK(ReadAsset(PathString::Join(lProject, OpaaxStringView("Data/Grunt.opaaxasset"))).Class == OPAAX_ID("EnemyStats"));
    CHECK(ReadAsset(PathString::Join(lEngine, OpaaxStringView("Data/Shared.opaaxasset"))).Class == OPAAX_ID("Material2D"));

    const AssetFile::Contents lBoss = ReadAsset(PathString::Join(lProject, OpaaxStringView("Data/Boss_EnemyStats.opaaxasset")));
    CHECK(lBoss.Properties == nlohmann::json{ { "Speed", 9.0 } });
    CHECK(ReadAsset(PathString::Join(lProject, OpaaxStringView("Data/Boss.opaaxasset"))).Class == OPAAX_ID("SomethingElse"));

    // A file that does not convert keeps its old form, and is reported.
    CHECK(FileSystem::Exists(lBroken));
    REQUIRE(lReport.Errors.size() == 1);
    CHECK(lReport.Errors[0].Contains("Broken.opaaxdata"));

    // Every path that named an old file now names its asset file: project, engine mount, config.
    const OpaaxString lMapText = FileIO::ReadAllText(lMap);
    CHECK(lMapText.Contains("\"Data/Grunt.opaaxasset\""));
    CHECK(lMapText.Contains("\"Data/Boss_EnemyStats.opaaxasset\""));
    CHECK(lMapText.Contains("\"/Engine/Data/Shared.opaaxasset\""));
    CHECK(lMapText.Contains("\"Textures/Hero.png\""));
    CHECK_FALSE(lMapText.Contains(".opaaxdata"));
    CHECK(FileIO::ReadAllText(lConfig).Contains("\"Data/Grunt.opaaxasset\""));
    CHECK(lReport.Rewritten.size() == 2);

    // Nothing left to do the second time.
    const AssetMigration::Report lAgain = AssetMigration::Migrate(lRoots, { lConfigs });
    CHECK(lAgain.Converted.empty());
    CHECK(lAgain.Rewritten.empty());
}

TEST_CASE("AssetMigration: a run rewrites every file of a split class in its subclass, once")
{
    TestTempDir       lDir("AssetMigrationSplit");
    const OpaaxString lProject = lDir.Sub("Game/Assets");

    const OpaaxString lFly    = Write(lProject, "Movers/Hero_Fly.opaaxasset",
                                      R"({ "Class": "MoveMode", "Properties": { "Mode": "FlyMove", "MaxSpeed": 600.0 } })");
    const OpaaxString lMover  = Write(lProject, "Movers/Hero.opaaxasset",
                                      R"({ "Class": "Mover", "Properties": { "DefaultMode": "Fly" } })");
    const OpaaxString lLegacy = Write(lProject, "Movers/Hero_Ground.opaaxmovemode", R"({ "Mode": "GroundMove" })");

    const TDynArray<AssetMigration::AssetRoot> lRoots = { { lProject, OpaaxString() } };

    const AssetMigration::Report lReport = AssetMigration::Migrate(lRoots);

    // On disk now, not only when loaded; the old-format file went straight to its subclass.
    REQUIRE(lReport.Upgraded.size() == 1);
    CHECK(lReport.Upgraded[0] == lFly);
    CHECK(ParseAsset(lFly).Class == OPAAX_ID("FlyMoveMode"));
    CHECK(ParseAsset(lFly).Properties == nlohmann::json{ { "MaxSpeed", 600.0 } });
    CHECK(ParseAsset(lMover).Class == OPAAX_ID("Mover"));

    REQUIRE(lReport.Converted.size() == 1);
    CHECK_FALSE(FileSystem::Exists(lLegacy));
    CHECK(ParseAsset(PathString::Join(lProject, OpaaxStringView("Movers/Hero_Ground.opaaxasset"))).Class
          == OPAAX_ID("GroundMoveMode"));
    CHECK(lReport.Errors.empty());

    const AssetMigration::Report lAgain = AssetMigration::Migrate(lRoots);
    CHECK(lAgain.Upgraded.empty());
    CHECK(lAgain.Converted.empty());
}

TEST_CASE("AssetMigration: paths to the engine's files converted by another project's run are rewritten too")
{
    TestTempDir       lDir("AssetMigrationShared");
    const OpaaxString lEngine = lDir.Sub("Engine/Assets");
    const OpaaxString lOther  = lDir.Sub("Other/Assets");

    // The engine's content already converted by the first project's run.
    Write(lEngine, "Fonts/Roboto.opaaxasset", R"({ "Class": "FontFamily" })");

    const OpaaxString lUi = Write(lOther, "UI/Hud.opaaxui", R"({ "Font": "/Engine/Fonts/Roboto.opaaxdata", "Gone": "Data/Missing.opaaxdata" })");

    const AssetMigration::Report lReport =
        AssetMigration::Migrate({ { lOther, OpaaxString() }, { lEngine, OpaaxString("/Engine/") } });

    CHECK(lReport.Converted.empty());
    const OpaaxString lText = FileIO::ReadAllText(lUi);
    CHECK(lText.Contains("\"/Engine/Fonts/Roboto.opaaxasset\""));
    CHECK(lText.Contains("\"Data/Missing.opaaxdata\""));   // no file to point at: left as it was
}
