// Suite: EntityRef — a field naming an entity by guid: its saved form, how it resolves, how a walk over
// saved data re-points it, and how reflection reports it.
#include <doctest.h>

#include "Engine/Reflection/PropertyJson.h"
#include "Engine/Reflection/PropertyVisitor.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityRef.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    struct RefTestTurret
    {
        EntityRef            Target;
        TDynArray<EntityRef> Patrol;
        float                Range = 5.f;

        OPAAX_PROPERTIES(RefTestTurret,
                         OPAAX_PROP(Target),
                         OPAAX_PROP(Patrol),
                         OPAAX_PROP(Range))
    };

    /** Records the entity references it is shown, and points them at Replacement. */
    class RefVisitor final : public IPropertyVisitor
    {
    public:
        TDynArray<OpaaxString> Names;
        Guid                   Replacement;

        void VisitEntityRef(const char* InName, Guid& InOutId, const PropertyMeta&) override
        {
            Names.emplace_back(InName);
            InOutId = Replacement;
        }

        void Visit(const char*, bool&, const PropertyMeta&) override {}
        void Visit(const char*, Int16&, const PropertyMeta&) override {}
        void Visit(const char*, Int32&, const PropertyMeta&) override {}
        void Visit(const char*, Uint32&, const PropertyMeta&) override {}
        void Visit(const char*, float&, const PropertyMeta&) override {}
        void Visit(const char*, Vector2F&, const PropertyMeta&) override {}
        void Visit(const char*, Vector3F&, const PropertyMeta&) override {}
        void Visit(const char*, Vector4F&, const PropertyMeta&) override {}
        void Visit(const char*, LinearColor&, const PropertyMeta&) override {}
        void Visit(const char*, OpaaxString&, const PropertyMeta&) override {}
        void Visit(const char*, OpaaxStringID&, const PropertyMeta&) override {}
        void VisitEnum(const char*, const char* const*, Uint32, Uint32&, const PropertyMeta&) override {}
        void VisitAssetRef(const char*, OpaaxString&, const ObjectClass&, const PropertyMeta&) override {}
        void VisitObjectRef(const char*, ObjectHandle&, const ObjectClass&, const PropertyMeta&) override {}
        bool BeginGroup(const char*, const PropertyMeta&) override { return true; }
        void EndGroup() override {}
        void VisitUnsupported(const char*, OpaaxStringView) override {}
    };
}

TEST_CASE("EntityRef: saved as {\"entityRef\": guid}, read back the same")
{
    EntityRef lRef;
    lRef.Id = Guid::New();

    const nlohmann::json lJson = lRef;
    CHECK(EntityRefJson::IsRef(lJson));
    CHECK(lJson.at(EntityRefJson::KEY).get<OpaaxString>() == lRef.Id.ToString());
    CHECK(lJson.get<EntityRef>() == lRef);

    // Empty names nothing, and stays empty.
    const nlohmann::json lEmpty = EntityRef{};
    CHECK(lEmpty.at(EntityRefJson::KEY).get<OpaaxString>().IsEmpty());
    CHECK(lEmpty.get<EntityRef>().IsEmpty());
}

TEST_CASE("EntityRef: a bare guid written by hand reads; anything else names nothing")
{
    const Guid lId = Guid::New();
    CHECK(nlohmann::json(lId.ToString().CStr()).get<EntityRef>().Id == lId);

    CHECK(nlohmann::json(42).get<EntityRef>().IsEmpty());
    CHECK(nlohmann::json::parse("{\"entityRef\": \"not a guid\"}").get<EntityRef>().IsEmpty());
    CHECK(nlohmann::json::parse("{\"other\": 1}").get<EntityRef>().IsEmpty());
}

TEST_CASE("EntityRef: resolves to the entity of its guid, whatever its name")
{
    World  lWorld("Refs");
    Entity lTarget = lWorld.CreateEntity("Player", MapId("Level"));
    lWorld.CreateEntity("Player", MapId("Level"));   // another of the same name changes nothing

    EntityRef lRef;
    lRef.Id = lTarget.GetGuid();
    CHECK(lRef.Resolve(lWorld).GetHandle() == lTarget.GetHandle());

    lWorld.DestroyEntity(lTarget.GetHandle());
    CHECK_FALSE(lRef.Resolve(lWorld).IsValid());
    CHECK_FALSE(EntityRef{}.Resolve(lWorld).IsValid());
}

TEST_CASE("EntityRef: the walk re-points references at any depth, and leaves the others")
{
    const Guid lInside  = Guid::New();
    const Guid lCopy    = Guid::New();
    const Guid lOutside = Guid::New();

    RefTestTurret lTurret;
    lTurret.Target.Id = lInside;
    lTurret.Patrol    = { EntityRef{ lOutside }, EntityRef{ lInside }, EntityRef{} };

    nlohmann::json lJson = PropertiesToJson(lTurret);

    const Uint64 lChanged = EntityRefJson::Remap(lJson, [&](const Guid& InId) { return InId == lInside ? lCopy : InId; });
    CHECK(lChanged == 2);

    RefTestTurret lRead;
    REQUIRE(PropertiesFromJson(lJson, lRead));
    CHECK(lRead.Target.Id == lCopy);
    CHECK(lRead.Patrol[0].Id == lOutside);
    CHECK(lRead.Patrol[1].Id == lCopy);
    CHECK(lRead.Patrol[2].IsEmpty());
    CHECK(lRead.Range == doctest::Approx(5.f));
}

TEST_CASE("EntityRef: reflection reports it as an entity reference, in lists too")
{
    RefTestTurret lTurret;
    lTurret.Patrol = { EntityRef{}, EntityRef{} };

    RefVisitor lVisitor;
    lVisitor.Replacement = Guid::New();
    VisitProperties(lTurret, lVisitor);

    CHECK(lVisitor.Names.size() == 3);
    CHECK(lTurret.Target.Id == lVisitor.Replacement);
    CHECK(lTurret.Patrol[1].Id == lVisitor.Replacement);
}
