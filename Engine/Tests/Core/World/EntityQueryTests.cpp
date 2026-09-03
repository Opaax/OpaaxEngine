// Suite: the one entity-AABB rule (World/Entity/EntityQuery.h).
//
// This is the only part of block 2 a test can reach — picking, the marquee and focus all live in
// the editor, which OpaaxTests cannot link. So everything that can be decided without pixels is
// decided here: which tier an entity's bounds come from, that the anchor fallback is opt-in, and
// that "topmost" means the renderer's order rather than iteration order.
#include <doctest.h>

#include "Core/Maths/Bounds2D.h"
#include "World/Components/DummyComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Components/TextComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityQuery.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    /** An entity that DRAWS something, at a position, so a hit test has a real box to find. */
    Entity MakeQuad(World& InWorld, const char* InName, const Vector2F& InPos, const Vector2F& InSize)
    {
        Entity lEntity = InWorld.CreateEntity(InName);
        lEntity.Get<TransformComponent>().Position = InPos;
        lEntity.Add<DummyComponent>().Size         = InSize;

        return lEntity;
    }
}

// =============================================================================
// Tier 1 — a component with an extent
// =============================================================================
TEST_CASE("EntityQuery: bounds come from the extent component, centred on the TRANSFORM")
{
    World lWorld("Bounds");

    Entity lQuad = MakeQuad(lWorld, "Quad", { 100.f, -50.f }, { 80.f, 40.f });

    Bounds2D lBounds;
    REQUIRE(EntityQuery::TryGetBounds(lQuad, lBounds));

    // The position is the transform's, the size is the component's — the split this block exists to
    // make. Reading a position off DummyComponent would not compile any more, but centring on the
    // ORIGIN instead would still "work" and be silently wrong for every entity but one.
    CHECK(lBounds.Center.x == doctest::Approx(100.f));
    CHECK(lBounds.Center.y == doctest::Approx(-50.f));
    CHECK(lBounds.HalfExtent.x == doctest::Approx(40.f));
    CHECK(lBounds.HalfExtent.y == doctest::Approx(20.f));
}

TEST_CASE("EntityQuery: an entity carrying BOTH a quad and a sprite gets the union")
{
    World lWorld("Union");

    Entity lEntity = MakeQuad(lWorld, "Both", { 0.f, 0.f }, { 40.f, 40.f });
    lEntity.Add<SpriteComponent>().Size = { 200.f, 20.f };

    Bounds2D lBounds;
    REQUIRE(EntityQuery::TryGetBounds(lEntity, lBounds));

    // Widest of the two on each axis independently — not whichever component was asked first.
    CHECK(lBounds.HalfExtent.x == doctest::Approx(100.f));
    CHECK(lBounds.HalfExtent.y == doctest::Approx(20.f));
}

TEST_CASE("EntityQuery: bounds follow the transform's ROTATION")
{
    World lWorld("Rotated");

    Entity lQuad = MakeQuad(lWorld, "Long", { 0.f, 0.f }, { 100.f, 20.f });
    lQuad.Get<TransformComponent>().Rotation = 90.f;   // DEGREES — what the Inspector authors

    Bounds2D lBounds;
    REQUIRE(EntityQuery::TryGetBounds(lQuad, lBounds));

    // Turned on its side, so the extents swap. If the degrees->radians conversion were missing the
    // box would barely change (90 radians is ~5157 degrees) and a rotated sprite would be
    // unclickable at its ends.
    CHECK(lBounds.HalfExtent.x == doctest::Approx(10.f));
    CHECK(lBounds.HalfExtent.y == doctest::Approx(50.f));
}

TEST_CASE("EntityQuery: bounds follow the transform's SCALE")
{
    World lWorld("Scaled");

    Entity lQuad = MakeQuad(lWorld, "Big", { 0.f, 0.f }, { 100.f, 40.f });
    lQuad.Get<TransformComponent>().Scale = { 2.f, 0.5f };

    Bounds2D lBounds;
    REQUIRE(EntityQuery::TryGetBounds(lQuad, lBounds));

    // The SAME multiply RendererManager applies. If these two ever disagree a scaled entity is
    // clickable somewhere other than where it is drawn — the failure SEL1 exists to prevent, and
    // one this file is the only thing that can catch (the draw path needs a GL context).
    CHECK(lBounds.HalfExtent.x == doctest::Approx(100.f));
    CHECK(lBounds.HalfExtent.y == doctest::Approx(10.f));
}

TEST_CASE("EntityQuery: scale and rotation compose, in that order")
{
    World lWorld("ScaledRotated");

    Entity lQuad = MakeQuad(lWorld, "Long", { 0.f, 0.f }, { 100.f, 20.f });
    lQuad.Get<TransformComponent>().Scale    = { 2.f, 1.f };   // -> 200 x 20
    lQuad.Get<TransformComponent>().Rotation = 90.f;           // -> swaps the extents

    Bounds2D lBounds;
    REQUIRE(EntityQuery::TryGetBounds(lQuad, lBounds));

    // Scaling the LOCAL size and then rotating is not the same as rotating and then scaling the
    // world box — with a non-uniform scale the two differ, and only the first matches what the
    // renderer draws.
    CHECK(lBounds.HalfExtent.x == doctest::Approx(10.f));
    CHECK(lBounds.HalfExtent.y == doctest::Approx(100.f));
}

// =============================================================================
// Tier 2 — the anchor fallback, and its opt-in
// =============================================================================
TEST_CASE("EntityQuery: an entity with NO extent has no bounds unless an anchor is asked for")
{
    World lWorld("Empty");

    Entity lBare = lWorld.CreateEntity("Bare");
    lBare.Get<TransformComponent>().Position = { 25.f, 75.f };

    // Default is zero — a game asking what an entity DRAWS must get false, not a placeholder box.
    Bounds2D lBounds;
    CHECK_FALSE(EntityQuery::TryGetBounds(lBare, lBounds));

    // The editor opts in, and gets a clickable box at the transform. This is the whole reason an
    // empty entity can be selected at all.
    REQUIRE(EntityQuery::TryGetBounds(lBare, lBounds, 8.f));
    CHECK(lBounds.Center.x == doctest::Approx(25.f));
    CHECK(lBounds.Center.y == doctest::Approx(75.f));
    CHECK(lBounds.HalfExtent.x == doctest::Approx(8.f));
    CHECK(lBounds.HalfExtent.y == doctest::Approx(8.f));
}

TEST_CASE("EntityQuery: an invalid entity has no bounds")
{
    Bounds2D lBounds;
    CHECK_FALSE(EntityQuery::TryGetBounds(Entity{}, lBounds, 8.f));
}

// =============================================================================
// Text — the one renderable that is NOT centred on its transform
// =============================================================================
TEST_CASE("EntityQuery: a text's box hangs DOWN-RIGHT of the transform, not around it")
{
    World lWorld("Text");

    Entity lText = lWorld.CreateEntity("Label");
    lText.Get<TransformComponent>().Position = { 10.f, 200.f };

    TextComponent& lComp = lText.Add<TextComponent>();
    lComp.Text = "Hello";
    lComp.Size = 40.f;

    Bounds2D lBounds;
    REQUIRE(EntityQuery::TryGetBounds(lText, lBounds));

    // The transform is where the first line STARTS — top-left — so the box sits to the right of it
    // and below it. Centring on the position (every other renderable's rule) would put half the
    // clickable area where there are no glyphs, which is the bug this case exists to catch.
    CHECK(lBounds.Center.x > 10.f);
    CHECK(lBounds.Center.y < 200.f);

    // The transform's corner is ON the box, not outside it.
    CHECK(lBounds.Center.x - lBounds.HalfExtent.x == doctest::Approx(10.f));
    CHECK(lBounds.Center.y + lBounds.HalfExtent.y == doctest::Approx(200.f));
}

TEST_CASE("EntityQuery: an EMPTY string has no extent, so a text entity falls back to the anchor")
{
    World lWorld("Text");

    Entity lText = lWorld.CreateEntity("Label");
    lText.Get<TransformComponent>().Position = { 0.f, 0.f };
    lText.Add<TextComponent>().Text = OpaaxString();

    // Nothing is drawn, so nothing is claimed — the entity is an icon like any other empty one.
    Bounds2D lBounds;
    CHECK_FALSE(EntityQuery::TryGetBounds(lText, lBounds));
    CHECK(EntityQuery::TryGetBounds(lText, lBounds, 8.f));
}

TEST_CASE("EntityQuery: a text and a sprite on one entity get the UNION, and the text's LAYER counts")
{
    World lWorld("Text");

    Entity lBoth = lWorld.CreateEntity("Both");
    lBoth.Get<TransformComponent>().Position = { 0.f, 0.f };
    lBoth.Add<SpriteComponent>().Size        = { 10.f, 10.f };

    TextComponent& lComp = lBoth.Add<TextComponent>();
    lComp.Text  = "wide enough to matter";
    lComp.Size  = 40.f;
    lComp.Layer = ERenderLayer::UI;

    Bounds2D lBounds;
    REQUIRE(EntityQuery::TryGetBounds(lBoth, lBounds));
    CHECK(lBounds.HalfExtent.x > 5.f);   // the sprite alone would be 5

    // A UI-band text must win a click over a Default-band sprite behind it, which only holds if
    // DrawRank reads the text's layer too.
    Entity lQuad = MakeQuad(lWorld, "Behind", { 0.f, 0.f }, { 400.f, 400.f });
    CHECK(EntityQuery::PickAt(lWorld, { 5.f, -5.f }).GetHandle() == lBoth.GetHandle());
    CHECK(lQuad.IsValid());
}

// =============================================================================
// PickAt
// =============================================================================
TEST_CASE("EntityQuery: PickAt finds what is under the point, and nothing where there is nothing")
{
    World lWorld("Pick");

    Entity lLeft  = MakeQuad(lWorld, "Left", { -100.f, 0.f }, { 50.f, 50.f });
    Entity lRight = MakeQuad(lWorld, "Right", { 100.f, 0.f }, { 50.f, 50.f });

    CHECK(EntityQuery::PickAt(lWorld, { -100.f, 0.f }).GetHandle() == lLeft.GetHandle());
    CHECK(EntityQuery::PickAt(lWorld, { 100.f, 10.f }).GetHandle() == lRight.GetHandle());

    // A miss is an INVALID entity, which is what lets a click on empty space clear the selection
    // rather than keep the last hit.
    CHECK_FALSE(EntityQuery::PickAt(lWorld, { 0.f, 0.f }).IsValid());
    CHECK_FALSE(EntityQuery::PickAt(lWorld, { -100.f, 400.f }).IsValid());
}

TEST_CASE("EntityQuery: PickAt returns the TOPMOST by layer, then by order in layer")
{
    World lWorld("Stack");

    // Three overlapping sprites at one point, created BACK to front and then deliberately out of
    // order, so passing this cannot be an accident of iteration order.
    Entity lBackground = lWorld.CreateEntity("Background");
    SpriteComponent& lBack = lBackground.Add<SpriteComponent>();
    lBack.Size  = { 100.f, 100.f };
    lBack.Layer = ERenderLayer::Background;

    Entity lFront = lWorld.CreateEntity("Front");
    SpriteComponent& lFrontSprite = lFront.Add<SpriteComponent>();
    lFrontSprite.Size         = { 100.f, 100.f };
    lFrontSprite.Layer        = ERenderLayer::Default;
    lFrontSprite.OrderInLayer = 10;

    Entity lMiddle = lWorld.CreateEntity("Middle");
    SpriteComponent& lMiddleSprite = lMiddle.Add<SpriteComponent>();
    lMiddleSprite.Size         = { 100.f, 100.f };
    lMiddleSprite.Layer        = ERenderLayer::Default;
    lMiddleSprite.OrderInLayer = 5;

    // The band wins first: Default beats Background whatever the fine key says.
    CHECK(EntityQuery::PickAt(lWorld, { 0.f, 0.f }).GetHandle() == lFront.GetHandle());

    // Within one band the fine key decides. Middle was created LAST, so a "last one wins"
    // implementation returns it instead.
    lFrontSprite.OrderInLayer = 1;
    CHECK(EntityQuery::PickAt(lWorld, { 0.f, 0.f }).GetHandle() == lMiddle.GetHandle());
}

TEST_CASE("EntityQuery: an anchor-only entity never steals a click from something drawn")
{
    World lWorld("IconUnderSprite");

    Entity lQuad = MakeQuad(lWorld, "Quad", { 0.f, 0.f }, { 100.f, 100.f });

    // Sitting at the same spot, created after — so it wins on iteration order and on any tie rule.
    // It must still lose, or an invisible entity would shadow the sprite an author is aiming at.
    Entity lMarker = lWorld.CreateEntity("Marker");
    lMarker.Get<TransformComponent>().Position = { 0.f, 0.f };

    CHECK(EntityQuery::PickAt(lWorld, { 0.f, 0.f }, 8.f).GetHandle() == lQuad.GetHandle());

    // Clear of the quad, the marker is exactly what should be picked.
    lMarker.Get<TransformComponent>().Position = { 300.f, 0.f };
    CHECK(EntityQuery::PickAt(lWorld, { 300.f, 0.f }, 8.f).GetHandle() == lMarker.GetHandle());
}

// =============================================================================
// QueryOverlapping — the marquee
// =============================================================================
TEST_CASE("EntityQuery: QueryOverlapping takes everything the region touches")
{
    World lWorld("Marquee");

    MakeQuad(lWorld, "A", { 0.f, 0.f }, { 50.f, 50.f });
    MakeQuad(lWorld, "B", { 100.f, 0.f }, { 50.f, 50.f });
    MakeQuad(lWorld, "C", { 1000.f, 0.f }, { 50.f, 50.f });

    TDynArray<EntityID> lHits;
    EntityQuery::QueryOverlapping(lWorld, Bounds2D::FromMinMax({ -200.f, -200.f }, { 200.f, 200.f }), lHits);
    CHECK(lHits.size() == 2u);

    // It APPENDS and never clears, so a Ctrl+drag can accumulate across gestures.
    EntityQuery::QueryOverlapping(lWorld, Bounds2D::FromMinMax({ 900.f, -50.f }, { 1100.f, 50.f }), lHits);
    CHECK(lHits.size() == 3u);

    // Grazing counts — a marquee that only clips a sprite's edge still selected it on screen.
    TDynArray<EntityID> lGrazed;
    EntityQuery::QueryOverlapping(lWorld, Bounds2D::FromMinMax({ 25.f, 0.f }, { 60.f, 10.f }), lGrazed);
    CHECK(lGrazed.size() == 1u);
}

TEST_CASE("EntityQuery: the marquee catches anchor-only entities only when asked")
{
    World lWorld("MarqueeIcons");

    Entity lBare = lWorld.CreateEntity("Bare");
    lBare.Get<TransformComponent>().Position = { 10.f, 10.f };

    const Bounds2D lRegion = Bounds2D::FromMinMax({ -100.f, -100.f }, { 100.f, 100.f });

    TDynArray<EntityID> lWithout;
    EntityQuery::QueryOverlapping(lWorld, lRegion, lWithout);
    CHECK(lWithout.empty());

    // Whatever the icon draws, the marquee has to be able to catch — the anchor size is one value
    // shared by the drawing and the hit test, so they cannot disagree.
    TDynArray<EntityID> lWith;
    EntityQuery::QueryOverlapping(lWorld, lRegion, lWith, 8.f);
    CHECK(lWith.size() == 1u);
}

// =============================================================================
// Combined bounds — focus on a multi-selection
// =============================================================================
TEST_CASE("EntityQuery: combined bounds cover every listed entity, skipping the ones with none")
{
    World lWorld("Combined");

    Entity lA = MakeQuad(lWorld, "A", { -100.f, 0.f }, { 40.f, 40.f });
    Entity lB = MakeQuad(lWorld, "B", { 100.f, 60.f }, { 40.f, 40.f });

    Entity lBare = lWorld.CreateEntity("Bare");
    lBare.Get<TransformComponent>().Position = { 5000.f, 0.f };

    TDynArray<EntityID> lIds;
    lIds.emplace_back(lA.GetHandle());
    lIds.emplace_back(lB.GetHandle());
    lIds.emplace_back(lBare.GetHandle());

    // No anchor: the bare entity contributes nothing, so focusing on this selection must not fly
    // the camera out to 5000.
    Bounds2D lBounds;
    REQUIRE(EntityQuery::TryGetBounds(lWorld, lIds, lBounds));
    CHECK(lBounds.Min().x == doctest::Approx(-120.f));
    CHECK(lBounds.Max().x == doctest::Approx(120.f));
    CHECK(lBounds.Max().y == doctest::Approx(80.f));

    // With one, it counts — and the box has to reach it.
    REQUIRE(EntityQuery::TryGetBounds(lWorld, lIds, lBounds, 8.f));
    CHECK(lBounds.Max().x == doctest::Approx(5008.f));

    // Nothing selectable at all is false, not an empty box at the origin: a caller has to be able
    // to tell "nothing to frame" from "frame the origin".
    TDynArray<EntityID> lNone;
    CHECK_FALSE(EntityQuery::TryGetBounds(lWorld, lNone, lBounds));
}

// =============================================================================
// Negative scale — a flip is legal, and it must not cost the entity its bounds
// =============================================================================

TEST_CASE("EntityQuery: a NEGATIVE scale still yields a positive half-extent")
{
    // Found by eye as "the outline takes all", which was the mildest symptom. A negative Scale made
    // Size * Scale negative, so HalfExtent went negative — and Contains compares
    // fabs(point - centre) <= HalfExtent, which is FALSE for every point against a negative bound.
    // The entity silently stopped being clickable, marquee-selectable and focusable.
    World lWorld("Flipped");

    Entity lQuad = MakeQuad(lWorld, "Flipped", { 10.f, 20.f }, { 80.f, 40.f });
    lQuad.Get<TransformComponent>().Scale = { -1.f, 1.f };

    Bounds2D lBounds;
    REQUIRE(EntityQuery::TryGetBounds(lQuad, lBounds));

    CHECK(lBounds.HalfExtent.x == doctest::Approx(40.f));
    CHECK(lBounds.HalfExtent.y == doctest::Approx(20.f));

    // The box covers the same ground as the unflipped one — a mirror about the centre is the same
    // rectangle.
    CHECK(lBounds.Contains({ 10.f, 20.f }));
    CHECK(lBounds.Contains({ -29.f, 20.f }));
    CHECK(lBounds.Contains({ 49.f, 20.f }));
    CHECK_FALSE(lBounds.Contains({ 51.f, 20.f }));
}

TEST_CASE("EntityQuery: a flipped entity is still PICKABLE")
{
    // The consequence that actually mattered, asserted through the verb an author uses.
    World lWorld("PickFlipped");

    Entity lQuad = MakeQuad(lWorld, "Flipped", { 0.f, 0.f }, { 100.f, 100.f });
    lQuad.Get<TransformComponent>().Scale = { -2.f, -2.f };

    // Scaled by 2 as well as flipped, so the box is 200 wide: a point at 80 is inside the scaled
    // box and outside the unscaled one, which pins that the MAGNITUDE is used rather than dropped.
    const Entity lHit = EntityQuery::PickAt(lWorld, { 80.f, 80.f });

    REQUIRE(lHit.IsValid());
    CHECK(lHit.GetHandle() == lQuad.GetHandle());
}

TEST_CASE("Bounds2D: FromCenterSize is unsigned, so a mirrored size is the same box")
{
    const Bounds2D lPositive = Bounds2D::FromCenterSize({ 0.f, 0.f }, { 10.f, 6.f });
    const Bounds2D lMirrored = Bounds2D::FromCenterSize({ 0.f, 0.f }, { -10.f, -6.f });

    CHECK(lMirrored.HalfExtent.x == doctest::Approx(lPositive.HalfExtent.x));
    CHECK(lMirrored.HalfExtent.y == doctest::Approx(lPositive.HalfExtent.y));

    // Rotated too — that overload does its own multiply and had the same hole.
    const Bounds2D lRotated = Bounds2D::FromCenterSizeRotated({ 0.f, 0.f }, { -10.f, -6.f }, 0.f);
    CHECK(lRotated.HalfExtent.x == doctest::Approx(5.f));
    CHECK(lRotated.HalfExtent.y == doctest::Approx(3.f));
}
