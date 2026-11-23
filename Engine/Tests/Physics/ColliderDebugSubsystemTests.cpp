// Suite: ColliderDebugSubsystem — which worlds get collider outlines, and that a paused world keeps
// them.
#include <doctest.h>

#include "Application/Services/IPaths.h"
#include "Core/Build/BuildInfo.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Input/InputManager.h"
#include "Physics/ColliderDebugSubsystem.h"
#include "Physics/Components/ColliderComponent.h"
#include "Renderer/DebugDraw.h"
#include "Resources/ResourceManager.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

namespace
{
    // The subsystem added the way CreateSubsystemsFor would, whatever the build (its ShouldCreate
    // is not asked here), with a debug queue that records in any build.
    struct OutlineFixture
    {
        ResourceManager  Resources;
        EngineEventBus   Events;
        DebugDraw        Debug{ true };
        EngineConfigData Config;
        InputManager     Input;
        WorldManager     Worlds;

        World*                  TheWorld = nullptr;
        ColliderDebugSubsystem* Outlines = nullptr;

        OutlineFixture()
        {
            TheWorld = Worlds.CreateWorld("Outlines", EWorldMode::Play);
            REQUIRE(TheWorld != nullptr);

            TheWorld->SetContext(WorldContext{ *TheWorld, Resources, IPaths::Null(), Events,
                                               Input, Config, /*Actions*/ nullptr, /*UI*/ nullptr, Debug });
            TheWorld->GetSubsystems().Add<ColliderDebugSubsystem>();
            TheWorld->GetSubsystems().StartupAll();

            Outlines = TheWorld->GetSubsystems().GetSubsystem<ColliderDebugSubsystem>();
            REQUIRE(Outlines != nullptr);
            REQUIRE(Worlds.SetActiveWorld(TheWorld));

            Entity lBox = TheWorld->CreateEntity("Box");
            ColliderComponent lCollider;
            lCollider.Shape = EColliderShape::Box;
            lCollider.Size  = { 32.f, 32.f };
            lBox.AddOrReplace<ColliderComponent>(lCollider);
        }

        /** One frame as Engine::Loop drives it; returns the shapes queued, before the renderer clears them. */
        Uint64 TickFrame()
        {
            Worlds.Update(1.0 / 60.0);
            Worlds.FixedUpdate(1.0 / 60.0);
            Worlds.Render(1.0);

            const Uint64 lShapes = static_cast<Uint64>(Debug.GetLines().size() + Debug.GetBoxes().size());
            Debug.Clear();
            return lShapes;
        }
    };
}

TEST_CASE("ColliderDebugSubsystem: a ship build's worlds get none, a dev build's get one")
{
    CHECK_FALSE(ColliderDebugSubsystem::ShouldCreateIn(false));
    CHECK(ColliderDebugSubsystem::ShouldCreateIn(true));
}

TEST_CASE("ColliderDebugSubsystem: Edit and Play worlds follow the build this was compiled as")
{
    World lPlay("Play", EWorldMode::Play);
    World lEdit("Edit", EWorldMode::Edit);

    // Asked on the class default object, as a world asks it.
    const WorldSubsystem* lDefault = Cast<WorldSubsystem>(ColliderDebugSubsystem::StaticClass().GetDefaultObject());
    REQUIRE(lDefault != nullptr);
    CHECK(lDefault->ShouldCreate(lPlay) == Build::IsDevBuild());
    CHECK(lDefault->ShouldCreate(lEdit) == Build::IsDevBuild());
}

TEST_CASE("ColliderDebugSubsystem: a paused world keeps its collider outlines")
{
    OutlineFixture lFixture;

    REQUIRE(lFixture.TickFrame() > 0u);
    REQUIRE(lFixture.Outlines->GetLastDrawnCount() == 1u);

    lFixture.Worlds.SetPaused(true);

    for (int lFrame = 0; lFrame < 3; ++lFrame)
    {
        CHECK(lFixture.TickFrame() > 0u);
        CHECK(lFixture.Outlines->GetLastDrawnCount() == 1u);
    }
}
