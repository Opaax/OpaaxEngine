// Suite: data assets — the type registry, the reading rules, the .opaaxdata file, typed access and
// live reload through a held handle.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Core/OpaaxTypes.h"

#include "Resources/ResourceManager.h"
#include "Resources/DataAsset/DataAssetFile.h"
#include "Resources/DataAsset/DataAssetHandle.h"
#include "Resources/DataAsset/DataAssetResource.h"
#include "Resources/DataAsset/DataAssetTypeRegistry.h"

using namespace Opaax;

namespace DataAssetTestTypes
{
    // Defined in the test exe, which is where a game module's types live too.
    struct TestTuning
    {
        float Speed   = 1.f;
        Int32 Lives   = 3;
        bool  bHard   = false;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TestTuning, Speed, Lives, bHard)
        OPAAX_PROPERTIES(TestTuning, OPAAX_PROP(Speed), OPAAX_PROP(Lives), OPAAX_PROP(bHard))
    };

    struct OtherTuning
    {
        float Gravity = 9.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(OtherTuning, Gravity)
        OPAAX_PROPERTIES(OtherTuning, OPAAX_PROP(Gravity))
    };
}

using DataAssetTestTypes::OtherTuning;
using DataAssetTestTypes::TestTuning;

namespace
{
    struct TempDir
    {
        TestTempDir Dir{ "DataAsset" };

        OpaaxString PathOf(const OpaaxStringView InName) const { return Dir.Sub(InName); }

        OpaaxString Write(const OpaaxStringView InName, const OpaaxString& InText) const
        {
            const OpaaxString lPath = PathOf(InName);
            FileIO::WriteAllText(lPath, InText);
            return lPath;
        }
    };
}

// =============================================================================
// The registry
// =============================================================================

TEST_CASE("DataAssetTypeRegistry: a struct registers under its C++ name, once")
{
    DataAssetTypeRegistry lRegistry;

    REQUIRE(lRegistry.Register<TestTuning>());
    CHECK_FALSE(lRegistry.Register<TestTuning>());   // same type twice

    const IDataAssetTypeEntry* lEntry = lRegistry.FindByName(OpaaxStringID("TestTuning"));
    REQUIRE(lEntry != nullptr);
    CHECK(lEntry == lRegistry.FindByTypeId(TypeIdOf<TestTuning>()));
    CHECK(lRegistry.FindByName(OpaaxStringID("OtherTuning")) == nullptr);

    // A default value, through the entry alone.
    const TUniquePtr<IDataAssetObject> lDefault = lEntry->Create();
    CHECK(lDefault->ToJson() == nlohmann::json(TestTuning{}));

    lRegistry.Seal();
    CHECK_FALSE(lRegistry.Register<OtherTuning>());
    CHECK(lRegistry.Count() == 1u);
}

// =============================================================================
// Reading rules
// =============================================================================

TEST_CASE("ReadDataAsset: a missing key keeps its default; a wrong type is refused, not guessed")
{
    const auto lPartial = ReadDataAsset<TestTuning>(nlohmann::json{ { "Speed", 2.5 } });
    REQUIRE(lPartial != nullptr);
    CHECK(lPartial->Value.Speed == doctest::Approx(2.5f));
    CHECK(lPartial->Value.Lives == 3);

    OpaaxString lError;
    CHECK(ReadDataAsset<TestTuning>(nlohmann::json{ { "Lives", "many" } }, &lError) == nullptr);
    CHECK_FALSE(lError.IsEmpty());

    CHECK(ReadDataAsset<TestTuning>(nlohmann::json::array()) == nullptr);
}

// =============================================================================
// The file
// =============================================================================

TEST_CASE("DataAssetFile: saves type + data and reads them back")
{
    TempDir lDir;
    const OpaaxString lPath = lDir.PathOf("Grunt.opaaxdata");

    TestTuning lValue;
    lValue.Speed = 4.f;

    REQUIRE(DataAssetFile::Save(lPath, OpaaxStringID("TestTuning"), nlohmann::json(lValue)));

    DataAssetFile::Contents lRead;
    REQUIRE(DataAssetFile::Load(lPath, lRead));
    CHECK(lRead.Type == OpaaxStringID("TestTuning"));
    CHECK(lRead.Data == nlohmann::json(lValue));
}

TEST_CASE("DataAssetFile: a file without a type name or a data object is refused")
{
    TempDir lDir;
    DataAssetFile::Contents lRead;

    CHECK_FALSE(DataAssetFile::Load(lDir.Write("a.opaaxdata", R"({"Data": {}})"), lRead));
    CHECK_FALSE(DataAssetFile::Load(lDir.Write("b.opaaxdata", R"({"Type": "TestTuning", "Data": 3})"), lRead));
    CHECK_FALSE(DataAssetFile::Load(lDir.Write("c.opaaxdata", R"({"Type": "", "Data": {}})"), lRead));
    CHECK_FALSE(DataAssetFile::Load(lDir.Write("d.opaaxdata", "not json"), lRead));
}

// =============================================================================
// Typed access and live reload
// =============================================================================

TEST_CASE("DataAssetResource: As<T> reads its own type and refuses another")
{
    TempDir lDir;
    TestTuning lValue;
    lValue.Lives = 9;

    const OpaaxString lPath = lDir.PathOf("t.opaaxdata");
    REQUIRE(DataAssetFile::Save(lPath, DataAssetTypeName<TestTuning>(), nlohmann::json(lValue)));

    ResourceManager lManager;
    REQUIRE(lManager.Startup());

    ResourceRef<DataAssetResource> lRef = lManager.Load<DataAssetResource>(lPath.CStr());
    REQUIRE(lRef.IsValid());
    REQUIRE(lRef.Get() != nullptr);

    const TestTuning* lTyped = lRef.Get()->As<TestTuning>();
    REQUIRE(lTyped != nullptr);
    CHECK(lTyped->Lives == 9);

    // Asked twice, the same cached value.
    CHECK(lRef.Get()->As<TestTuning>() == lTyped);

    CHECK(lRef.Get()->As<OtherTuning>() == nullptr);

    lManager.Shutdown();
}

TEST_CASE("TDataAssetHandle: saving the file and reloading changes what a held handle reads")
{
    TempDir lDir;
    const OpaaxString lPath = lDir.PathOf("live.opaaxdata");

    TestTuning lValue;
    lValue.Speed = 1.f;
    REQUIRE(DataAssetFile::Save(lPath, DataAssetTypeName<TestTuning>(), nlohmann::json(lValue)));

    ResourceManager lManager;
    REQUIRE(lManager.Startup());

    const TDataAssetHandle<TestTuning> lHandle(lManager.Load<DataAssetResource>(lPath.CStr()),
                                               OpaaxString("live.opaaxdata"));
    REQUIRE(lHandle.Get() != nullptr);
    CHECK(lHandle.Get()->Speed == doctest::Approx(1.f));

    // What the editor does on Save: write, then reload in place.
    lValue.Speed = 7.f;
    REQUIRE(DataAssetFile::Save(lPath, DataAssetTypeName<TestTuning>(), nlohmann::json(lValue)));
    REQUIRE(lManager.Reload<DataAssetResource>(lPath.CStr()));

    REQUIRE(lHandle.Get() != nullptr);
    CHECK(lHandle.Get()->Speed == doctest::Approx(7.f));

    // A handle of the wrong type reads nothing.
    const TDataAssetHandle<OtherTuning> lWrong(lManager.Load<DataAssetResource>(lPath.CStr()),
                                               OpaaxString("live.opaaxdata"));
    CHECK(lWrong.IsLoaded());
    CHECK(lWrong.Get() == nullptr);

    lManager.Shutdown();
}

TEST_CASE("TDataAssetRef: saved as its path")
{
    TDataAssetRef<TestTuning> lRef;
    lRef.Path = OpaaxString("Data/Grunt.opaaxdata");

    const nlohmann::json lJson = lRef;
    CHECK(lJson == "Data/Grunt.opaaxdata");

    TDataAssetRef<TestTuning> lBack;
    lJson.get_to(lBack);
    CHECK(lBack.Path == lRef.Path);
}
