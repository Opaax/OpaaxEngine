// Suite: assets — the .opaaxasset file, making an asset from the class it names, loading and sharing
// through the resource manager, typed access, live reload, and asset references in fields.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Core/OpaaxTypes.h"
#include "Engine/Reflection/PropertyJson.h"
#include "Engine/Reflection/PropertyVisitor.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/Asset/AssetHandle.h"
#include "Resources/Asset/AssetRef.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceManager.h"

using namespace Opaax;

namespace AssetTestTypes
{
    class TestTuningAsset : public Asset
    {
        OPAAX_OBJECT(TestTuningAsset, Asset)

    public:
        float Speed = 1.f;
        Int32 Lives = 3;

        /** Derived on load, never saved. */
        Int32 LivesTimesTwo = 0;

        OPAAX_PROPERTIES(TestTuningAsset, OPAAX_PROP(Speed), OPAAX_PROP(Lives))

        void PostLoad() override { LivesTimesTwo = Lives * 2; }
    };

    // A subclass: a field for the base class accepts it.
    class TestHardTuningAsset final : public TestTuningAsset
    {
        OPAAX_OBJECT(TestHardTuningAsset, TestTuningAsset)

    public:
        bool bNightmare = true;
        OPAAX_PROPERTIES(TestHardTuningAsset, OPAAX_PROP(bNightmare))
    };

    class TestOtherAsset final : public Asset
    {
        OPAAX_OBJECT(TestOtherAsset, Asset)

    public:
        float Gravity = 9.f;
        OPAAX_PROPERTIES(TestOtherAsset, OPAAX_PROP(Gravity))
    };

    // An Object that is not an Asset: a file cannot hold one.
    class TestNotAnAsset final : public Object
    {
        OPAAX_OBJECT(TestNotAnAsset, Object)
    };

    struct TestAssetUser
    {
        TAssetRef<TestTuningAsset> Tuning;

        OPAAX_PROPERTIES(TestAssetUser, OPAAX_PROP(Tuning))
    };
}

using namespace AssetTestTypes;

namespace
{
    struct TempDir
    {
        TestTempDir Dir{ "Asset" };

        OpaaxString PathOf(const OpaaxStringView InName) const { return Dir.Sub(InName); }

        OpaaxString Write(const OpaaxStringView InName, const OpaaxString& InText) const
        {
            const OpaaxString lPath = PathOf(InName);
            FileIO::WriteAllText(lPath, InText);
            return lPath;
        }
    };

    /** The classes a game would register. */
    struct TestClasses
    {
        ObjectClassRegistry Registry;

        TestClasses()
        {
            Registry.Register<Asset>();
            Registry.Register<TestTuningAsset>();
            Registry.Register<TestHardTuningAsset>();
            Registry.Register<TestOtherAsset>();
            Registry.Register<TestNotAnAsset>();
        }
    };
}

// =============================================================================
// The file
// =============================================================================

TEST_CASE("AssetFile: class, properties and version, keys sorted, as the editor writes them")
{
    TestTuningAsset lAsset;
    lAsset.Speed = 2.5f;

    const OpaaxString lText = AssetFile::Serialize(lAsset);

    const OpaaxString lExpected =
        "{\n"
        "    \"Class\": \"TestTuningAsset\",\n"
        "    \"Properties\": {\n"
        "        \"Lives\": 3,\n"
        "        \"Speed\": 2.5\n"
        "    },\n"
        "    \"Version\": 1\n"
        "}";
    CHECK(lText == lExpected);

    AssetFile::Contents lContents;
    REQUIRE(AssetFile::Parse(lText, lContents));
    CHECK(lContents.Class == OPAAX_ID("TestTuningAsset"));
    CHECK(lContents.Properties == nlohmann::json{ { "Lives", 3 }, { "Speed", 2.5f } });
    CHECK(lContents.Version == 1);
}

TEST_CASE("AssetFile: what a file must have, and what it may leave out")
{
    AssetFile::Contents lContents;
    OpaaxString         lError;

    // Written by hand: no properties and no version is a valid file, every field at its default.
    REQUIRE(AssetFile::Parse(OpaaxString(R"({ "Class": "TestTuningAsset" })"), lContents));
    CHECK(lContents.Properties == nlohmann::json::object());
    CHECK(lContents.Version == AssetFile::FORMAT_VERSION);

    CHECK_FALSE(AssetFile::Parse(OpaaxString("not json"), lContents, &lError));
    CHECK_FALSE(lError.IsEmpty());
    CHECK_FALSE(AssetFile::Parse(OpaaxString(R"({ "Properties": {} })"), lContents));
    CHECK_FALSE(AssetFile::Parse(OpaaxString(R"({ "Class": "" })"), lContents));
    CHECK_FALSE(AssetFile::Parse(OpaaxString(R"({ "Class": 4 })"), lContents));
    CHECK_FALSE(AssetFile::Parse(OpaaxString(R"({ "Class": "TestTuningAsset", "Properties": [] })"), lContents));
    CHECK_FALSE(AssetFile::Parse(OpaaxString(R"({ "Class": "TestTuningAsset", "Version": "one" })"), lContents));

    // A newer engine's file is refused rather than misread.
    lError = OpaaxString();
    CHECK_FALSE(AssetFile::Parse(OpaaxString(R"({ "Class": "TestTuningAsset", "Version": 2 })"), lContents, &lError));
    CHECK(lError.Contains("newer"));
}

TEST_CASE("AssetFile: saved and loaded back, named after its file")
{
    TempDir     lDir;
    TestClasses lClasses;

    TestHardTuningAsset lAsset;
    lAsset.Lives      = 7;
    lAsset.bNightmare = false;

    const OpaaxString lPath = lDir.PathOf("Hard.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lAsset));

    const TStrongObjectPtr<Asset> lLoaded = AssetFile::LoadAsset(lPath, lClasses.Registry);
    REQUIRE(lLoaded.IsValid());
    CHECK(lLoaded->GetName() == OPAAX_ID("Hard"));

    const TestHardTuningAsset* lTyped = Cast<TestHardTuningAsset>(lLoaded.Get());
    REQUIRE(lTyped != nullptr);
    CHECK(lTyped->Lives == 7);           // a parent's property
    CHECK_FALSE(lTyped->bNightmare);     // its own
    CHECK(lTyped->LivesTimesTwo == 14);  // PostLoad ran

    CHECK_FALSE(AssetFile::LoadAsset(lDir.PathOf("Missing.opaaxasset"), lClasses.Registry).IsValid());
}

// =============================================================================
// From the class a file names
// =============================================================================

TEST_CASE("AssetFile::Instantiate: the class a file names must be a registered, concrete Asset")
{
    TestClasses lClasses;
    OpaaxString lError;

    AssetFile::Contents lContents;
    lContents.Class      = OPAAX_ID("TestOtherAsset");
    lContents.Properties = nlohmann::json{ { "Gravity", 3.f } };

    const TStrongObjectPtr<Asset> lMade = AssetFile::Instantiate(lContents, lClasses.Registry, OPAAX_ID("Moon"));
    REQUIRE(lMade.IsValid());
    CHECK(lMade->IsA<TestOtherAsset>());
    CHECK(lMade->GetName() == OPAAX_ID("Moon"));
    CHECK(lMade->HasAnyFlags(EObjectFlags::Managed));
    CHECK(Cast<TestOtherAsset>(lMade.Get())->Gravity == 3.f);

    lContents.Class = OPAAX_ID("NoSuchAsset");
    CHECK_FALSE(AssetFile::Instantiate(lContents, lClasses.Registry, {}, &lError).IsValid());
    CHECK(lError.Contains("NoSuchAsset"));

    lContents.Class = OPAAX_ID("Asset");   // abstract
    CHECK_FALSE(AssetFile::Instantiate(lContents, lClasses.Registry, {}).IsValid());

    lContents.Class = OPAAX_ID("TestNotAnAsset");
    CHECK_FALSE(AssetFile::Instantiate(lContents, lClasses.Registry, {}).IsValid());
}

TEST_CASE("AssetFile::Instantiate: a missing value keeps its default; a wrong type fails, naming the field")
{
    TestClasses lClasses;
    OpaaxString lError;

    AssetFile::Contents lContents;
    lContents.Class      = OPAAX_ID("TestTuningAsset");
    lContents.Properties = nlohmann::json{ { "Speed", 4.f } };

    const TStrongObjectPtr<Asset> lPartial = AssetFile::Instantiate(lContents, lClasses.Registry, {});
    REQUIRE(lPartial.IsValid());
    CHECK(Cast<TestTuningAsset>(lPartial.Get())->Speed == 4.f);
    CHECK(Cast<TestTuningAsset>(lPartial.Get())->Lives == 3);

    lContents.Properties = nlohmann::json{ { "Lives", "many" } };
    CHECK_FALSE(AssetFile::Instantiate(lContents, lClasses.Registry, {}, &lError).IsValid());
    CHECK(lError.Contains("Lives"));
}

// =============================================================================
// Loaded and shared as a resource
// =============================================================================

TEST_CASE("AssetResource: loaded through the resource manager, read as its class or a parent")
{
    TempDir     lDir;
    TestClasses lClasses;

    TestHardTuningAsset lAsset;
    lAsset.Speed = 6.f;
    const OpaaxString lPath = lDir.PathOf("Shared.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lAsset));

    ResourceManager lManager;
    lManager.SetObjectClasses(&lClasses.Registry);
    REQUIRE(lManager.Startup());

    {
        ResourceRef<AssetResource> lRef = lManager.Load<AssetResource>(lPath.CStr());
        REQUIRE(lRef.Get() != nullptr);
        CHECK(lRef.Get()->ClassName == OPAAX_ID("TestHardTuningAsset"));

        const TestTuningAsset* lAsBase = lRef.Get()->Get<TestTuningAsset>();
        REQUIRE(lAsBase != nullptr);
        CHECK(lAsBase->Speed == 6.f);
        CHECK(lRef.Get()->Get<TestHardTuningAsset>() == lAsBase);
        CHECK(lRef.Get()->Get<TestOtherAsset>() == nullptr);

        // Loaded once, shared.
        ResourceRef<AssetResource> lSecond = lManager.Load<AssetResource>(lPath.CStr());
        CHECK(lSecond.Get()->Instance.Get() == lRef.Get()->Instance.Get());
        CHECK(lManager.GetLoadedCount<AssetResource>() == 1u);
    }

    lManager.Shutdown();
}

TEST_CASE("AssetResource: a file that does not load leaves no asset to read")
{
    TempDir     lDir;
    TestClasses lClasses;

    const OpaaxString lUnknown = lDir.Write("Unknown.opaaxasset", OpaaxString(R"({ "Class": "Nobody" })"));
    const OpaaxString lBroken  = lDir.Write("Broken.opaaxasset", OpaaxString("{"));

    ResourceManager lManager;
    lManager.SetObjectClasses(&lClasses.Registry);
    REQUIRE(lManager.Startup());

    CHECK(lManager.Load<AssetResource>(lUnknown.CStr()).Get()->Get<TestTuningAsset>() == nullptr);
    CHECK(lManager.Load<AssetResource>(lBroken.CStr()).Get()->Get<TestTuningAsset>() == nullptr);

    lManager.Shutdown();

    // A manager given no classes cannot read any asset.
    TestTuningAsset lAsset;
    const OpaaxString lGood = lDir.PathOf("Good.opaaxasset");
    REQUIRE(AssetFile::Save(lGood, lAsset));

    ResourceManager lBare;
    REQUIRE(lBare.Startup());
    CHECK(lBare.Load<AssetResource>(lGood.CStr()).Get()->Get<TestTuningAsset>() == nullptr);
    lBare.Shutdown();
}

TEST_CASE("TAssetHandle: saving the file and reloading changes what a held handle reads")
{
    TempDir     lDir;
    TestClasses lClasses;

    TestTuningAsset lAsset;
    lAsset.Speed = 1.f;
    const OpaaxString lPath = lDir.PathOf("Live.opaaxasset");
    REQUIRE(AssetFile::Save(lPath, lAsset));

    ResourceManager lManager;
    lManager.SetObjectClasses(&lClasses.Registry);
    REQUIRE(lManager.Startup());

    const TAssetHandle<TestTuningAsset> lHandle(lManager.Load<AssetResource>(lPath.CStr()), OpaaxString("Live.opaaxasset"));
    REQUIRE(lHandle.Get() != nullptr);
    CHECK(lHandle.Get()->Speed == 1.f);
    CHECK(lHandle.GetPath() == "Live.opaaxasset");

    // What the editor does on Save: write, then reload in place. The old object is gone, the handle
    // reads the new one.
    const TWeakObjectPtr<const TestTuningAsset> lOld = lHandle.Get();
    lAsset.Speed = 7.f;
    REQUIRE(AssetFile::Save(lPath, lAsset));
    REQUIRE(lManager.Reload<AssetResource>(lPath.CStr()));

    REQUIRE(lHandle.Get() != nullptr);
    CHECK(lHandle.Get()->Speed == 7.f);
    CHECK_FALSE(lOld.IsValid());

    // A handle of another class reads nothing, or the class's defaults when asked for them.
    const TAssetHandle<TestOtherAsset> lWrong(lManager.Load<AssetResource>(lPath.CStr()), OpaaxString("Live.opaaxasset"));
    CHECK(lWrong.IsLoaded());
    CHECK(lWrong.Get() == nullptr);
    CHECK(lWrong.GetOrDefault().Gravity == 9.f);

    // An empty handle too.
    CHECK(TAssetHandle<TestOtherAsset>().GetOrDefault().Gravity == 9.f);

    lManager.Shutdown();
}

// =============================================================================
// References
// =============================================================================

namespace
{
    /** Records the asset references it is shown; ignores everything else. */
    class AssetRefRecorder final : public IPropertyVisitor
    {
    public:
        TDynArray<OpaaxString> Lines;

        void Visit(const char*, bool&, const PropertyMeta&) override          {}
        void Visit(const char*, Int16&, const PropertyMeta&) override         {}
        void Visit(const char*, Int32&, const PropertyMeta&) override         {}
        void Visit(const char*, Uint32&, const PropertyMeta&) override        {}
        void Visit(const char*, float&, const PropertyMeta&) override         {}
        void Visit(const char*, Vector2F&, const PropertyMeta&) override      {}
        void Visit(const char*, Vector3F&, const PropertyMeta&) override      {}
        void Visit(const char*, Vector4F&, const PropertyMeta&) override      {}
        void Visit(const char*, LinearColor&, const PropertyMeta&) override   {}
        void Visit(const char*, OpaaxString&, const PropertyMeta&) override   {}
        void Visit(const char*, OpaaxStringID&, const PropertyMeta&) override {}
        void VisitEnum(const char*, const char* const*, Uint32, Uint32&, const PropertyMeta&) override {}
        void VisitResourcePath(const char*, OpaaxString&, Uint32, const PropertyMeta&) override {}

        void VisitObjectRef(const char*, ObjectHandle&, const ObjectClass&, const PropertyMeta&) override {}
        bool BeginGroup(const char*, const PropertyMeta&) override { return true; }
        void EndGroup() override {}
        void VisitUnsupported(const char*, OpaaxStringView) override {}

        void VisitAssetRef(const char* InName, OpaaxString& InPath, const ObjectClass& InClass,
                           const PropertyMeta&) override
        {
            Lines.push_back(OpaaxString::Format("{} {} {}", InName, InPath.CStr(), InClass.GetName().CStr()));
            InPath = OpaaxString("Data/Picked.opaaxasset");   // what a drop does
        }
    };
}

TEST_CASE("TAssetRef: saved as its path, visited with the class it accepts")
{
    TestAssetUser lUser;
    lUser.Tuning.Path = OpaaxString("Data/Grunt.opaaxasset");

    const nlohmann::json lJson = PropertiesToJson(lUser);
    CHECK(lJson == nlohmann::json{ { "Tuning", "Data/Grunt.opaaxasset" } });

    TestAssetUser lBack;
    REQUIRE(PropertiesFromJson(lJson, lBack));
    CHECK(lBack.Tuning == lUser.Tuning);

    AssetRefRecorder lRecorder;
    VisitProperties(lUser, lRecorder);

    REQUIRE(lRecorder.Lines.size() == 1);
    CHECK(lRecorder.Lines[0] == "Tuning Data/Grunt.opaaxasset TestTuningAsset");
    CHECK(lUser.Tuning.Path == "Data/Picked.opaaxasset");
}
