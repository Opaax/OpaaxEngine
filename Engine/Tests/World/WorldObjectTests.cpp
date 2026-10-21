// Suite: a World and its Level are objects — a world's name and identity are its object's,
// WorldManager owns it, its Level goes with it, and the engine registers both classes.
#include <doctest.h>

#include "Engine/Registries/AutoRegistration.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Registries/ModuleRegistrar.h"
#include "Object/ObjectClassRegistry.h"
#include "Object/ObjectPtr.h"
#include "World/Level.h"
#include "World/World.h"
#include "World/WorldManager.h"

using namespace Opaax;

TEST_CASE("World object: named by its object name; two worlds of one name are two objects")
{
    const World lFirst("Main");
    const World lSecond("Main", EWorldMode::Edit);

    CHECK(&lFirst.GetClass() == &World::StaticClass());
    CHECK(lFirst.GetName() == OPAAX_ID("Main"));
    CHECK(lSecond.GetName() == lFirst.GetName());

    // The handle is the identity (what the camera manager remembers a world by).
    CHECK(lFirst.GetHandle().IsValid());
    CHECK_FALSE(lFirst.GetHandle() == lSecond.GetHandle());
}

TEST_CASE("World object: WorldManager owns the worlds it makes; a weak pointer reads null once one is destroyed")
{
    WorldManager lWorlds;   // bare: no Level, no subsystems

    World* lWorld = lWorlds.CreateWorld(OPAAX_ID("Doomed"), EWorldMode::Play);
    REQUIRE(lWorld != nullptr);
    CHECK(lWorld->GetName() == OPAAX_ID("Doomed"));
    CHECK(lWorld->HasAnyFlags(EObjectFlags::Managed));

    const TWeakObjectPtr<World> lWeak(lWorld);
    REQUIRE(lWeak.Get() == lWorld);

    lWorlds.DestroyWorld(lWorld);
    CHECK(lWeak.Get() == nullptr);
}

TEST_CASE("World object: CreateWorld gives the world a Level object named like it, destroyed with it")
{
    EngineRegistries lRegistries;
    WorldManager     lWorlds(&lRegistries);
    REQUIRE(lWorlds.Startup());   // the engine's services: null ones in a test, enough for a Level

    World* lWorld = lWorlds.CreateWorld(OPAAX_ID("Main"), EWorldMode::Edit);
    REQUIRE(lWorld != nullptr);

    Level* lLevel = lWorld->GetLevel();
    REQUIRE(lLevel != nullptr);
    CHECK(&lLevel->GetClass() == &Level::StaticClass());
    CHECK(lLevel->GetName() == OPAAX_ID("Main"));

    const TWeakObjectPtr<Level> lWeak(lLevel);
    lWorlds.DestroyWorld(lWorld);
    CHECK(lWeak.Get() == nullptr);
}

TEST_CASE("World object: the engine registers World and Level under their names")
{
    EngineRegistries lRegistries;
    ModuleRegistrar  lRegistrar;
    lRegistrar.BindEngineRegistries(lRegistries);
    RunAutoRegistrations(lRegistrar);

    CHECK(lRegistries.ObjectClasses().FindByName(OPAAX_ID("World")) == &World::StaticClass());
    CHECK(lRegistries.ObjectClasses().FindByName(OPAAX_ID("Level")) == &Level::StaticClass());

    // A Level is only made with its world: no class default, nothing an editor offers.
    CHECK(Level::StaticClass().IsAbstract());
    CHECK_FALSE(World::StaticClass().IsAbstract());
}
