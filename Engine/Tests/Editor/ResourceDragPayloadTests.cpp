// Suite: the resource drag payload (Editor/Resources/ResourceDragPayload.h): its bytes, and which drop
// targets accept it. Header-only, so the rule a typed data asset field relies on is tested here.
#include <doctest.h>

#include "Editor/Resources/ResourceDragPayload.h"

using namespace Opaax;
using namespace Opaax::Editor;

TEST_CASE("ResourceDragPayload: type, sub-type and path survive the bytes")
{
    const TDynArray<Uint8> lBytes = ResourceDragPayload::Encode(7u, 42u, OpaaxString("Data/Grunt.opaaxdata"));

    ResourceDragPayload::Decoded lDecoded;
    REQUIRE(ResourceDragPayload::Decode(lBytes.data(), lBytes.size(), lDecoded));

    CHECK(lDecoded.TypeId == 7u);
    CHECK(lDecoded.SubTypeId == 42u);
    CHECK(lDecoded.AssetPath == OpaaxString("Data/Grunt.opaaxdata"));

    // Too short to hold the header: not a payload.
    CHECK_FALSE(ResourceDragPayload::Decode(lBytes.data(), 5u, lDecoded));
}

TEST_CASE("ResourceDragPayload: a typed target accepts only its sub-type; an untyped one takes any")
{
    ResourceDragPayload::Decoded lGrunt;
    lGrunt.TypeId    = 7u;
    lGrunt.SubTypeId = 42u;

    // A plain resource field (sub-type 0): any file of the type, as before.
    CHECK(ResourceDragPayload::Accepts(lGrunt, 7u, 0u));

    // A data asset field for this data type, and one for another.
    CHECK(ResourceDragPayload::Accepts(lGrunt, 7u, 42u));
    CHECK_FALSE(ResourceDragPayload::Accepts(lGrunt, 7u, 43u));

    // Another resource type, whatever the sub-type.
    CHECK_FALSE(ResourceDragPayload::Accepts(lGrunt, 8u, 0u));

    // A data asset whose type could not be read carries no sub-type: a typed field refuses it.
    ResourceDragPayload::Decoded lUnreadable;
    lUnreadable.TypeId = 7u;
    CHECK_FALSE(ResourceDragPayload::Accepts(lUnreadable, 7u, 42u));
}
