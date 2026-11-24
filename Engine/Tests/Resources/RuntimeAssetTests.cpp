// Suite: assets made in code and put in the resource manager under a runtime path ("/Runtime/...") —
// found by that path like a loaded file, never read from disk, released once nothing holds them.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/OpaaxTypes.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetHandle.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceManager.h"

using namespace Opaax;

namespace RuntimeAssetTestTypes
{
    class RuntimeNote final : public Asset
    {
        OPAAX_OBJECT(RuntimeNote, Asset)

    public:
        Int32 Initializations = 0;

        //~Begin Asset interface
        void   InitializeOnMainThread() override { ++Initializations; }
        Uint64 GetResourceSize() const override { return 500; }
        //~End Asset interface
    };
}

using namespace RuntimeAssetTestTypes;

namespace
{
    struct Fixture
    {
        ObjectClassRegistry Classes;
        ResourceManager     Resources;

        Fixture()
        {
            Classes.Register<Asset>();
            Classes.Register<RuntimeNote>();
            Resources.SetObjectClasses(&Classes);
            REQUIRE(Resources.Startup());
        }

        ~Fixture() { Resources.Shutdown(); }

        ResourceRef<AssetResource> AddNote(const char* InPath)
        {
            AssetResource lResource;
            lResource.ClassName = RuntimeNote::StaticClass().GetName();
            lResource.Instance  = NewObject<RuntimeNote>();
            return Resources.Add<AssetResource>(InPath, Move(lResource));
        }
    };
}

TEST_CASE("Runtime asset: added under its path, initialized once, found by that path")
{
    Fixture lFixture;

    ResourceRef<AssetResource> lRef = lFixture.AddNote("/Runtime/Note");
    REQUIRE(lRef.IsValid());

    const RuntimeNote* lNote = lRef.Get()->Get<RuntimeNote>();
    REQUIRE(lNote != nullptr);
    CHECK(lNote->Initializations == 1);
    CHECK(lFixture.Resources.GetLoadedCount<AssetResource>() == 1u);
    CHECK(lFixture.Resources.GetBytes<AssetResource>() >= 500u);

    const ResourceRef<AssetResource> lFound = lFixture.Resources.Find<AssetResource>("/Runtime/Note");
    REQUIRE(lFound.IsValid());
    CHECK(lFound.Get()->Get<RuntimeNote>() == lNote);
}

TEST_CASE("Runtime asset: a path in use is refused, the first one kept")
{
    Fixture lFixture;

    const ResourceRef<AssetResource> lFirst  = lFixture.AddNote("/Runtime/Note");
    const ResourceRef<AssetResource> lSecond = lFixture.AddNote("/Runtime/Note");

    CHECK(lFirst.IsValid());
    CHECK_FALSE(lSecond.IsValid());
    CHECK(lFixture.Resources.GetLoadedCount<AssetResource>() == 1u);
}

TEST_CASE("Runtime asset: released at the next update once nothing holds it; the path is free again")
{
    Fixture lFixture;

    ResourceRef<AssetResource> lRef = lFixture.AddNote("/Runtime/Note");
    lRef = ResourceRef<AssetResource>{};

    // Deferred like a loaded file: there until the next update.
    CHECK(lFixture.Resources.GetLoadedCount<AssetResource>() == 1u);
    lFixture.Resources.Update(0.0);
    CHECK(lFixture.Resources.GetLoadedCount<AssetResource>() == 0u);
    CHECK_FALSE(lFixture.Resources.Find<AssetResource>("/Runtime/Note").IsValid());

    CHECK(lFixture.AddNote("/Runtime/Note").IsValid());
}

TEST_CASE("Runtime asset: LoadAsset finds it and never reads a file; nothing made, nothing found")
{
    // The null paths resolve nothing: a runtime path does not need them.
    Fixture       lFixture;
    const IPaths& lPaths = IPaths::Null();

    // Not made yet: an empty handle, and no failed slot left behind.
    CHECK_FALSE(LoadAsset<RuntimeNote>(lFixture.Resources, lPaths, OpaaxString("/Runtime/Note")).IsLoaded());
    CHECK_FALSE(LoadAssetAsync<RuntimeNote>(lFixture.Resources, lPaths, OpaaxString("/Runtime/Note")).IsLoaded());
    CHECK(lFixture.Resources.GetLoadingCount<AssetResource>() == 0u);

    ResourceRef<AssetResource> lRef = lFixture.AddNote("/Runtime/Note");
    {
        const TAssetHandle<RuntimeNote> lHandle =
            LoadAsset<RuntimeNote>(lFixture.Resources, lPaths, OpaaxString("/Runtime/Note"));
        REQUIRE(lHandle.Get() != nullptr);
        CHECK(lHandle.GetPath() == OpaaxString("/Runtime/Note"));
    }

    // A handle is a holder like any other: the asset stays while the game's ref does.
    lFixture.Resources.Update(0.0);
    CHECK(lFixture.Resources.GetLoadedCount<AssetResource>() == 1u);
}
