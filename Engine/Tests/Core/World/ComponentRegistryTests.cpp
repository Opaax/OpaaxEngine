// Suite: ComponentRegistry — registration, refusals, sealing, and the entry's Has/Add/Save/Load.
// Every case builds its own registry or WorldManager (no shared state).
#include <doctest.h>

#include <entt/entt.hpp>

#include "World/Components/ComponentRegistry.h"
#include "World/Components/QuadComponent.h"
#include "World/Entity/Entity.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Engine.h"

using namespace Opaax;

namespace
{
    // A component defined HERE, in the test exe — the same position a game module is in.
    // Satisfying CComponent is its whole contract: no base class, no engine registration.
    struct ProbeComponent
    {
        int         Health = 0;
        float       Speed  = 0.f;
        OpaaxString Tag;

        bool operator==(const ProbeComponent& InOther) const
        {
            return Health == InOther.Health && Speed == InOther.Speed && Tag == InOther.Tag;
        }
    };

    inline void to_json(nlohmann::json& InJson, const ProbeComponent& InValue)
    {
        InJson = nlohmann::json{
            {"Health", InValue.Health},
            {"Speed", InValue.Speed},
            {"Tag", std::string(InValue.Tag.CStr())}
        };
    }

    inline void from_json(const nlohmann::json& InJson, ProbeComponent& InValue)
    {
        InJson.at("Health").get_to(InValue.Health);
        InJson.at("Speed").get_to(InValue.Speed);
        InValue.Tag = OpaaxString(InJson.at("Tag").get<std::string>().c_str());
    }

    // Concept-satisfaction is a COMPILE-time gate, so it is asserted at compile time. A type
    // without the json pair simply won't instantiate Register<T> — no runtime case can show that.
    static_assert(CComponent<ProbeComponent>, "ProbeComponent must satisfy CComponent");
    static_assert(CComponent<QuadComponent>, "QuadComponent must satisfy CComponent");
    static_assert(!CComponent<EntityMeta>,
                  "EntityMeta is identity, not user data — the snapshot core writes it by hand "
                  "and it must NOT be registrable as an ordinary component.");
}

// =============================================================================
// Registration
// =============================================================================
TEST_CASE("ComponentRegistry: a registered type is findable by name and by type id")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    CHECK(lRegistry.Count() == 1u);

    const IComponentEntry* lByName = lRegistry.FindByName(OpaaxStringID("Probe"));
    REQUIRE(lByName != nullptr);

    const IComponentEntry* lById = lRegistry.Find<ProbeComponent>();
    REQUIRE(lById != nullptr);

    // Both lookups must land on the SAME entry — two entries for one type would mean the
    // round trip could pick either.
    CHECK(lByName == lById);
    CHECK(lByName->GetName() == OpaaxStringID("Probe"));
}

TEST_CASE("ComponentRegistry: an unregistered type resolves to nullptr, not to something else")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));

    CHECK(lRegistry.FindByName(OpaaxStringID("NeverRegistered")) == nullptr);
    CHECK(lRegistry.Find<QuadComponent>() == nullptr);
}

TEST_CASE("ComponentRegistry: ForEach visits every entry in registration order")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    REQUIRE(lRegistry.Register<QuadComponent>("Quad"));

    TDynArray<OpaaxStringID> lSeen;
    lRegistry.ForEach([&](const IComponentEntry& InEntry) { lSeen.push_back(InEntry.GetName()); });

    REQUIRE(lSeen.size() == 2u);
    CHECK(lSeen[0] == OpaaxStringID("Probe"));
    CHECK(lSeen[1] == OpaaxStringID("Quad"));
}

// =============================================================================
// Refusal rules — each one exists to stop a specific silent corruption
// =============================================================================
TEST_CASE("ComponentRegistry: a duplicate NAME is refused (it would make a map file ambiguous)")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Taken"));
    CHECK_FALSE(lRegistry.Register<QuadComponent>("Taken"));

    // The first registration must survive intact — a refusal is not a replacement.
    CHECK(lRegistry.Count() == 1u);
    CHECK(lRegistry.FindByName(OpaaxStringID("Taken"))->GetTypeId()
          == TypeIdOf<ProbeComponent>());
}

TEST_CASE("ComponentRegistry: registering the same TYPE twice is refused")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("First"));
    CHECK_FALSE(lRegistry.Register<ProbeComponent>("Second"));

    CHECK(lRegistry.Count() == 1u);
    CHECK(lRegistry.FindByName(OpaaxStringID("Second")) == nullptr);
}

TEST_CASE("ComponentRegistry: an empty name is refused")
{
    ComponentRegistry lRegistry;

    CHECK_FALSE(lRegistry.Register<ProbeComponent>(OpaaxStringID{}));
    CHECK(lRegistry.Count() == 0u);
}

// =============================================================================
// Sealing
// =============================================================================
TEST_CASE("ComponentRegistry: Seal is idempotent and refuses every later registration")
{
    ComponentRegistry lRegistry;

    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    CHECK_FALSE(lRegistry.IsSealed());

    lRegistry.Seal();
    lRegistry.Seal(); // idempotent — a second world must not be a second seal event
    CHECK(lRegistry.IsSealed());

    // The whole point: a type accepted now would be missing from every entity in the world
    // that already exists, with no error anywhere.
    CHECK_FALSE(lRegistry.Register<QuadComponent>("TooLate"));
    CHECK(lRegistry.Count() == 1u);
}

TEST_CASE("WorldManager: the registries seal at the FIRST CreateWorld, not before")
{
    // The registries are the ENGINE's; WorldManager only borrows them to seal on its way to
    // the first world. A bare manager takes them by pointer, which is what makes that testable
    // without standing up an Engine.
    EngineRegistries lRegistries;
    WorldManager     lManager(&lRegistries);

    REQUIRE(lRegistries.Components().Register<ProbeComponent>("Probe"));
    CHECK_FALSE(lRegistries.Components().IsSealed());

    lManager.CreateWorld("First");
    CHECK(lRegistries.Components().IsSealed());

    lManager.CreateWorld("Second"); // still fine — SealAll is idempotent
    CHECK(lRegistries.Components().IsSealed());

    // And the seal means what it says.
    CHECK_FALSE(lRegistries.Components().Register<QuadComponent>("TooLate"));
}

TEST_CASE("WorldManager: a manager with no registries still creates worlds")
{
    // Null registries is the bare-test case. It must not crash on the seal path — the whole
    // point of making the borrow explicit rather than assumed.
    WorldManager lManager;

    CHECK(lManager.GetRegistries() == nullptr);
    CHECK(lManager.CreateWorld("Orphan") != nullptr);
    CHECK(lManager.GetWorldCount() == 1u);
}

TEST_CASE("Engine: QuadComponent is registered natively, DLL-side, and found exe-side")
{
    // Natives are the ENGINE's job now, done in its ctor before any subsystem exists.
    // Constructing an Engine only queues subsystem factories + registers natives — nothing
    // starts, no service is touched.
    Engine lEngine;

    // The cross-boundary statement: RegisterNativeTypes runs inside the DLL (Engine.cpp), the
    // type id below is computed HERE in the exe. A mismatch returns null — see
    // ComponentIdentityTests.cpp for why this can be trusted.
    const IComponentEntry* lEntry =
        lEngine.GetRegistries().Components().Find<QuadComponent>();

    REQUIRE(lEntry != nullptr);
    CHECK(lEntry->GetName() == OpaaxStringID("Quad"));
}

// =============================================================================
// The type-erased entry — Has / Add / Save / Load
// =============================================================================
TEST_CASE("ComponentEntry: Save/Load round-trips a component's values through json")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));

    const IComponentEntry* lEntry = lRegistry.FindByName(OpaaxStringID("Probe"));
    REQUIRE(lEntry != nullptr);

    World  lWorld("EntryTest");
    Entity lSource = lWorld.CreateEntity("Source");
    Entity lTarget = lWorld.CreateEntity("Target");

    const ProbeComponent lOriginal{ 42, 3.5f, OpaaxString("hero") };
    lSource.Add<ProbeComponent>(lOriginal);

    EntityRegistry& lReg = lWorld.GetRegistry();

    CHECK(lEntry->Has(lReg, lSource.GetHandle()));
    CHECK_FALSE(lEntry->Has(lReg, lTarget.GetHandle()));

    const nlohmann::json lJson = lEntry->Save(lReg, lSource.GetHandle());
    lEntry->Load(lReg, lTarget.GetHandle(), lJson);

    REQUIRE(lTarget.Has<ProbeComponent>());
    CHECK(lTarget.Get<ProbeComponent>() == lOriginal);
}

TEST_CASE("ComponentEntry: Load ADDS the component when the target doesn't have it yet")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    const IComponentEntry* lEntry = lRegistry.FindByName(OpaaxStringID("Probe"));

    World  lWorld("EntryTest");
    Entity lEntity = lWorld.CreateEntity("Fresh");

    // This is the instantiate path: the entity is brand new and carries nothing.
    REQUIRE_FALSE(lEntity.Has<ProbeComponent>());

    lEntry->Load(lWorld.GetRegistry(), lEntity.GetHandle(),
                 nlohmann::json{{"Health", 7}, {"Speed", 1.25f}, {"Tag", "spawned"}});

    REQUIRE(lEntity.Has<ProbeComponent>());
    CHECK(lEntity.Get<ProbeComponent>().Health == 7);
    CHECK(lEntity.Get<ProbeComponent>().Tag == "spawned");
}

TEST_CASE("ComponentEntry: Add default-constructs, and is a no-op when already present")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    const IComponentEntry* lEntry = lRegistry.FindByName(OpaaxStringID("Probe"));

    World  lWorld("EntryTest");
    Entity lEntity = lWorld.CreateEntity("Subject");

    lEntry->Add(lWorld.GetRegistry(), lEntity.GetHandle());
    REQUIRE(lEntity.Has<ProbeComponent>());
    CHECK(lEntity.Get<ProbeComponent>().Health == 0);

    lEntity.Get<ProbeComponent>().Health = 99;

    // Adding again must not wipe the edit — the "Add Component" menu path must be safe to
    // press twice.
    lEntry->Add(lWorld.GetRegistry(), lEntity.GetHandle());
    CHECK(lEntity.Get<ProbeComponent>().Health == 99);
}

TEST_CASE("ComponentEntry: Save on an entity without the component yields a null json")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));
    const IComponentEntry* lEntry = lRegistry.FindByName(OpaaxStringID("Probe"));

    World  lWorld("EntryTest");
    Entity lEntity = lWorld.CreateEntity("Empty");

    // Capture asks Has() before Save(), but a null return keeps the entry honest on its own.
    CHECK(lEntry->Save(lWorld.GetRegistry(), lEntity.GetHandle()).is_null());
}

namespace
{
    /** Counts what it sees and edits Size, the way the editor's widgets would. */
    class EntryVisitor final : public IPropertyVisitor
    {
    public:
        TDynArray<OpaaxString> Names;

        void Visit(const char* InName, Vector2F& InValue, const PropertyMeta&) override
        {
            Names.emplace_back(InName);
            InValue = { 7.f, 8.f };
        }
        void Visit(const char* InName, LinearColor&, const PropertyMeta&) override { Names.emplace_back(InName); }

        void Visit(const char*, bool&, const PropertyMeta&) override {}
        void Visit(const char*, Int16&, const PropertyMeta&) override {}
        void Visit(const char*, Int32&, const PropertyMeta&) override {}
        void Visit(const char*, Uint32&, const PropertyMeta&) override {}
        void Visit(const char*, float&, const PropertyMeta&) override {}
        void Visit(const char*, Vector3F&, const PropertyMeta&) override {}
        void Visit(const char*, Vector4F&, const PropertyMeta&) override {}
        void Visit(const char*, OpaaxString&, const PropertyMeta&) override {}
        void Visit(const char*, OpaaxStringID&, const PropertyMeta&) override {}
        void VisitEnum(const char*, const char* const*, Uint32, Uint32&, const PropertyMeta&) override {}
        void VisitResourcePath(const char*, OpaaxString&, Uint32, const PropertyMeta&) override {}
        void VisitDataAssetRef(const char*, OpaaxString&, OpaaxStringID, const PropertyMeta&) override {}
        bool BeginGroup(const char*, const PropertyMeta&) override { return true; }
        void EndGroup() override {}
        void VisitUnsupported(const char*, std::string_view) override {}
    };
}

TEST_CASE("ComponentRegistry: an alias loads an old name as the current type, and guards that name")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<QuadComponent>("Quad"));

    CHECK(lRegistry.AddAlias(OpaaxStringID("Dummy"), OpaaxStringID("Quad")));
    CHECK(lRegistry.FindByName(OpaaxStringID("Dummy")) == lRegistry.FindByName(OpaaxStringID("Quad")));

    // Saved back under the current name: the entry is the Quad one.
    CHECK(lRegistry.FindByName(OpaaxStringID("Dummy"))->GetName() == OpaaxStringID("Quad"));

    // The old name stays taken, or one file key would mean two types.
    CHECK_FALSE(lRegistry.Register<ProbeComponent>("Dummy"));

    // No alias to nothing, and no alias over a live name.
    CHECK_FALSE(lRegistry.AddAlias(OpaaxStringID("Ghost"), OpaaxStringID("Missing")));
    CHECK_FALSE(lRegistry.AddAlias(OpaaxStringID("Quad"), OpaaxStringID("Quad")));

    lRegistry.Seal();
    CHECK_FALSE(lRegistry.AddAlias(OpaaxStringID("Square"), OpaaxStringID("Quad")));
}

TEST_CASE("ComponentRegistry: an entry walks its component's fields without its C++ type")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<QuadComponent>("Quad"));
    REQUIRE(lRegistry.Register<ProbeComponent>("Probe"));

    const IComponentEntry* lQuad = lRegistry.FindByName(OpaaxStringID("Quad"));
    const IComponentEntry* lProbe = lRegistry.FindByName(OpaaxStringID("Probe"));
    REQUIRE(lQuad != nullptr);
    REQUIRE(lProbe != nullptr);

    // Only a type that lists its fields can be drawn generically.
    CHECK(lQuad->IsReflected());
    CHECK_FALSE(lProbe->IsReflected());

    World  lWorld("VisitTest");
    Entity lEntity = lWorld.CreateEntity("Visited");
    EntryVisitor lVisitor;

    // Absent: nothing visited.
    CHECK_FALSE(lQuad->VisitProperties(lWorld.GetRegistry(), lEntity.GetHandle(), lVisitor));
    CHECK(lVisitor.Names.empty());

    lEntity.Add<QuadComponent>();
    CHECK(lQuad->VisitProperties(lWorld.GetRegistry(), lEntity.GetHandle(), lVisitor));

    REQUIRE(lVisitor.Names.size() == 2u);
    CHECK(lVisitor.Names[0] == OpaaxString("Size"));
    CHECK(lVisitor.Names[1] == OpaaxString("Color"));

    // The edit reached the live component.
    CHECK(lEntity.Get<QuadComponent>().Size.x == doctest::Approx(7.f));

    // Not reflected: refused even when present.
    lEntity.Add<ProbeComponent>();
    CHECK_FALSE(lProbe->VisitProperties(lWorld.GetRegistry(), lEntity.GetHandle(), lVisitor));
}
