// Suite: Resources system (M-RES-1). Exercises the full core against BinaryResource
// (leaf, Placeholder policy) and a test-local composite ManifestResource (FailFast,
// acquires sub-manifests through LoadContext): dedup, refcount + RAII release,
// generational stale-handle safety, composite load + release cascade, hard-cycle
// detection, Placeholder-vs-FailFast resolve, bytes accounting, Pin, and TypeID.
#include <doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "Core/OpaaxTypes.h"
#include "Core/Engine/Subsystems/Resources/ResourceManager.h"
#include "Core/Engine/Subsystems/Resources/Types/BinaryResource.hpp"

using namespace Opaax;

namespace
{
    // -------------------------------------------------------------------------
    // Per-test temp workspace: a unique dir under the OS temp path, removed on
    // destruction. Files are written + referenced by their native path string, so
    // the string passed to Load matches the string interned for dedup / cycles.
    // -------------------------------------------------------------------------
    struct TempWorkspace
    {
        std::filesystem::path Dir;

        TempWorkspace()
        {
            static std::atomic<Uint64> s_Counter{0};
            const Uint64 lTick = static_cast<Uint64>(std::chrono::steady_clock::now().time_since_epoch().count());
            Dir = std::filesystem::temp_directory_path() /
                  ("opaax_res_" + std::to_string(lTick) + "_" + std::to_string(s_Counter.fetch_add(1)));
            std::filesystem::create_directories(Dir);
        }

        ~TempWorkspace()
        {
            std::error_code lEc;
            std::filesystem::remove_all(Dir, lEc);
        }

        std::string PathOf(const std::string& InName) const { return (Dir / InName).string(); }

        std::string Write(const std::string& InName, const std::string& InContents) const
        {
            const std::filesystem::path lPath = Dir / InName;
            std::ofstream lOut(lPath, std::ios::binary);
            lOut.write(InContents.data(), static_cast<std::streamsize>(InContents.size()));
            return lPath.string();
        }
    };

    std::string Trim(const std::string& InStr)
    {
        const char* lWs = " \t\r\n";
        const size_t lBeg = InStr.find_first_not_of(lWs);
        if (lBeg == std::string::npos) { return {}; }
        const size_t lEnd = InStr.find_last_not_of(lWs);
        return InStr.substr(lBeg, lEnd - lBeg + 1);
    }

    // -------------------------------------------------------------------------
    // ManifestResource — test-local composite. A text file of child manifest paths;
    // each child is hard-acquired through LoadContext (refs chain -> release cascades).
    // FailFast: a missing child or a cycle fails the whole load.
    // -------------------------------------------------------------------------
    struct ManifestResource
    {
        TDynArray<ResourceRef<ManifestResource>> Children;

        static constexpr EFailPolicy FailPolicy = EFailPolicy::FailFast;

        static std::optional<ManifestResource> Load(const char* InPath, LoadContext& InCtx)
        {
            std::ifstream lFile(InPath, std::ios::binary);
            if (!lFile.is_open()) { return std::nullopt; }

            ManifestResource lManifest;
            std::string      lLine;
            while (std::getline(lFile, lLine))
            {
                const std::string lChildPath = Trim(lLine);
                if (lChildPath.empty()) { continue; }

                ResourceRef<ManifestResource> lChild = InCtx.Acquire<ManifestResource>(lChildPath.c_str());
                if (!lChild) { return std::nullopt; } // FailFast: cycle / missing child fails us
                lManifest.Children.push_back(Move(lChild));
            }
            return lManifest;
        }

        static ManifestResource Placeholder() { return ManifestResource{}; }
    };
}

// =============================================================================
TEST_CASE("Resources: same path dedups to one slot; refcount tracks claims")
{
    TempWorkspace   lWs;
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    const std::string lFile = lWs.Write("a.bin", "hello"); // 5 bytes

    ResourceRef<BinaryResource> lRefA = lMgr.Load<BinaryResource>(lFile.c_str());
    ResourceRef<BinaryResource> lRefB = lMgr.Load<BinaryResource>(lFile.c_str());

    CHECK(lRefA.IsValid());
    CHECK(lRefA.GetHandle() == lRefB.GetHandle());       // one copy per unique path
    CHECK(lMgr.GetLoadedCount<BinaryResource>() == 1);
    CHECK(lMgr.Resolve(lRefA.GetHandle()) != nullptr);
    CHECK(lMgr.Resolve(lRefA.GetHandle())->Bytes.size() == 5);
}

// =============================================================================
TEST_CASE("Resources: releasing the last ref unloads; stale handle resolves safe")
{
    TempWorkspace   lWs;
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    const std::string              lFile = lWs.Write("a.bin", "hello");
    ResourceHandle<BinaryResource> lStale;

    {
        ResourceRef<BinaryResource> lRef = lMgr.Load<BinaryResource>(lFile.c_str());
        lStale = lRef.GetHandle();
        CHECK(lMgr.GetLoadedCount<BinaryResource>() == 1);
    } // last ref dropped -> unload

    CHECK(lMgr.GetLoadedCount<BinaryResource>() == 0);

    // Placeholder policy: a stale handle never dangles — it resolves to the (empty)
    // placeholder, not a crash and not the old bytes.
    BinaryResource* lResolved = lMgr.Resolve(lStale);
    CHECK(lResolved != nullptr);
    CHECK(lResolved->Bytes.empty());

    // Reload reuses the slot with a bumped generation, so the stale handle stays stale.
    ResourceRef<BinaryResource> lFresh = lMgr.Load<BinaryResource>(lFile.c_str());
    CHECK(lFresh.GetHandle().Slot == lStale.Slot);
    CHECK(lFresh.GetHandle().Generation != lStale.Generation);
    CHECK(lMgr.Resolve(lStale)->Bytes.empty());          // still placeholder for the old gen
    CHECK(lMgr.Resolve(lFresh.GetHandle())->Bytes.size() == 5);
}

// =============================================================================
TEST_CASE("Resources: a composite loads its dependencies; release cascades")
{
    TempWorkspace   lWs;
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    const std::string lChild1 = lWs.Write("c1.man", "");   // leaf manifests
    const std::string lChild2 = lWs.Write("c2.man", "");
    const std::string lParent = lWs.Write("p.man", lChild1 + "\n" + lChild2 + "\n");

    {
        ResourceRef<ManifestResource> lRoot = lMgr.Load<ManifestResource>(lParent.c_str());
        CHECK(lRoot.IsValid());
        CHECK(lMgr.GetLoadedCount<ManifestResource>() == 3); // parent + 2 children
    } // dropping the root releases its hard child refs -> all unload

    CHECK(lMgr.GetLoadedCount<ManifestResource>() == 0);
}

// =============================================================================
TEST_CASE("Resources: a hard dependency cycle fails loudly, does not hang")
{
    TempWorkspace   lWs;
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    SUBCASE("self cycle")
    {
        const std::string lSelf = lWs.PathOf("self.man");
        lWs.Write("self.man", lSelf + "\n");             // lists itself

        ResourceRef<ManifestResource> lRoot = lMgr.Load<ManifestResource>(lSelf.c_str());
        CHECK_FALSE(lRoot.IsValid());
        CHECK(lMgr.GetLoadedCount<ManifestResource>() == 0);
    }

    SUBCASE("two-node cycle A -> B -> A")
    {
        const std::string lA = lWs.PathOf("a.man");
        const std::string lB = lWs.PathOf("b.man");
        lWs.Write("a.man", lB + "\n");
        lWs.Write("b.man", lA + "\n");

        ResourceRef<ManifestResource> lRoot = lMgr.Load<ManifestResource>(lA.c_str());
        CHECK_FALSE(lRoot.IsValid());
        CHECK(lMgr.GetLoadedCount<ManifestResource>() == 0);
    }
}

// =============================================================================
TEST_CASE("Resources: Placeholder resolves to a substitute, FailFast resolves to null")
{
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    // Invalid handle, Placeholder policy -> non-null empty placeholder.
    BinaryResource* lPlaceholder = lMgr.Resolve(ResourceHandle<BinaryResource>{});
    CHECK(lPlaceholder != nullptr);

    // Invalid handle, FailFast policy -> null (a placeholder there would lie).
    ManifestResource* lNull = lMgr.Resolve(ResourceHandle<ManifestResource>{});
    CHECK(lNull == nullptr);
}

// =============================================================================
TEST_CASE("Resources: a failed load applies the fail policy (Placeholder vs FailFast)")
{
    TempWorkspace   lWs;
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    SUBCASE("Placeholder leaf: missing file -> invalid handle but Get yields placeholder")
    {
        ResourceRef<BinaryResource> lRef = lMgr.Load<BinaryResource>(lWs.PathOf("missing.bin").c_str());
        CHECK_FALSE(lRef.IsValid());                     // the load failed
        CHECK(lRef.Get() != nullptr);                    // ...but a texture-like type still renders
        CHECK(lRef.Get()->Bytes.empty());
        CHECK(lMgr.GetLoadedCount<BinaryResource>() == 0);
    }

    SUBCASE("FailFast composite: a missing child fails the parent")
    {
        const std::string lParent = lWs.Write("p.man", lWs.PathOf("missing.man") + "\n");
        ResourceRef<ManifestResource> lRoot = lMgr.Load<ManifestResource>(lParent.c_str());
        CHECK_FALSE(lRoot.IsValid());
        CHECK(lMgr.GetLoadedCount<ManifestResource>() == 0);
    }
}

// =============================================================================
TEST_CASE("Resources: bytes accounting reflects loaded payloads")
{
    TempWorkspace   lWs;
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    const std::string lFile = lWs.Write("big.bin", std::string(1234, 'x'));

    {
        ResourceRef<BinaryResource> lRef = lMgr.Load<BinaryResource>(lFile.c_str());
        CHECK(lMgr.GetBytes<BinaryResource>() == 1234);
    }
    CHECK(lMgr.GetBytes<BinaryResource>() == 0);
}

// =============================================================================
TEST_CASE("Resources: Pin bridges a data handle to a lifetime claim")
{
    TempWorkspace   lWs;
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    const std::string lFile = lWs.Write("pin.bin", "data");

    ResourceRef<BinaryResource>    lRef    = lMgr.Load<BinaryResource>(lFile.c_str());
    ResourceHandle<BinaryResource> lHandle = lRef.GetHandle();

    {
        ResourceRef<BinaryResource> lPinned = lMgr.Pin(lHandle);
        CHECK(lPinned.IsValid());
        CHECK(lMgr.GetLoadedCount<BinaryResource>() == 1); // shared, still one slot
    }
    CHECK(lMgr.Resolve(lHandle) != nullptr);               // lRef still holds it

    // Pin cannot resurrect a dead handle.
    ResourceHandle<BinaryResource> lDead;
    {
        ResourceRef<BinaryResource> lTmp = lMgr.Load<BinaryResource>(lWs.Write("t.bin", "z").c_str());
        lDead = lTmp.GetHandle();
    }
    ResourceRef<BinaryResource> lPinnedDead = lMgr.Pin(lDead);
    CHECK_FALSE(lPinnedDead.IsValid());
}

// =============================================================================
TEST_CASE("Resources: ResourceTypeID is unique per type and stable within a run")
{
    CHECK(ResourceTypeID::Get<BinaryResource>()   == ResourceTypeID::Get<BinaryResource>());
    CHECK(ResourceTypeID::Get<ManifestResource>() == ResourceTypeID::Get<ManifestResource>());
    CHECK(ResourceTypeID::Get<BinaryResource>()   != ResourceTypeID::Get<ManifestResource>());
}
