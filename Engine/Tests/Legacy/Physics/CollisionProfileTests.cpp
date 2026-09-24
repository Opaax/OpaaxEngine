// Suite: CollisionProfile 3-state response matrix -> filter bits.
//
// CollisionProfile is OPAAX_API (reached via the import lib). Its ctor reads a file;
// a missing path -> Failed state but with defaults RETAINED (channel WorldDynamic,
// every response Block). We construct against a bogus path and author the matrix in
// memory — no disk fixture needed. The expected-error log is silenced in Main.cpp.
//
// CollisionProfile is non-copyable + non-movable, so each test constructs it in place.
#include <doctest.h>

#include "Physics/Collision/CollisionProfile.h"
#include "Physics/Collision/CollisionChannel.h"

using namespace Opaax;

namespace
{
    constexpr const char* k_NoFile = "___no_such_collision_profile___.json";

    Uint64 AllChannelBits()
    {
        Uint64 lMask = 0;
        for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
        {
            lMask |= CategoryBit(static_cast<ECollisionChannel>(i));
        }
        return lMask;
    }
}

TEST_CASE("CollisionProfile: ComputeCategoryBits is the single channel bit")
{
    CollisionProfile lProfile(OpaaxString(k_NoFile), OPAAX_ID("UnitTestProfile"));

    // Default channel is WorldDynamic.
    CHECK(lProfile.ComputeCategoryBits() == CategoryBit(ECollisionChannel::WorldDynamic));

    lProfile.SetChannel(ECollisionChannel::WorldStatic);
    CHECK(lProfile.ComputeCategoryBits() == CategoryBit(ECollisionChannel::WorldStatic));
}

TEST_CASE("CollisionProfile: default (all Block) masks include every channel")
{
    CollisionProfile lProfile(OpaaxString(k_NoFile), OPAAX_ID("UnitTestProfile"));

    const Uint64 lAll = AllChannelBits();
    CHECK(lProfile.ComputeMaskBits()      == lAll);
    CHECK(lProfile.ComputeBlockMaskBits() == lAll);
}

TEST_CASE("CollisionProfile: Ignore clears the channel from both masks")
{
    CollisionProfile lProfile(OpaaxString(k_NoFile), OPAAX_ID("UnitTestProfile"));
    lProfile.SetResponse(ECollisionChannel::WorldStatic, ECollisionResponse::Ignore);

    const Uint64 lBit = CategoryBit(ECollisionChannel::WorldStatic);
    CHECK((lProfile.ComputeMaskBits()      & lBit) == 0u);
    CHECK((lProfile.ComputeBlockMaskBits() & lBit) == 0u);
    // untouched channels remain set
    CHECK((lProfile.ComputeMaskBits() & CategoryBit(ECollisionChannel::WorldDynamic)) != 0u);
}

TEST_CASE("CollisionProfile: Overlap is in the event mask but NOT the block mask")
{
    CollisionProfile lProfile(OpaaxString(k_NoFile), OPAAX_ID("UnitTestProfile"));
    lProfile.SetResponse(ECollisionChannel::WorldStatic, ECollisionResponse::Overlap);

    const Uint64 lBit = CategoryBit(ECollisionChannel::WorldStatic);
    CHECK((lProfile.ComputeMaskBits()      & lBit) != 0u); // interacts (events/presence)
    CHECK((lProfile.ComputeBlockMaskBits() & lBit) == 0u); // but is not a solid wall
}

TEST_CASE("CollisionProfile: Block is present in both masks")
{
    CollisionProfile lProfile(OpaaxString(k_NoFile), OPAAX_ID("UnitTestProfile"));
    lProfile.SetResponse(ECollisionChannel::Pawn, ECollisionResponse::Block);

    const Uint64 lBit = CategoryBit(ECollisionChannel::Pawn);
    CHECK((lProfile.ComputeMaskBits()      & lBit) != 0u);
    CHECK((lProfile.ComputeBlockMaskBits() & lBit) != 0u);
}

TEST_CASE("CollisionProfile: ToJson -> FromJson round-trips channel + responses")
{
    CollisionProfile lA(OpaaxString(k_NoFile), OPAAX_ID("A"));
    lA.SetChannel(ECollisionChannel::Pawn);
    lA.SetResponse(ECollisionChannel::WorldStatic,  ECollisionResponse::Block);
    lA.SetResponse(ECollisionChannel::WorldDynamic, ECollisionResponse::Overlap);
    lA.SetResponse(ECollisionChannel::Pawn,         ECollisionResponse::Ignore);

    const nlohmann::json lJson = lA.ToJson();

    CollisionProfile lB(OpaaxString(k_NoFile), OPAAX_ID("B"));
    lB.FromJson(lJson);

    CHECK(lB.GetChannel() == ECollisionChannel::Pawn);
    CHECK(lB.GetResponse(ECollisionChannel::WorldStatic)  == ECollisionResponse::Block);
    CHECK(lB.GetResponse(ECollisionChannel::WorldDynamic) == ECollisionResponse::Overlap);
    CHECK(lB.GetResponse(ECollisionChannel::Pawn)         == ECollisionResponse::Ignore);
    // a channel not written stays at the constructor default (Block)
    CHECK(lB.GetResponse(ECollisionChannel::Projectile)   == ECollisionResponse::Block);
}
