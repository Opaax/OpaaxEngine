// Suite: FindAssetClass (Editor/Resources/AssetFileClass.hpp) — the asset class of one file, as the
// browser shows it and asset.open opens it: named by an .opaaxasset, or the class that reads the file's
// extension.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Editor/Resources/AssetFileClass.hpp"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetFile.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace AssetFileClassTestTypes
{
    // A wave read from a file of its own.
    class ScanWave final : public Asset
    {
        OPAAX_OBJECT(ScanWave, Asset)
        OPAAX_ASSET_SOURCE(".scanwave")

    public:
        bool LoadSource(const char*) override { return true; }
    };

    // An asset class of .opaaxasset files.
    class ScanTuning final : public Asset
    {
        OPAAX_OBJECT(ScanTuning, Asset)

    public:
        Int32 Lives = 3;
        OPAAX_PROPERTIES(ScanTuning, OPAAX_PROP(Lives))
    };
}

using namespace AssetFileClassTestTypes;

TEST_CASE("AssetFileClass: an .opaaxasset's class is the one it names, when that name is known")
{
    const TestTempDir lDir("scan_asset");

    ObjectClassRegistry lClasses;
    REQUIRE(lClasses.Register<ScanTuning>());

    const OpaaxString lTuning = lDir.Sub("Tuning.opaaxasset");
    REQUIRE(AssetFile::Save(lTuning, ScanTuning()));
    CHECK(FindAssetClass(lTuning, &lClasses) == ScanTuning::StaticClass().GetName());

    // Read from the file, not looked up in the registry given.
    CHECK(FindAssetClass(lTuning, nullptr) == ScanTuning::StaticClass().GetName());

    // A name never seen gives none: scanning many files adds no names.
    const OpaaxString lStranger = lDir.Sub("Stranger.opaaxasset");
    FileIO::WriteAllText(lStranger, OpaaxString(R"({ "Class": "AssetFileClassNeverSeen", "Version": 1, "Properties": {} })"));
    CHECK_FALSE(FindAssetClass(lStranger, &lClasses).IsValid());

    CHECK_FALSE(FindAssetClass(lDir.Sub("Missing.opaaxasset"), &lClasses).IsValid());
}

TEST_CASE("AssetFileClass: another file's class is the one that reads its extension, whatever its case")
{
    const TestTempDir lDir("scan_source");

    ObjectClassRegistry lClasses;
    REQUIRE(lClasses.Register<ScanWave>());

    // Read by its extension only: the file need not exist.
    CHECK(FindAssetClass(lDir.Sub("First.scanwave"), &lClasses) == ScanWave::StaticClass().GetName());
    CHECK(FindAssetClass(lDir.Sub("Loud.SCANWAVE"), &lClasses) == ScanWave::StaticClass().GetName());

    // No class reads it, or no classes were given: not an asset.
    CHECK_FALSE(FindAssetClass(lDir.Sub("Notes.txt"), &lClasses).IsValid());
    CHECK_FALSE(FindAssetClass(lDir.Sub("NoExtension"), &lClasses).IsValid());
    CHECK_FALSE(FindAssetClass(lDir.Sub("First.scanwave"), nullptr).IsValid());
}
