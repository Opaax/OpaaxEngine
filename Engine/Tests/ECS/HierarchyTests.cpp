// Suite: Hierarchy world-transform composition + parent guards.
//
// Uses a real headless World (entt-backed, OPAAX_API). Hierarchy::SetParent /
// GetWorldTransform / IsDescendantOf are out-of-line OPAAX_API symbols reached via the
// import lib. Composition is root->leaf: the child's local position is scaled by the
// accumulated parent scale and rotated by the accumulated parent rotation before being
// translated; scale multiplies, rotation + ZOrder sum.
#include <doctest.h>

#include "World/World.h"
#include "ECS/Hierarchy.h"
#include "ECS/Components/TransformComponent.h"

using namespace Opaax;
namespace H = Opaax::ECS::Hierarchy;
using Opaax::ECS::TransformComponent;

namespace
{
    constexpr float k_HalfPi = 1.57079632679489661923f;

    TransformComponent& AddTransform(World& InWorld, EntityID InEntity,
                                     Vector2F InPos, Vector2F InScale, float InRot, float InZ)
    {
        auto& lTr   = InWorld.AddComponent<TransformComponent>(InEntity);
        lTr.Position = InPos;
        lTr.Scale    = InScale;
        lTr.Rotation = InRot;
        lTr.ZOrder   = InZ;
        return lTr;
    }
}

TEST_CASE("Hierarchy: a root entity's world transform equals its local transform")
{
    World lWorld;
    const EntityID lE = lWorld.CreateEntity("Root");
    AddTransform(lWorld, lE, { 3.f, 4.f }, { 2.f, 2.f }, 0.5f, 7.f);

    const H::WorldTransform lW = H::GetWorldTransform(lWorld, lE);
    CHECK(lW.Position.x == doctest::Approx(3.f));
    CHECK(lW.Position.y == doctest::Approx(4.f));
    CHECK(lW.Scale.x    == doctest::Approx(2.f));
    CHECK(lW.Scale.y    == doctest::Approx(2.f));
    CHECK(lW.Rotation   == doctest::Approx(0.5f));
    CHECK(lW.ZOrder     == doctest::Approx(7.f));
}

TEST_CASE("Hierarchy: parent-child composes scale/position, sums rotation/ZOrder (no rotation)")
{
    World lWorld;
    const EntityID lParent = lWorld.CreateEntity("Parent");
    const EntityID lChild  = lWorld.CreateEntity("Child");
    AddTransform(lWorld, lParent, { 10.f, 20.f }, { 2.f, 3.f }, 0.f, 1.f);
    AddTransform(lWorld, lChild,  {  5.f,  7.f }, { 4.f, 5.f }, 0.f, 2.f);

    REQUIRE(H::SetParent(lWorld, lChild, lParent));

    const H::WorldTransform lW = H::GetWorldTransform(lWorld, lChild);
    // pos = parentPos + (childPos componentwise* parentScale) = (10,20) + (5*2, 7*3)
    CHECK(lW.Position.x == doctest::Approx(20.f));
    CHECK(lW.Position.y == doctest::Approx(41.f));
    CHECK(lW.Scale.x    == doctest::Approx(8.f));   // 2 * 4
    CHECK(lW.Scale.y    == doctest::Approx(15.f));  // 3 * 5
    CHECK(lW.Rotation   == doctest::Approx(0.f));
    CHECK(lW.ZOrder     == doctest::Approx(3.f));   // 1 + 2
}

TEST_CASE("Hierarchy: parent rotation rotates the child offset")
{
    World lWorld;
    const EntityID lParent = lWorld.CreateEntity("Parent");
    const EntityID lChild  = lWorld.CreateEntity("Child");
    AddTransform(lWorld, lParent, { 0.f, 0.f }, { 1.f, 1.f }, k_HalfPi, 0.f); // +90 deg CCW
    AddTransform(lWorld, lChild,  { 1.f, 0.f }, { 1.f, 1.f }, 0.f,       0.f);

    REQUIRE(H::SetParent(lWorld, lChild, lParent));

    const H::WorldTransform lW = H::GetWorldTransform(lWorld, lChild);
    // rotating local (1,0) by +90 deg about the parent origin -> (0,1)
    CHECK(lW.Position.x == doctest::Approx(0.f).epsilon(0.001));
    CHECK(lW.Position.y == doctest::Approx(1.f).epsilon(0.001));
    CHECK(lW.Rotation   == doctest::Approx(k_HalfPi));
}

TEST_CASE("Hierarchy: an ancestor without a TransformComponent contributes identity")
{
    World lWorld;
    const EntityID lParent = lWorld.CreateEntity("ParentNoTransform"); // no TransformComponent
    const EntityID lChild  = lWorld.CreateEntity("Child");
    AddTransform(lWorld, lChild, { 5.f, 6.f }, { 1.f, 1.f }, 0.f, 0.f);

    REQUIRE(H::SetParent(lWorld, lChild, lParent));

    const H::WorldTransform lW = H::GetWorldTransform(lWorld, lChild);
    CHECK(lW.Position.x == doctest::Approx(5.f));
    CHECK(lW.Position.y == doctest::Approx(6.f));
}

TEST_CASE("Hierarchy: SetParent guards self-parent and cycles; IsDescendantOf tracks the chain")
{
    World lWorld;
    const EntityID lParent = lWorld.CreateEntity("Parent");
    const EntityID lChild  = lWorld.CreateEntity("Child");

    CHECK_FALSE(H::SetParent(lWorld, lChild, lChild));   // self-parent rejected
    CHECK(H::SetParent(lWorld, lChild, lParent));        // valid

    CHECK(H::IsDescendantOf(lWorld, lChild, lParent));
    CHECK_FALSE(H::IsDescendantOf(lWorld, lParent, lChild));

    // Making the parent a child of its own descendant would create a cycle.
    CHECK_FALSE(H::SetParent(lWorld, lParent, lChild));
}

TEST_CASE("Hierarchy: detaching with ENTITY_NONE removes the parent contribution")
{
    World lWorld;
    const EntityID lParent = lWorld.CreateEntity("Parent");
    const EntityID lChild  = lWorld.CreateEntity("Child");
    AddTransform(lWorld, lParent, { 100.f, 0.f }, { 1.f, 1.f }, 0.f, 0.f);
    AddTransform(lWorld, lChild,  {   5.f, 6.f }, { 1.f, 1.f }, 0.f, 0.f);

    REQUIRE(H::SetParent(lWorld, lChild, lParent));
    REQUIRE(H::SetParent(lWorld, lChild, ENTITY_NONE)); // detach

    const H::WorldTransform lW = H::GetWorldTransform(lWorld, lChild);
    CHECK(lW.Position.x == doctest::Approx(5.f)); // parent's +100 no longer applies
    CHECK(lW.Position.y == doctest::Approx(6.f));
}
