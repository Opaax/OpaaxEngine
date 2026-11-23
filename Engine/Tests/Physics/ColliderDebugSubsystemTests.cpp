// Suite: ColliderDebugSubsystem — which worlds get collider outlines.
#include <doctest.h>

#include "Core/Build/BuildInfo.h"
#include "Physics/ColliderDebugSubsystem.h"
#include "World/World.h"

using namespace Opaax;

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
