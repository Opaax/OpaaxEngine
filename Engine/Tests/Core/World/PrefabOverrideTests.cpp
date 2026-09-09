// Suite: ⑦-C P3 — PrefabOverrides, what one instance entity changed about its template.
//
// THE HEADLINE CASE is "the prefab moves a property the instance did not touch" — the one that
// separates per-PROPERTY overrides from per-COMPONENT ones. Under a per-component model an author
// who nudged a sprite's Size would stop receiving the prefab's later colour change, silently and
// forever; that case is written out below and is the reason this file exists.
//
// Pure json in, pure json out — no World, no registry, no resources.
#include <doctest.h>

#include "World/Prefab/PrefabOverrides.h"

using namespace Opaax;

namespace
{
    ComponentData Component(const char* InName, nlohmann::json InPayload)
    {
        return ComponentData{ OpaaxStringID(InName), Move(InPayload) };
    }

    EntityData Entity(const char* InName, TDynArray<ComponentData> InComponents)
    {
        EntityData lEntity;
        lEntity.Name       = OpaaxString(InName);
        lEntity.Components = Move(InComponents);
        return lEntity;
    }

    const nlohmann::json* Find(const EntityData& InEntity, const char* InName)
    {
        for (const ComponentData& lComponent : InEntity.Components)
        {
            if (lComponent.TypeName == OpaaxStringID(InName)) { return &lComponent.Payload; }
        }
        return nullptr;
    }

    // A sprite-shaped payload: a nested object, a scalar and a string, which is the shape every
    // real component has.
    nlohmann::json Sprite(double InSizeX, double InSizeY, double InRed, const char* InTexture)
    {
        return nlohmann::json{
            { "Size",    { { "x", InSizeX }, { "y", InSizeY } } },
            { "Color",   { { "r", InRed }, { "g", 1.0 }, { "b", 1.0 } } },
            { "Texture", InTexture },
        };
    }

    const OpaaxStringID k_NoIgnore{};
}

// =============================================================================
// THE HEADLINE CASE
// =============================================================================
TEST_CASE("PrefabOverrides: a prefab edit reaches a property the instance did NOT override")
{
    // The author nudged Size. The prefab later changes Color. Both must hold at once — the
    // instance keeps its Size and picks up the new Color.
    const EntityData lTemplate = Entity("Turret", { Component("Sprite", Sprite(100.0, 100.0, 1.0, "A.png")) });
    const EntityData lInstance = Entity("Turret", { Component("Sprite", Sprite(250.0, 100.0, 1.0, "A.png")) });

    const nlohmann::json lPatch = PrefabOverrides::Diff(lTemplate, lInstance, k_NoIgnore);

    // Only the touched property is recorded — NOT the whole Sprite.
    REQUIRE(lPatch.contains(PrefabOverrides::KEY_COMPONENTS));
    const nlohmann::json& lSpritePatch = lPatch[PrefabOverrides::KEY_COMPONENTS]["Sprite"];
    CHECK(lSpritePatch.contains("Size"));
    CHECK_FALSE(lSpritePatch.contains("Color"));
    CHECK_FALSE(lSpritePatch.contains("Texture"));

    // Now the PREFAB changes, in a property the instance never touched.
    EntityData lNewTemplate = Entity("Turret", { Component("Sprite", Sprite(100.0, 100.0, 0.25, "A.png")) });
    PrefabOverrides::Apply(lPatch, lNewTemplate);

    const nlohmann::json* lResult = Find(lNewTemplate, "Sprite");
    REQUIRE(lResult != nullptr);

    // The override held...
    CHECK((*lResult)["Size"]["x"].get<double>() == doctest::Approx(250.0));
    // ...AND the prefab's change arrived. A per-component override would report 1.0 here.
    CHECK((*lResult)["Color"]["r"].get<double>() == doctest::Approx(0.25));
}

// =============================================================================
// Diff / Apply round trip
// =============================================================================
TEST_CASE("PrefabOverrides: an untouched instance produces an EMPTY patch")
{
    const EntityData lTemplate = Entity("A", { Component("Sprite", Sprite(1.0, 2.0, 3.0, "T.png")) });
    const EntityData lInstance = lTemplate;

    const nlohmann::json lPatch = PrefabOverrides::Diff(lTemplate, lInstance, k_NoIgnore);

    // Callers rely on this: an unmodified instance entity writes no patch at all.
    CHECK(PrefabOverrides::IsEmpty(lPatch));
}

TEST_CASE("PrefabOverrides: Apply(Diff(t, i), t) reproduces i")
{
    const EntityData lTemplate = Entity("Base", {
        Component("Transform", nlohmann::json{ { "Position", { { "x", 0.0 }, { "y", 0.0 } } }, { "Rotation", 0.0 } }),
        Component("Sprite",    Sprite(100.0, 100.0, 1.0, "A.png")),
        Component("Dropped",   nlohmann::json{ { "v", 1 } }),
    });

    EntityData lInstance = Entity("Renamed", {
        Component("Transform", nlohmann::json{ { "Position", { { "x", 5.0 }, { "y", 9.0 } } }, { "Rotation", 0.0 } }),
        Component("Sprite",    Sprite(100.0, 100.0, 0.5, "B.png")),
        Component("Added",     nlohmann::json{ { "hp", 42 } }),
    });

    const nlohmann::json lPatch = PrefabOverrides::Diff(lTemplate, lInstance, k_NoIgnore);

    EntityData lRebuilt = lTemplate;
    PrefabOverrides::Apply(lPatch, lRebuilt);

    CHECK(lRebuilt.Name == lInstance.Name);
    REQUIRE(lRebuilt.Components.size() == lInstance.Components.size());

    for (const ComponentData& lWanted : lInstance.Components)
    {
        const nlohmann::json* lGot = Find(lRebuilt, lWanted.TypeName.CStr());
        REQUIRE(lGot != nullptr);
        CHECK(*lGot == lWanted.Payload);
    }

    CHECK(Find(lRebuilt, "Dropped") == nullptr);   // removed on the instance, and it stayed removed
}

// =============================================================================
// The four component cases, one at a time
// =============================================================================
TEST_CASE("PrefabOverrides: a component ADDED on the instance is carried whole")
{
    const EntityData lTemplate = Entity("A", {});
    const EntityData lInstance = Entity("A", { Component("Health", nlohmann::json{ { "hp", 7 } }) });

    const nlohmann::json lPatch = PrefabOverrides::Diff(lTemplate, lInstance, k_NoIgnore);
    CHECK(lPatch[PrefabOverrides::KEY_COMPONENTS]["Health"]["hp"].get<int>() == 7);

    EntityData lRebuilt = lTemplate;
    PrefabOverrides::Apply(lPatch, lRebuilt);
    REQUIRE(Find(lRebuilt, "Health") != nullptr);
}

TEST_CASE("PrefabOverrides: a component REMOVED on the instance is recorded as null")
{
    const EntityData lTemplate = Entity("A", { Component("Health", nlohmann::json{ { "hp", 7 } }) });
    const EntityData lInstance = Entity("A", {});

    const nlohmann::json lPatch = PrefabOverrides::Diff(lTemplate, lInstance, k_NoIgnore);
    CHECK(lPatch[PrefabOverrides::KEY_COMPONENTS]["Health"].is_null());

    EntityData lRebuilt = lTemplate;
    PrefabOverrides::Apply(lPatch, lRebuilt);
    CHECK(Find(lRebuilt, "Health") == nullptr);
}

TEST_CASE("PrefabOverrides: the NAME rides beside the components, and only when it changed")
{
    const EntityData lTemplate = Entity("Turret", {});

    CHECK_FALSE(PrefabOverrides::Diff(lTemplate, Entity("Turret", {}), k_NoIgnore)
                    .contains(PrefabOverrides::KEY_NAME));

    const nlohmann::json lPatch = PrefabOverrides::Diff(lTemplate, Entity("Left Turret", {}), k_NoIgnore);
    REQUIRE(lPatch.contains(PrefabOverrides::KEY_NAME));

    EntityData lRebuilt = lTemplate;
    PrefabOverrides::Apply(lPatch, lRebuilt);
    CHECK(lRebuilt.Name == OpaaxString("Left Turret"));
}

TEST_CASE("PrefabOverrides: the IGNORED component never appears, either way round")
{
    // The marker is identity: the template never has one, so without the exclusion it would show
    // up as an "added component" on every single instance entity.
    const EntityData lTemplate = Entity("A", { Component("Sprite", Sprite(1.0, 1.0, 1.0, "T.png")) });
    const EntityData lInstance = Entity("A", {
        Component("Sprite",         Sprite(1.0, 1.0, 1.0, "T.png")),
        Component("PrefabInstance", nlohmann::json{ { "InstanceId", "abc" } }),
    });

    const nlohmann::json lPatch =
        PrefabOverrides::Diff(lTemplate, lInstance, OpaaxStringID("PrefabInstance"));

    CHECK(PrefabOverrides::IsEmpty(lPatch));
}

// =============================================================================
// The documented limits — pinned so they are a decision, not a surprise
// =============================================================================
TEST_CASE("PrefabOverrides: a nested object diffs per member, not wholesale")
{
    const EntityData lTemplate = Entity("A", { Component("T", nlohmann::json{
        { "Position", { { "x", 0.0 }, { "y", 0.0 } } } }) });
    const EntityData lInstance = Entity("A", { Component("T", nlohmann::json{
        { "Position", { { "x", 8.0 }, { "y", 0.0 } } } }) });

    const nlohmann::json lPatch = PrefabOverrides::Diff(lTemplate, lInstance, k_NoIgnore);
    const nlohmann::json& lPos  = lPatch[PrefabOverrides::KEY_COMPONENTS]["T"]["Position"];

    // Only x. A wholesale replace would carry y too, and the prefab could never move y again.
    CHECK(lPos.contains("x"));
    CHECK_FALSE(lPos.contains("y"));
}

TEST_CASE("PrefabOverrides: an ARRAY is replaced wholesale — the documented merge-patch limit")
{
    const EntityData lTemplate = Entity("A", { Component("T", nlohmann::json{ { "Frames", { 1, 2, 3 } } }) });
    const EntityData lInstance = Entity("A", { Component("T", nlohmann::json{ { "Frames", { 1, 9, 3 } } }) });

    const nlohmann::json lPatch = PrefabOverrides::Diff(lTemplate, lInstance, k_NoIgnore);

    // Not "element 1 changed" — the whole array. Asserted so the limit is a decision on record;
    // the trigger to revisit is the first component with a meaningfully editable array in it.
    CHECK(lPatch[PrefabOverrides::KEY_COMPONENTS]["T"]["Frames"] == nlohmann::json({ 1, 9, 3 }));

    EntityData lRebuilt = lTemplate;
    PrefabOverrides::Apply(lPatch, lRebuilt);
    CHECK((*Find(lRebuilt, "T"))["Frames"] == nlohmann::json({ 1, 9, 3 }));
}

TEST_CASE("PrefabOverrides: Apply survives a malformed patch instead of throwing")
{
    // This runs at map load (**MP3**), where a hand-edited file is an ordinary input.
    EntityData lEntity = Entity("A", { Component("Sprite", Sprite(1.0, 1.0, 1.0, "T.png")) });
    const EntityData lUntouched = lEntity;

    PrefabOverrides::Apply(nlohmann::json("not an object"), lEntity);
    PrefabOverrides::Apply(nlohmann::json::array({ 1, 2 }), lEntity);

    CHECK(lEntity.Name == lUntouched.Name);
    CHECK(lEntity.Components.size() == lUntouched.Components.size());
}
