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
#include <thread>

#include "Core/OpaaxTypes.h"
#include "Application/Services/IJobSystem.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/ResourceView.hpp"
#include "Engine/Subsystems/Resources/Types/BinaryResource.hpp"

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

    // -------------------------------------------------------------------------
    // GpuLikeResource — a leaf that records the thread its Load ran on and whose
    // optional Initialize() (main-thread GPU "log-in") bumps a counter: lets the async
    // tests prove Load runs on a worker and Initialize runs once, on the pump.
    // -------------------------------------------------------------------------
    struct GpuLikeResource
    {
        std::thread::id LoadThread{};
        int             InitCount = 0;

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<GpuLikeResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            std::ifstream lFile(InPath, std::ios::binary);
            if (!lFile.is_open()) { return std::nullopt; }
            GpuLikeResource lRes;
            lRes.LoadThread = std::this_thread::get_id();
            return lRes;
        }

        void Initialize() { ++InitCount; } // main-thread log-in (detected via if-constexpr)
        static GpuLikeResource Placeholder() { return GpuLikeResource{}; }
    };

    // Pump the drain + GC until no resource of T is in flight (bounded so a stuck load
    // can't hang the suite). Stands in for the engine loop's per-frame DrainCompletions.
    template<CResource T>
    void PumpUntilIdle(ResourceManager& InMgr, JobSystem& InJobs)
    {
        for (int i = 0; i < 100000 && InMgr.template GetLoadingCount<T>() != 0; ++i)
        {
            InJobs.DrainCompletions();
            InMgr.Update(0.0);
            std::this_thread::yield();
        }
    }
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
    } // last ref dropped -> deferred unload (graveyard)

    // One-frame grace: still loaded + resolvable to real bytes until the next pump.
    CHECK(lMgr.GetLoadedCount<BinaryResource>() == 1);
    CHECK(lMgr.Resolve(lStale) != nullptr);
    CHECK(lMgr.Resolve(lStale)->Bytes.size() == 5);

    lMgr.Update(0.0); // pump -> collect the graveyard
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
TEST_CASE("Resources: a dedup Load during the grace window resurrects the slot")
{
    TempWorkspace   lWs;
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    const std::string              lFile = lWs.Write("g.bin", "grace");
    ResourceHandle<BinaryResource> lHandle;

    {
        ResourceRef<BinaryResource> lRef = lMgr.Load<BinaryResource>(lFile.c_str());
        lHandle = lRef.GetHandle();
    } // released -> graveyard, still Loaded until the next pump

    // Load before the pump dedups onto the still-live slot: same slot AND generation
    // (no reload, no generation bump) — the pending grave entry will simply no-op.
    ResourceRef<BinaryResource> lRevived = lMgr.Load<BinaryResource>(lFile.c_str());
    CHECK(lRevived.GetHandle() == lHandle);
    CHECK(lMgr.GetLoadedCount<BinaryResource>() == 1);

    lMgr.Update(0.0);                                    // grave entry sees refcount>0 -> skipped
    CHECK(lMgr.GetLoadedCount<BinaryResource>() == 1);   // survived — still referenced by lRevived
    CHECK(lMgr.Resolve(lHandle) != nullptr);
    CHECK(lMgr.Resolve(lHandle)->Bytes.size() == 5);
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
    } // dropping the root enqueues it for deferred unload

    // Cascade unwinds one level per pump: the root's grave entry destroys the parent,
    // whose child refs then release into the NEXT pump's graveyard.
    CHECK(lMgr.GetLoadedCount<ManifestResource>() == 3); // grace (pre-pump)
    lMgr.Update(0.0);
    CHECK(lMgr.GetLoadedCount<ManifestResource>() == 2); // parent gone; children released, one-frame grace
    lMgr.Update(0.0);
    CHECK(lMgr.GetLoadedCount<ManifestResource>() == 0); // children collected
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
    CHECK(lMgr.GetBytes<BinaryResource>() == 1234); // grace: accounted until the pump
    lMgr.Update(0.0);
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
    lMgr.Update(0.0); // pump past the grace so the slot is actually destroyed
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

// =============================================================================
TEST_CASE("Resources: LoadAsync loads off the main thread and publishes at the pump")
{
    TempWorkspace   lWs;
    JobSystem       lJobs(1);   // >=1 real worker so onComplete defers to a drain (not inline)
    ResourceManager lMgr;
    lMgr.SetJobSystem(lJobs);
    REQUIRE(lMgr.Startup());

    const std::string lFile = lWs.Write("g.bin", "x");

    ResourceRef<GpuLikeResource> lRef = lMgr.LoadAsync<GpuLikeResource>(lFile.c_str());
    CHECK(lRef.IsValid());                                       // a Loading claim, returned immediately
    CHECK(lMgr.GetLoadingCount<GpuLikeResource>() == 1);
    CHECK(lMgr.GetLoadedCount<GpuLikeResource>() == 0);
    GpuLikeResource* lWhileLoading = lMgr.Resolve(lRef.GetHandle());
    REQUIRE(lWhileLoading != nullptr);                           // resolves to the placeholder while Loading

    PumpUntilIdle<GpuLikeResource>(lMgr, lJobs);

    CHECK(lMgr.GetLoadingCount<GpuLikeResource>() == 0);
    CHECK(lMgr.GetLoadedCount<GpuLikeResource>() == 1);

    GpuLikeResource* lLoaded = lMgr.Resolve(lRef.GetHandle());
    REQUIRE(lLoaded != nullptr);
    // What the mid-load Resolve handed back was the PLACEHOLDER, a different object — asserted on
    // identity rather than on a field, because both objects carry the same fields ([[L25]]). The
    // placeholder is initialised too (see the case below), so counting InitCount here could not
    // tell the two apart.
    CHECK(lLoaded != lWhileLoading);
    CHECK(lLoaded->InitCount == 1);                              // Initialize ran exactly once...
    CHECK(lLoaded->LoadThread != std::this_thread::get_id());    // ...and Load ran on a worker thread
}

// =============================================================================
TEST_CASE("Resources: the placeholder is INITIALISED, like any other payload")
{
    // A substitute has to be as USABLE as the thing it substitutes for. For a GPU-backed type that
    // means uploaded: an un-initialised magenta texture has no GPU handle and draws as nothing,
    // which is the silent-wrong-answer failure the Placeholder policy exists to prevent.
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    GpuLikeResource* lPlaceholder = lMgr.Resolve(ResourceHandle<GpuLikeResource>{});
    REQUIRE(lPlaceholder != nullptr);
    CHECK(lPlaceholder->InitCount == 1);

    // Built ONCE per pool, not per resolve.
    CHECK(lMgr.Resolve(ResourceHandle<GpuLikeResource>{}) == lPlaceholder);
    CHECK(lPlaceholder->InitCount == 1);
}

// =============================================================================
TEST_CASE("Resources: an async Ref dropped while still Loading is abandoned, not leaked")
{
    TempWorkspace   lWs;
    JobSystem       lJobs(1);
    ResourceManager lMgr;
    lMgr.SetJobSystem(lJobs);
    REQUIRE(lMgr.Startup());

    const std::string lFile = lWs.Write("d.bin", "drop");

    {
        ResourceRef<BinaryResource> lRef = lMgr.LoadAsync<BinaryResource>(lFile.c_str());
        CHECK(lMgr.GetLoadingCount<BinaryResource>() == 1);
    } // ref dropped WHILE the slot is still Loading -> refcount must decrement to 0

    PumpUntilIdle<BinaryResource>(lMgr, lJobs); // worker fills; publish sees refcount 0 -> abandons

    CHECK(lMgr.GetLoadingCount<BinaryResource>() == 0);
    CHECK(lMgr.GetLoadedCount<BinaryResource>()  == 0); // abandoned, NOT leaked at flush
    CHECK(lMgr.GetBytes<BinaryResource>()        == 0);
}

// =============================================================================
TEST_CASE("Resources: LoadAsync completion callback fires Loaded and can retain the resource")
{
    TempWorkspace   lWs;
    JobSystem       lJobs(1);
    ResourceManager lMgr;
    lMgr.SetJobSystem(lJobs);
    REQUIRE(lMgr.Startup());

    const std::string lFile = lWs.Write("cb.bin", "hello"); // 5 bytes

    int                         lFired  = 0;
    bool                        lFailed = true;
    ResourceRef<BinaryResource> lKept;

    // Fire-and-forget: we do NOT keep the returned Ref — the internal claim keeps it alive.
    lMgr.LoadAsync<BinaryResource>(lFile.c_str(),
        [&](LoadAsyncResult<BinaryResource> InResult)
        {
            ++lFired;
            lFailed = InResult.bFailed;
            lKept   = InResult.Ref; // retain it
        });

    CHECK(lFired == 0); // callbacks fire on a pump, never inside LoadAsync

    for (int i = 0; i < 100000 && lFired == 0; ++i)
    {
        lJobs.DrainCompletions();
        lMgr.Update(0.0);
        std::this_thread::yield();
    }

    CHECK(lFired == 1);
    CHECK_FALSE(lFailed);
    CHECK(lKept.IsValid());
    CHECK(lMgr.GetLoadedCount<BinaryResource>() == 1);          // retained -> stays loaded
    CHECK(lMgr.Resolve(lKept.GetHandle())->Bytes.size() == 5);
}

// =============================================================================
TEST_CASE("Resources: LoadAsync completion callback reports failure for a missing file")
{
    TempWorkspace   lWs;
    JobSystem       lJobs(1);
    ResourceManager lMgr;
    lMgr.SetJobSystem(lJobs);
    REQUIRE(lMgr.Startup());

    int  lFired  = 0;
    bool lFailed = false;
    bool lRefValid = true;

    lMgr.LoadAsync<BinaryResource>(lWs.PathOf("nope.bin").c_str(),
        [&](LoadAsyncResult<BinaryResource> InResult)
        {
            ++lFired;
            lFailed   = InResult.bFailed;
            lRefValid = InResult.Ref.IsValid();
        });

    for (int i = 0; i < 100000 && lFired == 0; ++i)
    {
        lJobs.DrainCompletions();
        lMgr.Update(0.0);
        std::this_thread::yield();
    }

    CHECK(lFired == 1);
    CHECK(lFailed);
    CHECK_FALSE(lRefValid);                                     // no Ref delivered on failure
    CHECK(lMgr.GetLoadingCount<BinaryResource>() == 0);
    CHECK(lMgr.GetLoadedCount<BinaryResource>()  == 0);
}

// =============================================================================
TEST_CASE("Resources: a callback that drops the result lets the resource unload")
{
    TempWorkspace   lWs;
    JobSystem       lJobs(1);
    ResourceManager lMgr;
    lMgr.SetJobSystem(lJobs);
    REQUIRE(lMgr.Startup());

    const std::string lFile = lWs.Write("ff.bin", "bye");

    int lFired = 0;
    lMgr.LoadAsync<BinaryResource>(lFile.c_str(),
        [&](LoadAsyncResult<BinaryResource> InResult) { ++lFired; /* drop InResult.Ref */ });

    for (int i = 0; i < 100000 && lFired == 0; ++i)
    {
        lJobs.DrainCompletions();
        lMgr.Update(0.0);
        std::this_thread::yield();
    }

    CHECK(lFired == 1);
    // Internal claim released when the callback fired -> unloads on the next pump.
    lMgr.Update(0.0);
    CHECK(lMgr.GetLoadedCount<BinaryResource>() == 0);
}

// =============================================================================
TEST_CASE("Resources: LoadAsync loads a composite subtree on the worker")
{
    TempWorkspace   lWs;
    JobSystem       lJobs(1);
    ResourceManager lMgr;
    lMgr.SetJobSystem(lJobs);
    REQUIRE(lMgr.Startup());

    const std::string lChild1 = lWs.Write("c1.man", "");
    const std::string lChild2 = lWs.Write("c2.man", "");
    const std::string lParent = lWs.Write("p.man", lChild1 + "\n" + lChild2 + "\n");

    ResourceRef<ManifestResource> lRoot = lMgr.LoadAsync<ManifestResource>(lParent.c_str());
    CHECK(lRoot.IsValid());

    PumpUntilIdle<ManifestResource>(lMgr, lJobs);

    CHECK(lMgr.GetLoadingCount<ManifestResource>() == 0);
    CHECK(lMgr.GetLoadedCount<ManifestResource>() == 3);         // parent + 2 children, all off-thread
    REQUIRE(lRoot.Get() != nullptr);
    CHECK(lRoot.Get()->Children.size() == 2);

    // Deferred unload cascades for async-loaded composites too.
    lRoot = ResourceRef<ManifestResource>{};
    lMgr.Update(0.0);                                            // parent unloads -> releases children
    lMgr.Update(0.0);                                            // children unload
    CHECK(lMgr.GetLoadedCount<ManifestResource>() == 0);
}

// =============================================================================
TEST_CASE("Resources: CheckedView flags a Resolve pointer held across a pump")
{
    TempWorkspace   lWs;
    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    const std::string lFile = lWs.Write("v.bin", "view");
    ResourceRef<BinaryResource> lRef = lMgr.Load<BinaryResource>(lFile.c_str());

    CheckedView<BinaryResource> lView = CheckedResolve(lMgr, lRef.GetHandle());
    CHECK_FALSE(lView.IsStale());
    CHECK(lView.Get() != nullptr);         // safe within the frame it was taken
    CHECK(lView->Bytes.size() == 4);

    lMgr.Update(0.0);                      // pump -> epoch advances
    CHECK(lView.IsStale());                // a cached view is now flagged (Get() would assert in debug)
}

// =============================================================================
// Reload — the same slot, new bytes. Added the day the sprite sheet editor shipped: the editor
// saves a file that a sprite is ALREADY holding, and without this the renderer kept drawing what
// was loaded the first time. So the case that matters is not "the payload changed", it is "a ref
// TAKEN BEFORE the reload sees the change".
// =============================================================================
TEST_CASE("ResourceManager::Reload: a ref taken BEFORE the reload sees the new payload")
{
    TempWorkspace lWs;
    const std::string lPath = lWs.Write("hot.bin", "before");

    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    ResourceRef<BinaryResource> lHeld = lMgr.Load<BinaryResource>(lPath.c_str());
    REQUIRE(lHeld.IsValid());
    REQUIRE(Trim(std::string(reinterpret_cast<const char*>(lHeld.Get()->Bytes.data()),
                             lHeld.Get()->Bytes.size())) == "before");

    lWs.Write("hot.bin", "after!");

    CHECK(lMgr.Reload<BinaryResource>(lPath.c_str()));

    // The SAME claim, never re-taken — this is the assertion the bug was about.
    CHECK(lHeld.IsValid());
    CHECK(Trim(std::string(reinterpret_cast<const char*>(lHeld.Get()->Bytes.data()),
                           lHeld.Get()->Bytes.size())) == "after!");

    lMgr.Shutdown();
}

TEST_CASE("ResourceManager::Reload: a path nobody holds is FALSE, and is not a failure")
{
    TempWorkspace lWs;
    const std::string lPath = lWs.Write("cold.bin", "x");

    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    // Never loaded, so nothing to update — and deliberately NOT a load, or saving a file would
    // pull it into memory for nobody.
    CHECK_FALSE(lMgr.Reload<BinaryResource>(lPath.c_str()));
    CHECK(lMgr.GetLoadedCount<BinaryResource>() == 0u);

    lMgr.Shutdown();
}

TEST_CASE("ResourceManager::Reload: a failed re-read KEEPS the resident payload")
{
    TempWorkspace lWs;
    const std::string lPath = lWs.Write("vanish.bin", "original");

    ResourceManager lMgr;
    REQUIRE(lMgr.Startup());

    ResourceRef<BinaryResource> lHeld = lMgr.Load<BinaryResource>(lPath.c_str());
    REQUIRE(lHeld.IsValid());

    std::error_code lEc;
    std::filesystem::remove(lWs.Dir / "vanish.bin", lEc);

    // Blanking a live resource because a re-read failed is worse than keeping something stale.
    CHECK_FALSE(lMgr.Reload<BinaryResource>(lPath.c_str()));
    CHECK(lHeld.IsValid());
    CHECK(Trim(std::string(reinterpret_cast<const char*>(lHeld.Get()->Bytes.data()),
                           lHeld.Get()->Bytes.size())) == "original");

    lMgr.Shutdown();
}
