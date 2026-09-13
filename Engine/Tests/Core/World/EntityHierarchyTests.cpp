// Suite: parenting (§HR) — the link on EntityMeta, the local/world composition, and what the
// snapshot core does with both.
//
// The rule under test: you AUTHOR local and READ world, and the two never disagree. A root's
// local is its world, so every pre-parenting map keeps its meaning; a child's world is walked.
#include <doctest.h>

#include "World/Components/ComponentRegistry.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapJson.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    constexpr float k_Tolerance = 1e-3f;

    bool Near(const Vector2F& InA, const Vector2F& InB)
    {
        return Maths::Abs(InA.x - InB.x) <= k_Tolerance && Maths::Abs(InA.y - InB.y) <= k_Tolerance;
    }

    bool Near(const TransformComponent& InA, const TransformComponent& InB)
    {
        return Near(InA.Position, InB.Position)
            && Maths::Abs(InA.Rotation - InB.Rotation) <= k_Tolerance
            && Near(InA.Scale, InB.Scale);
    }

    TransformComponent MakeXf(const Vector2F& InPosition, const float InRotation, const Vector2F& InScale = { 1.f, 1.f })
    {
        TransformComponent lXf;
        lXf.Position = InPosition;
        lXf.Rotation = InRotation;
        lXf.Scale    = InScale;
        return lXf;
    }

    const EntityData* FindData(const MapData& InData, const Guid& InId)
    {
        for (const EntityData& lEntity : InData.Entities)
        {
            if (lEntity.Id == InId) { return &lEntity; }
        }
        return nullptr;
    }
}

// =============================================================================
// The pure half
// =============================================================================
TEST_CASE("Hierarchy: Compose rotates and scales the local by the parent, then translates")
{
    const TransformComponent lParent = MakeXf({ 100.f, 0.f }, 90.f);
    const TransformComponent lLocal  = MakeXf({ 10.f, 0.f }, 0.f);

    const TransformComponent lWorld = Compose(lParent, lLocal);

    CHECK(Near(lWorld.Position, { 100.f, 10.f }));
    CHECK(lWorld.Rotation == doctest::Approx(90.f));
}

TEST_CASE("Hierarchy: ToLocal is Compose's inverse under a rotated, non-uniformly scaled parent")
{
    const TransformComponent lParent = MakeXf({ -30.f, 12.f }, 37.f, { 2.f, 0.5f });
    const TransformComponent lLocal  = MakeXf({ 4.f, -9.f }, -15.f, { 1.5f, 3.f });

    const TransformComponent lBack = ToLocal(lParent, Compose(lParent, lLocal));

    CHECK(Near(lBack, lLocal));
}

TEST_CASE("Hierarchy: a zero parent scale axis passes through rather than dividing by zero")
{
    const TransformComponent lParent = MakeXf({ 0.f, 0.f }, 0.f, { 0.f, 1.f });
    const TransformComponent lWorld  = MakeXf({ 5.f, 7.f }, 0.f);

    const TransformComponent lLocal = ToLocal(lParent, lWorld);

    CHECK(lLocal.Position.x == doctest::Approx(5.f));
    CHECK(lLocal.Position.y == doctest::Approx(7.f));
}

// =============================================================================
// The walk
// =============================================================================
TEST_CASE("Hierarchy: WorldTransform composes the chain root -> leaf; a root answers its own transform")
{
    World lWorld("Hierarchy");

    Entity lRoot  = lWorld.CreateEntity("Root");
    Entity lMid   = lWorld.CreateEntity("Mid");
    Entity lLeaf  = lWorld.CreateEntity("Leaf");

    lRoot.Get<TransformComponent>() = MakeXf({ 100.f, 0.f }, 90.f);
    lMid.Get<TransformComponent>()  = MakeXf({ 10.f, 0.f }, 0.f);
    lLeaf.Get<TransformComponent>() = MakeXf({ 0.f, 5.f }, 0.f);

    REQUIRE(EntityHierarchy::SetParent(lMid, lRoot, /*bKeepWorld*/false));
    REQUIRE(EntityHierarchy::SetParent(lLeaf, lMid, /*bKeepWorld*/false));

    CHECK(Near(EntityHierarchy::WorldTransform(lRoot).Position, { 100.f, 0.f }));
    CHECK(Near(EntityHierarchy::WorldTransform(lMid).Position,  { 100.f, 10.f }));
    CHECK(Near(EntityHierarchy::WorldTransform(lLeaf).Position, { 95.f, 10.f }));

    CHECK(EntityHierarchy::GetParent(lLeaf).GetHandle() == lMid.GetHandle());
    CHECK_FALSE(EntityHierarchy::GetParent(lRoot).IsValid());
}

TEST_CASE("Hierarchy: SetParent keeps the world pose — nothing visibly moves")
{
    World lWorld("Hierarchy");

    Entity lParent = lWorld.CreateEntity("Parent");
    Entity lChild  = lWorld.CreateEntity("Child");

    lParent.Get<TransformComponent>() = MakeXf({ 100.f, 0.f }, 90.f, { 2.f, 2.f });
    lChild.Get<TransformComponent>()  = MakeXf({ 10.f, 0.f }, 45.f);

    REQUIRE(EntityHierarchy::SetParent(lChild, lParent));

    CHECK(Near(EntityHierarchy::WorldTransform(lChild), MakeXf({ 10.f, 0.f }, 45.f)));
    CHECK_FALSE(Near(lChild.Get<TransformComponent>().Position, { 10.f, 0.f }));   // the local moved

    // And back to root, still where it was.
    REQUIRE(EntityHierarchy::SetParent(lChild, Entity{}));
    CHECK(Near(lChild.Get<TransformComponent>(), MakeXf({ 10.f, 0.f }, 45.f)));
    CHECK_FALSE(lChild.Get<EntityMeta>().Parent.IsValid());
}

TEST_CASE("Hierarchy: SetWorldTransform stores the local that lands the entity there")
{
    World lWorld("Hierarchy");

    Entity lParent = lWorld.CreateEntity("Parent");
    Entity lChild  = lWorld.CreateEntity("Child");
    lParent.Get<TransformComponent>() = MakeXf({ 50.f, 50.f }, 180.f);

    REQUIRE(EntityHierarchy::SetParent(lChild, lParent, false));

    EntityHierarchy::SetWorldTransform(lChild, MakeXf({ 40.f, 50.f }, 180.f));

    CHECK(Near(lChild.Get<TransformComponent>().Position, { 10.f, 0.f }));
    CHECK(Near(EntityHierarchy::WorldTransform(lChild).Position, { 40.f, 50.f }));
}

// =============================================================================
// Refusals
// =============================================================================
TEST_CASE("Hierarchy: SetParent refuses self, a cycle, and a parent in another world")
{
    World lWorld("Hierarchy");
    World lOther("Other");

    Entity lA = lWorld.CreateEntity("A");
    Entity lB = lWorld.CreateEntity("B");
    Entity lC = lWorld.CreateEntity("C");
    Entity lX = lOther.CreateEntity("X");

    REQUIRE(EntityHierarchy::SetParent(lB, lA));
    REQUIRE(EntityHierarchy::SetParent(lC, lB));

    CHECK_FALSE(EntityHierarchy::SetParent(lA, lA));
    CHECK_FALSE(EntityHierarchy::SetParent(lA, lC));   // A is C's grandparent
    CHECK_FALSE(EntityHierarchy::SetParent(lA, lX));

    CHECK_FALSE(lA.Get<EntityMeta>().Parent.IsValid());   // untouched by the refusals
    CHECK(EntityHierarchy::IsDescendantOf(lC, lA));
    CHECK_FALSE(EntityHierarchy::IsDescendantOf(lA, lC));
}

// =============================================================================
// Maps and the subtree
// =============================================================================
TEST_CASE("Hierarchy: a child lives in its parent's map; a runtime-spawned one stays runtime")
{
    World lWorld("Hierarchy");

    const MapId lMapA = MapId("A");
    const MapId lMapB = MapId("B");

    Entity lParent     = lWorld.CreateEntity("Parent", lMapA);
    Entity lChild      = lWorld.CreateEntity("Child", lMapB);
    Entity lGrandchild = lWorld.CreateEntity("Grandchild", lMapB);
    Entity lSpawned    = lWorld.CreateEntity("Bullet");   // no map

    REQUIRE(EntityHierarchy::SetParent(lGrandchild, lChild));
    REQUIRE(EntityHierarchy::SetParent(lSpawned, lChild));
    REQUIRE(EntityHierarchy::SetParent(lChild, lParent));

    CHECK(lChild.Get<EntityMeta>().OwnerMap == lMapA);
    CHECK(lGrandchild.Get<EntityMeta>().OwnerMap == lMapA);
    CHECK_FALSE(lSpawned.Get<EntityMeta>().OwnerMap.IsValid());
}

TEST_CASE("Hierarchy: CollectSubtree takes every descendant once; TopmostOf drops covered ids")
{
    World lWorld("Hierarchy");

    Entity lRoot  = lWorld.CreateEntity("Root");
    Entity lChild = lWorld.CreateEntity("Child");
    Entity lLeaf  = lWorld.CreateEntity("Leaf");
    Entity lLoose = lWorld.CreateEntity("Loose");

    REQUIRE(EntityHierarchy::SetParent(lChild, lRoot));
    REQUIRE(EntityHierarchy::SetParent(lLeaf, lChild));

    TDynArray<EntityID> lSubtree;
    EntityHierarchy::CollectSubtree(lWorld, { lRoot.GetHandle(), lLeaf.GetHandle() }, lSubtree);
    CHECK(lSubtree.size() == 3u);
    CHECK(lSubtree.front() == lRoot.GetHandle());

    TDynArray<EntityID> lTopmost;
    EntityHierarchy::TopmostOf(lWorld, { lLeaf.GetHandle(), lRoot.GetHandle(), lLoose.GetHandle() }, lTopmost);
    REQUIRE(lTopmost.size() == 2u);
    CHECK(lTopmost[0] == lRoot.GetHandle());
    CHECK(lTopmost[1] == lLoose.GetHandle());
}

TEST_CASE("Hierarchy: destroying a parent destroys its subtree and the guid registry forgets them")
{
    World lWorld("Hierarchy");

    Entity lRoot  = lWorld.CreateEntity("Root");
    Entity lChild = lWorld.CreateEntity("Child");
    Entity lLeaf  = lWorld.CreateEntity("Leaf");
    Entity lLoose = lWorld.CreateEntity("Loose");

    REQUIRE(EntityHierarchy::SetParent(lChild, lRoot));
    REQUIRE(EntityHierarchy::SetParent(lLeaf, lChild));

    const Guid lChildGuid = lChild.GetGuid();
    const Guid lLeafGuid  = lLeaf.GetGuid();

    lWorld.DestroyEntity(lRoot);

    CHECK(lWorld.GetEntityCount() == 1u);
    CHECK_FALSE(lWorld.FindByGuid(lChildGuid).IsValid());
    CHECK_FALSE(lWorld.FindByGuid(lLeafGuid).IsValid());
    CHECK(lLoose.IsValid());
}

// =============================================================================
// The snapshot core and the file
// =============================================================================
TEST_CASE("Hierarchy: the link survives capture -> instantiate, and the file writes `parent` only for a child")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<TransformComponent>("Transform"));

    const MapId lMap = MapId("Level");
    World       lWorld("Source");

    Entity lParent = lWorld.CreateEntity("Parent", lMap);
    Entity lChild  = lWorld.CreateEntity("Child", lMap);
    lParent.Get<TransformComponent>() = MakeXf({ 100.f, 0.f }, 90.f);
    lChild.Get<TransformComponent>()  = MakeXf({ 10.f, 0.f }, 0.f);
    REQUIRE(EntityHierarchy::SetParent(lChild, lParent, false));

    const MapData lCaptured = MapSerializer::CaptureMap(lWorld, lRegistry, lMap);

    const nlohmann::json lJson = MapJson::ToJson(lCaptured);
    CHECK(lJson[MapJson::KEY_VERSION].get<Uint32>() == MapJson::MAP_FORMAT_VERSION_PARENTS);

    Uint32 lWithParent = 0;
    for (const nlohmann::json& lEntity : lJson[MapJson::KEY_ENTITIES])
    {
        if (lEntity.contains(MapJson::KEY_PARENT)) { ++lWithParent; }
    }
    CHECK(lWithParent == 1u);

    MapData lRead;
    REQUIRE(MapJson::FromJson(lJson, lRead));

    World lRestored("Restored");
    REQUIRE(MapFactory::Instantiate(lRead, lRestored, lRegistry) == 2u);

    Entity lChildAgain = lRestored.FindByGuid(lChild.GetGuid());
    REQUIRE(lChildAgain.IsValid());
    CHECK(lChildAgain.Get<EntityMeta>().Parent == lParent.GetGuid());
    CHECK(Near(EntityHierarchy::WorldTransform(lChildAgain).Position, { 100.f, 10.f }));
}

TEST_CASE("Hierarchy: a link naming an entity not in the world is cleared at instantiate")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<TransformComponent>("Transform"));

    MapData lData;
    lData.Id = MapId("Level");

    EntityData lOrphan;
    lOrphan.Id       = Guid::New();
    lOrphan.Name     = OpaaxString("Orphan");
    lOrphan.OwnerMap = lData.Id;
    lOrphan.Parent   = Guid::New();   // nobody
    lData.Entities.emplace_back(Move(lOrphan));

    World lWorld("Orphans");
    REQUIRE(MapFactory::Instantiate(lData, lWorld, lRegistry) == 1u);

    Entity lLive = lWorld.FindByGuid(lData.Entities.front().Id);
    REQUIRE(lLive.IsValid());
    CHECK_FALSE(lLive.Get<EntityMeta>().Parent.IsValid());
    CHECK_FALSE(EntityHierarchy::GetParent(lLive).IsValid());
}

TEST_CASE("Hierarchy: a v3 map (no `parent` keys) reads as all roots; an unreadable parent reads as root")
{
    const OpaaxString lText(
        "{\"entities\":["
        "{\"components\":{},\"guid\":\"0123456789abcdef0123456789abcdef\",\"name\":\"A\",\"ownerMap\":\"M\"},"
        "{\"components\":{},\"guid\":\"fedcba9876543210fedcba9876543210\",\"name\":\"B\",\"ownerMap\":\"M\",\"parent\":\"not-a-guid\"}"
        "],\"mapId\":\"M\",\"version\":3}");

    MapData lRead;
    REQUIRE(MapJson::Deserialize(lText, lRead));
    REQUIRE(lRead.EntityCount() == 2u);

    for (const EntityData& lEntity : lRead.Entities)
    {
        CHECK_FALSE(lEntity.Parent.IsValid());
    }
}
