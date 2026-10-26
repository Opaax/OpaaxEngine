// Suite: the resource drag payload (Editor/Resources/ResourceDragPayload.h): its bytes, and which drop
// targets accept it. Header-only, so the rule a typed asset field relies on is tested here.
#include <doctest.h>

#include "Editor/Resources/ResourceDragPayload.h"

using namespace Opaax;
using namespace Opaax::Editor;

TEST_CASE("ResourceDragPayload: type, kinds and path survive the bytes")
{
    const TDynArray<Uint8> lBytes = ResourceDragPayload::Encode(7u, { 42u, 41u, 40u }, OpaaxString("Data/Grunt.opaaxasset"));

    ResourceDragPayload::Decoded lDecoded;
    REQUIRE(ResourceDragPayload::Decode(lBytes.data(), lBytes.size(), lDecoded));

    CHECK(lDecoded.TypeId == 7u);
    CHECK(lDecoded.Kinds == TDynArray<Uint32>{ 42u, 41u, 40u });
    CHECK(lDecoded.AssetPath == OpaaxString("Data/Grunt.opaaxasset"));

    // No kinds: a plain resource file.
    const TDynArray<Uint8> lPlain = ResourceDragPayload::Encode(3u, {}, OpaaxString("Textures/Hero.png"));
    REQUIRE(ResourceDragPayload::Decode(lPlain.data(), lPlain.size(), lDecoded));
    CHECK(lDecoded.Kinds.empty());
    CHECK(lDecoded.AssetPath == OpaaxString("Textures/Hero.png"));

    // Too short to hold the header, or the kinds it announces: not a payload.
    CHECK_FALSE(ResourceDragPayload::Decode(lBytes.data(), 5u, lDecoded));
    CHECK_FALSE(ResourceDragPayload::Decode(lBytes.data(), 12u, lDecoded));
}

TEST_CASE("ResourceDragPayload: a typed target accepts its kind and the kinds below it; an untyped one takes any")
{
    // A file whose class is 42, child of 41, child of 40.
    ResourceDragPayload::Decoded lGrunt;
    lGrunt.TypeId = 7u;
    lGrunt.Kinds  = { 42u, 41u, 40u };

    // A plain resource field (kind 0): any file of the type.
    CHECK(ResourceDragPayload::Accepts(lGrunt, 7u, 0u));

    // A field for its class, or for one of its parents; not for another class.
    CHECK(ResourceDragPayload::Accepts(lGrunt, 7u, 42u));
    CHECK(ResourceDragPayload::Accepts(lGrunt, 7u, 40u));
    CHECK_FALSE(ResourceDragPayload::Accepts(lGrunt, 7u, 43u));

    // Another resource type, whatever the kind.
    CHECK_FALSE(ResourceDragPayload::Accepts(lGrunt, 8u, 0u));

    // A file whose class could not be read carries no kind: a typed field refuses it.
    ResourceDragPayload::Decoded lUnreadable;
    lUnreadable.TypeId = 7u;
    CHECK_FALSE(ResourceDragPayload::Accepts(lUnreadable, 7u, 42u));
}
