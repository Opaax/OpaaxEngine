// Suite: CameraManager::Resolve — which camera frames a world. Resolve is static and pure,
// so it only needs a World.
#include <doctest.h>

#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/CameraComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/World.h"

using namespace Opaax;

TEST_CASE("Resolve: a world with no camera answers the DEFAULT frame, not an empty one")
{
    World lWorld("Empty");

    const CameraResolution lResolution = CameraManager::Resolve(lWorld);

    CHECK(lResolution.Count == 0u);
    CHECK(lResolution.Entity == ENTITY_NONE);

    // The default view is the fallback; a zeroed CameraView would give a black viewport.
    CHECK(lResolution.View.Position.x == doctest::Approx(0.f));
    CHECK(lResolution.View.Position.y == doctest::Approx(0.f));
    CHECK(lResolution.View.OrthoSize  == doctest::Approx(300.f));
}

TEST_CASE("Resolve: one camera answers ITS values and names its entity")
{
    World lWorld("Framed");

    Entity lCamera = lWorld.CreateEntity("MainCamera");
    lCamera.Add<CameraComponent>();
    lCamera.Get<TransformComponent>().Position = Vector2F{ 250.f, -75.f };
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
    // The count matters: it is how several cameras get reported.
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

    // The winner is one of them (which one depends on entt's storage order).
    Entity lWinner = Entity(lResolution.Entity, &lWorld);
    CHECK(lResolution.View.OrthoSize == doctest::Approx(lWinner.Get<CameraComponent>().OrthoSize));
}

TEST_CASE("Resolve: a camera REMOVED falls back to the default rather than the last value")
{
    // Update always writes the resolved view, so a deleted camera does not freeze the frame.
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
