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
    CHECK(lResolution.Tied == 3u);   // all at the default priority: reported as ambiguous
    REQUIRE(lResolution.Entity != ENTITY_NONE);

    // The winner is one of them (which one depends on entt's storage order).
    Entity lWinner = Entity(lResolution.Entity, &lWorld);
    CHECK(lResolution.View.OrthoSize == doctest::Approx(lWinner.Get<CameraComponent>().OrthoSize));
}

TEST_CASE("Resolve: the highest priority frames the world, whatever the creation order")
{
    for (const bool bHighFirst : { true, false })
    {
        World lWorld("Priorities");

        const auto lMake = [&lWorld](const char* InName, const Int32 InPriority, const float InSize)
        {
            Entity lCamera = lWorld.CreateEntity(InName);
            lCamera.Add<CameraComponent>();
            lCamera.Get<CameraComponent>().Priority  = InPriority;
            lCamera.Get<CameraComponent>().OrthoSize = InSize;
            return lCamera;
        };

        Entity lHigh;
        if (bHighFirst) { lHigh = lMake("High", 5, 500.f); }
        lMake("Low", 0, 100.f);
        lMake("Middle", 1, 200.f);
        if (!bHighFirst) { lHigh = lMake("High", 5, 500.f); }

        const CameraResolution lResolution = CameraManager::Resolve(lWorld);
        CHECK(lResolution.Count == 3u);
        CHECK(lResolution.Tied == 1u);
        CHECK(lResolution.Entity == lHigh.GetHandle());
        CHECK(lResolution.View.OrthoSize == doctest::Approx(500.f));
    }
}

TEST_CASE("Resolve: raising a camera's priority switches to it; equal priorities are counted as tied")
{
    World lWorld("Switch");

    Entity lGameplay = lWorld.CreateEntity("Gameplay");
    lGameplay.Add<CameraComponent>().Priority = 1;
    Entity lOverview = lWorld.CreateEntity("Overview");
    lOverview.Add<CameraComponent>().Priority = 0;
    lOverview.Get<TransformComponent>().Position = Vector2F{ -100.f, 40.f };

    CHECK(CameraManager::Resolve(lWorld).Entity == lGameplay.GetHandle());

    lOverview.Get<CameraComponent>().Priority = 10;
    const CameraResolution lSwitched = CameraManager::Resolve(lWorld);
    CHECK(lSwitched.Entity == lOverview.GetHandle());
    CHECK(lSwitched.View.Position.x == doctest::Approx(-100.f));
    CHECK(lSwitched.View.Position.y == doctest::Approx(40.f));

    lGameplay.Get<CameraComponent>().Priority = 10;
    CHECK(CameraManager::Resolve(lWorld).Tied == 2u);
}

TEST_CASE("Camera: a camera saved before Priority loads at priority 0, and Priority round-trips")
{
    const CameraComponent lOld = nlohmann::json{ { "OrthoSize", 120.f } }.get<CameraComponent>();
    CHECK(lOld.OrthoSize == doctest::Approx(120.f));
    CHECK(lOld.Priority == 0);

    CameraComponent lCamera;
    lCamera.Priority = 7;
    CHECK(nlohmann::json(lCamera).get<CameraComponent>().Priority == 7);
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
