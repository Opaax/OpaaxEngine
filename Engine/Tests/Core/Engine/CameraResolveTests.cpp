// Suite: CameraManager::Resolve — which camera frames a world (①).
//
// Resolve is static and pure precisely so this suite can exist: it needs a World and nothing
// else, no engine to boot and no GL context. That is also the claim the whole placement rests
// on — a camera resolve has no per-world state, which is why CameraManager is one ENGINE
// subsystem instead of one instance per world.
//
// The POSITIVE branch is what this pins. A smoke log can show the "no camera" warning fire,
// but "the right camera won and these were its numbers" has nowhere to show up except here.
#include <doctest.h>

#include "Engine/Subsystems/Camera/CameraManager.h"
#include "World/Components/CameraComponent.h"
#include "World/Entity/Entity.h"
#include "World/World.h"

using namespace Opaax;

TEST_CASE("Resolve: a world with no camera answers the DEFAULT frame, not an empty one")
{
    World lWorld("Empty");

    const CameraResolution lResolution = CameraManager::Resolve(lWorld);

    CHECK(lResolution.Count == 0u);
    CHECK(lResolution.Entity == ENTITY_NONE);

    // The default view IS the fallback — the frame the engine drew before cameras existed.
    // If this ever answered a zeroed CameraView the projection would collapse and the viewport
    // would go black, which is the failure BO4c's rule exists to prevent.
    CHECK(lResolution.View.Position.x == doctest::Approx(0.f));
    CHECK(lResolution.View.Position.y == doctest::Approx(0.f));
    CHECK(lResolution.View.OrthoSize  == doctest::Approx(300.f));
}

TEST_CASE("Resolve: one camera answers ITS values and names its entity")
{
    World lWorld("Framed");

    Entity lCamera = lWorld.CreateEntity("MainCamera");
    lCamera.Add<CameraComponent>();
    lCamera.Get<CameraComponent>().Position  = Vector2F{ 250.f, -75.f };
    lCamera.Get<CameraComponent>().OrthoSize = 120.f;

    const CameraResolution lResolution = CameraManager::Resolve(lWorld);

    CHECK(lResolution.Count == 1u);
    CHECK(lResolution.Entity == lCamera.GetHandle());
    CHECK(lResolution.View.Position.x == doctest::Approx(250.f));
    CHECK(lResolution.View.Position.y == doctest::Approx(-75.f));
    CHECK(lResolution.View.OrthoSize  == doctest::Approx(120.f));
}

TEST_CASE("Resolve: entities WITHOUT a camera are not counted")
{
    World lWorld("Mixed");

    lWorld.CreateEntity("Quad");
    lWorld.CreateEntity("Crate");

    Entity lCamera = lWorld.CreateEntity("MainCamera");
    lCamera.Add<CameraComponent>();

    const CameraResolution lResolution = CameraManager::Resolve(lWorld);

    CHECK(lResolution.Count == 1u);
    CHECK(lResolution.Entity == lCamera.GetHandle());
}

TEST_CASE("Resolve: several cameras — one wins and the COUNT reports the rest")
{
    // The count is not decoration: it is the whole of the "priority is not built" contract.
    // Losing it would turn a world with three cameras into a silent coin flip.
    World lWorld("Crowded");

    for (int lIndex = 0; lIndex < 3; ++lIndex)
    {
        Entity lCamera = lWorld.CreateEntity("Camera");
        lCamera.Add<CameraComponent>();
        lCamera.Get<CameraComponent>().OrthoSize = 100.f * static_cast<float>(lIndex + 1);
    }

    const CameraResolution lResolution = CameraManager::Resolve(lWorld);

    CHECK(lResolution.Count == 3u);
    REQUIRE(lResolution.Entity != ENTITY_NONE);

    // Whichever one won, the view is ONE OF THEM and never a blend or a default. Asserting the
    // specific winner would pin entt's storage order, which is not a promise this rule makes.
    Entity lWinner = Entity(lResolution.Entity, &lWorld);
    CHECK(lResolution.View.OrthoSize == doctest::Approx(lWinner.Get<CameraComponent>().OrthoSize));
}

TEST_CASE("Resolve: a camera REMOVED falls back to the default rather than the last value")
{
    // The mid-play deletion case. Update writes the resolved view unconditionally, so this is
    // what stops a destroyed camera from freezing the frame on its final position.
    World lWorld("Transient");

    Entity lCamera = lWorld.CreateEntity("MainCamera");
    lCamera.Add<CameraComponent>();
    lCamera.Get<CameraComponent>().OrthoSize = 42.f;

    REQUIRE(CameraManager::Resolve(lWorld).View.OrthoSize == doctest::Approx(42.f));

    lWorld.DestroyEntity(lCamera);

    const CameraResolution lAfter = CameraManager::Resolve(lWorld);
    CHECK(lAfter.Count == 0u);
    CHECK(lAfter.View.OrthoSize == doctest::Approx(300.f));
}
