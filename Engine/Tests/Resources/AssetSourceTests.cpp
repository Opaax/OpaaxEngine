// Suite: assets read from files of their own (OPAAX_ASSET_SOURCE) — the class lists its extensions,
// the registry finds it by extension, and the resource manager loads, initializes, sizes and reloads
// such a file as an asset of that class.
#include <doctest.h>

#include "../TestFolderPaths.h"
#include "../TestTempDir.h"
#include "Core/IO/FileIO.h"
#include "Core/OpaaxTypes.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetHandle.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceFormat.h"
#include "Resources/ResourceManager.h"

using namespace Opaax;

namespace AssetSourceTestTypes
{
    // A note read from a text file of its own.
    class SourceNote final : public Asset
    {
        OPAAX_OBJECT(SourceNote, Asset)
        OPAAX_ASSET_SOURCE(".sourcenote", "NOTE")

    public:
        OpaaxString Text;
        Int32       Initializations = 0;

        //~Begin Asset interface
        bool LoadSource(const char* InAbsPath) override
        {
            Text = FileIO::ReadAllText(OpaaxString(InAbsPath));
            return !Text.IsEmpty();
        }

        void   InitializeOnMainThread() override { ++Initializations; }
        Uint64 GetResourceSize() const override { return 1000 + Text.GetLength(); }
        //~End Asset interface
    };

    // Claims an extension another class reads already.
    class RivalNote final : public Asset
    {
        OPAAX_OBJECT(RivalNote, Asset)
        OPAAX_ASSET_SOURCE(".note")
    };

    // An asset class of .opaaxasset files.
    class PlainTuning final : public Asset
    {
        OPAAX_OBJECT(PlainTuning, Asset)

    public:
        Int32 Lives = 3;
        OPAAX_PROPERTIES(PlainTuning, OPAAX_PROP(Lives))
    };
}

using namespace AssetSourceTestTypes;

namespace
{
    /** The classes a game would register, and a resource manager reading with them. */
    struct Fixture
    {
        TestTempDir         Dir{ "AssetSource" };
        ObjectClassRegistry Classes;
        ResourceManager     Resources;

        Fixture()
        {
            Classes.Register<Asset>();
            Classes.Register<SourceNote>();
            Classes.Register<PlainTuning>();

            Resources.SetObjectClasses(&Classes);
            REQUIRE(Resources.Startup());
        }

        ~Fixture() { Resources.Shutdown(); }

        OpaaxString Write(const OpaaxStringView InName, const char* InText) const
        {
            const OpaaxString lPath = Dir.Sub(InName);
            FileIO::WriteAllText(lPath, OpaaxString(InText));
            return lPath;
        }
    };

    const SourceNote* NoteOf(const ResourceRef<AssetResource>& InRef)
    {
        return InRef.Get() != nullptr ? InRef.Get()->Get<SourceNote>() : nullptr;
    }
}

// =============================================================================
// The class
// =============================================================================

TEST_CASE("Asset source: a class lists the extensions of the files it is read from; other classes list none")
{
    const ObjectClass& lNote = SourceNote::StaticClass();
    REQUIRE(lNote.IsReadFromSourceFiles());

    const TDynArray<const char*> lExtensions = lNote.GetSourceExtensions();
    REQUIRE(lExtensions.size() == 2u);
    CHECK(OpaaxString(lExtensions[0]) == ".sourcenote");
    CHECK(OpaaxString(lExtensions[1]) == "NOTE");

    CHECK_FALSE(PlainTuning::StaticClass().IsReadFromSourceFiles());
    CHECK(PlainTuning::StaticClass().GetSourceExtensions().empty());
    CHECK_FALSE(Asset::StaticClass().IsReadFromSourceFiles());
}

TEST_CASE("Asset source: the registry finds the class by extension, whatever its case or dot")
{
    ObjectClassRegistry lClasses;
    REQUIRE(lClasses.Register<SourceNote>());
    REQUIRE(lClasses.Register<PlainTuning>());

    CHECK(lClasses.FindBySourceExtension(NormalizeExtension(".sourcenote")) == &SourceNote::StaticClass());
    CHECK(lClasses.FindBySourceExtension(NormalizeExtension(".SourceNote")) == &SourceNote::StaticClass());
    CHECK(lClasses.FindBySourceExtension(NormalizeExtension("note")) == &SourceNote::StaticClass());

    CHECK(lClasses.FindBySourceExtension(NormalizeExtension(".png")) == nullptr);
    CHECK(lClasses.FindBySourceExtension(OpaaxStringID()) == nullptr);
}

TEST_CASE("Asset source: a second class for an extension already read is refused, whole")
{
    ObjectClassRegistry lClasses;
    REQUIRE(lClasses.Register<SourceNote>());

    CHECK_FALSE(lClasses.Register<RivalNote>());
    CHECK(lClasses.FindByName(OPAAX_ID("RivalNote")) == nullptr);
    CHECK(lClasses.FindBySourceExtension(NormalizeExtension(".note")) == &SourceNote::StaticClass());
}

// =============================================================================
// Loading
// =============================================================================

TEST_CASE("Asset source: a file of the class's extension loads as an asset of it, initialized once")
{
    Fixture lFixture;
    const OpaaxString lPath = lFixture.Write("Hello.sourcenote", "hello");

    const ResourceRef<AssetResource> lRef = lFixture.Resources.Load<AssetResource>(lPath.CStr());
    const SourceNote* lNote = NoteOf(lRef);
    REQUIRE(lNote != nullptr);

    CHECK(lNote->Text == "hello");
    CHECK(lNote->GetName() == OPAAX_ID("Hello"));
    CHECK(lNote->Initializations == 1);
    CHECK(lRef.Get()->ClassName == OPAAX_ID("SourceNote"));

    // Its size is what the class says it holds.
    CHECK(lFixture.Resources.GetBytes<AssetResource>() >= 1005u);

    // Loaded once, shared; still initialized once.
    const ResourceRef<AssetResource> lSecond = lFixture.Resources.Load<AssetResource>(lPath.CStr());
    CHECK(NoteOf(lSecond) == lNote);
    CHECK(lNote->Initializations == 1);
}

TEST_CASE("Asset source: LoadAsset reads it through a handle, by its asset-relative path")
{
    Fixture lFixture;
    lFixture.Write("Notes/Shared.note", "shared");

    const TestFolderPaths lPaths(lFixture.Dir.Path());

    const TAssetHandle<SourceNote> lHandle = LoadAsset<SourceNote>(lFixture.Resources, lPaths, OpaaxString("Notes/Shared.note"));
    REQUIRE(lHandle.Get() != nullptr);
    CHECK(lHandle.Get()->Text == "shared");

    // A handle of another class reads nothing.
    const TAssetHandle<PlainTuning> lWrong = LoadAsset<PlainTuning>(lFixture.Resources, lPaths, OpaaxString("Notes/Shared.note"));
    CHECK(lWrong.IsLoaded());
    CHECK(lWrong.Get() == nullptr);
}

TEST_CASE("Asset source: an async load is initialized on the main thread, once published")
{
    Fixture lFixture;
    const OpaaxString lPath = lFixture.Write("Late.sourcenote", "late");

    const ResourceRef<AssetResource> lRef = lFixture.Resources.LoadAsync<AssetResource>(lPath.CStr());
    lFixture.Resources.Update(0.0);

    const SourceNote* lNote = NoteOf(lRef);
    REQUIRE(lNote != nullptr);
    CHECK(lNote->Text == "late");
    CHECK(lNote->Initializations == 1);
}

TEST_CASE("Asset source: a file the class cannot read, or of an extension no class reads, leaves no asset")
{
    Fixture lFixture;
    const OpaaxString lEmpty   = lFixture.Write("Empty.sourcenote", "");
    const OpaaxString lUnknown = lFixture.Write("Data.unknownkind", "data");

    CHECK(NoteOf(lFixture.Resources.Load<AssetResource>(lEmpty.CStr())) == nullptr);

    const ResourceRef<AssetResource> lRef = lFixture.Resources.Load<AssetResource>(lUnknown.CStr());
    REQUIRE(lRef.Get() != nullptr);
    CHECK_FALSE(lRef.Get()->Instance.IsValid());
}

TEST_CASE("Asset source: an .opaaxasset still names its class, whatever classes read other files")
{
    Fixture lFixture;
    const OpaaxString lPath = lFixture.Write("Tuning.opaaxasset", R"({ "Class": "PlainTuning", "Properties": { "Lives": 9 } })");

    const ResourceRef<AssetResource> lRef = lFixture.Resources.Load<AssetResource>(lPath.CStr());
    REQUIRE(lRef.Get() != nullptr);

    const PlainTuning* lTuning = lRef.Get()->Get<PlainTuning>();
    REQUIRE(lTuning != nullptr);
    CHECK(lTuning->Lives == 9);
}

TEST_CASE("Asset source: a reload reads the file again, and the new asset is initialized")
{
    Fixture lFixture;
    const OpaaxString lPath = lFixture.Write("Live.sourcenote", "first");

    const ResourceRef<AssetResource> lRef = lFixture.Resources.Load<AssetResource>(lPath.CStr());
    REQUIRE(NoteOf(lRef) != nullptr);

    FileIO::WriteAllText(lPath, OpaaxString("second"));
    REQUIRE(lFixture.Resources.Reload<AssetResource>(lPath.CStr()));

    const SourceNote* lNote = NoteOf(lRef);
    REQUIRE(lNote != nullptr);
    CHECK(lNote->Text == "second");
    CHECK(lNote->Initializations == 1);
}
