// Suite: the M5 TEXT layer — MapData <-> json, plus the Guid string form it rests on.
//
// The rule this suite exists to defend is WM2: an OpaaxStringID is an intern-table INDEX, so a
// map that wrote one as a number would mean something different on the next run. Every interned
// value must survive as its STRING, and the invalid id must survive as INVALID — not as the
// literal "None" that OpaaxStringID::ToString() answers for it.
//
// No file IO here (that is MapFileTests) and no World (that is MapSnapshotTests): this is the
// transformation alone.
#include <doctest.h>

#include "Core/GUID/Guid.h"
#include "World/Serialization/MapJson.h"

using namespace Opaax;

namespace
{
    ComponentData MakeComponent(const char* InName, nlohmann::json InPayload)
    {
        return ComponentData{ OpaaxStringID(InName), Move(InPayload) };
    }

    EntityData MakeEntity(Guid InId, const char* InName, MapId InOwnerMap)
    {
        EntityData lEntity;
        lEntity.Id       = InId;
        lEntity.Name     = OpaaxString(InName);
        lEntity.OwnerMap = InOwnerMap;
        return lEntity;
    }

    const EntityData* FindByGuid(const MapData& InData, const Guid& InId)
    {
        for (const EntityData& lEntity : InData.Entities)
        {
            if (lEntity.Id == InId) { return &lEntity; }
        }
        return nullptr;
    }
}

// =============================================================================
// Guid <-> text — the identity the whole format hangs on
// =============================================================================
TEST_CASE("Guid: ToString/FromString round-trip preserves both halves")
{
    const Guid lGuid{ 0x0123456789ABCDEFull, 0xFEDCBA9876543210ull };

    const OpaaxString lText = lGuid.ToString();
    CHECK(lText.GetLength() == 32);
    CHECK(lText == OpaaxString("0123456789abcdeffedcba9876543210"));

    Guid lParsed;
    REQUIRE(Guid::FromString(lText, lParsed));
    CHECK(lParsed == lGuid);
}

TEST_CASE("Guid: a random Guid survives the text round trip")
{
    // Randomised on purpose — a hand-picked constant cannot catch a nibble-order bug that only
    // shows up for particular bit patterns.
    for (int lIteration = 0; lIteration < 64; ++lIteration)
    {
        const Guid lGuid = Guid::New();

        Guid lParsed;
        REQUIRE(Guid::FromString(lGuid.ToString(), lParsed));
        CHECK(lParsed == lGuid);
    }
}

TEST_CASE("Guid: an invalid Guid has a text form too, and it round-trips as invalid")
{
    const Guid lInvalid;
    REQUIRE_FALSE(lInvalid.IsValid());

    // Total, not special-cased: 32 zeros rather than an empty string, so a reader never has to
    // branch on "this entity had no guid text".
    CHECK(lInvalid.ToString() == OpaaxString("00000000000000000000000000000000"));

    Guid lParsed = Guid::New();
    REQUIRE(Guid::FromString(lInvalid.ToString(), lParsed));
    CHECK_FALSE(lParsed.IsValid());
}

TEST_CASE("Guid: FromString rejects malformed text and LEAVES THE OUTPUT UNTOUCHED")
{
    const Guid lOriginal{ 0xAAAAAAAAAAAAAAAAull, 0xBBBBBBBBBBBBBBBBull };

    // A half-written Guid is a DIFFERENT identity, not a rejected one — and the caller would
    // have no way to tell. Each of these fails at a different point in the parse, including one
    // that fails on the very last character (after 31 good ones).
    const char* lBadInputs[] = {
        "",                                        // empty
        "0123",                                    // too short
        "0123456789abcdeffedcba98765432100",       // too long
        "0123456789abcdeffedcba987654321g",        // non-hex, last character
        "g123456789abcdeffedcba9876543210",        // non-hex, first character
        "0123456789abcdef-edcba9876543210",        // dashes are not our format
    };

    for (const char* lBad : lBadInputs)
    {
        Guid lTarget = lOriginal;
        CAPTURE(lBad);
        CHECK_FALSE(Guid::FromString(OpaaxString(lBad), lTarget));
        CHECK(lTarget == lOriginal);
    }
}

TEST_CASE("Guid: FromString accepts upper case")
{
    Guid lParsed;
    REQUIRE(Guid::FromString(OpaaxString("0123456789ABCDEFFEDCBA9876543210"), lParsed));
    CHECK(lParsed == Guid{ 0x0123456789ABCDEFull, 0xFEDCBA9876543210ull });
}

// =============================================================================
// The round trip
// =============================================================================
TEST_CASE("MapJson: round trip preserves guid, name, ownerMap and every component payload")
{
    const Guid  lHeroId  = Guid::New();
    const Guid  lCrateId = Guid::New();
    const MapId lMap     = MapId("Level01");

    MapData lSource;

    EntityData lHero = MakeEntity(lHeroId, "Hero", lMap);
    lHero.Components.push_back(MakeComponent("Stats", nlohmann::json{{"Health", 100}, {"Speed", 4.5}}));
    lHero.Components.push_back(MakeComponent("Dummy", nlohmann::json{{"Position", {12.0, -3.0}}}));
    lSource.Entities.push_back(Move(lHero));

    EntityData lCrate = MakeEntity(lCrateId, "Crate", lMap);
    lCrate.Components.push_back(MakeComponent("Dummy", nlohmann::json{{"Position", {0.0, 0.0}}}));
    lSource.Entities.push_back(Move(lCrate));

    MapData lParsed;
    REQUIRE(MapJson::Deserialize(MapJson::Serialize(lSource), lParsed));

    REQUIRE(lParsed.EntityCount() == 2);

    const EntityData* lParsedHero = FindByGuid(lParsed, lHeroId);
    REQUIRE(lParsedHero != nullptr);
    CHECK(lParsedHero->Name == OpaaxString("Hero"));
    CHECK(lParsedHero->OwnerMap == lMap);
    REQUIRE(lParsedHero->Components.size() == 2);

    // Components come back as an object, so look them up by name rather than by index — the
    // order is the json object's, not the source array's, and nothing depends on it.
    bool lFoundStats = false;
    for (const ComponentData& lComponent : lParsedHero->Components)
    {
        if (lComponent.TypeName == OpaaxStringID("Stats"))
        {
            lFoundStats = true;
            CHECK(lComponent.Payload.at("Health").get<int>() == 100);
            CHECK(lComponent.Payload.at("Speed").get<double>() == doctest::Approx(4.5));
        }
    }
    CHECK(lFoundStats);

    const EntityData* lParsedCrate = FindByGuid(lParsed, lCrateId);
    REQUIRE(lParsedCrate != nullptr);
    CHECK(lParsedCrate->Name == OpaaxString("Crate"));
}

TEST_CASE("MapJson: an interned id is written as its STRING, never as its index (WM2)")
{
    MapData lData;
    EntityData lEntity = MakeEntity(Guid::New(), "Tagged", MapId("Level01"));
    lEntity.Components.push_back(MakeComponent("Stats", nlohmann::json::object()));
    lData.Entities.push_back(Move(lEntity));

    const nlohmann::json lJson = MapJson::ToJson(lData);

    // The point of the rule: an intern index is a number and would compare equal to a DIFFERENT
    // string on the next run. Assert the type, not just the value.
    const nlohmann::json& lEntityJson = lJson.at(MapJson::KEY_ENTITIES).at(0);
    CHECK(lEntityJson.at(MapJson::KEY_OWNER_MAP).is_string());
    CHECK(lEntityJson.at(MapJson::KEY_OWNER_MAP).get<std::string>() == "Level01");
    CHECK(lEntityJson.at(MapJson::KEY_COMPONENTS).contains("Stats"));
}

TEST_CASE("OpaaxStringID: the reserved \"None\" text IS the invalid id")
{
    // Pinned because MapJson's encoding of an invalid MapId depends on it. The pool reserves
    // index 0 for "None" (OpaaxStringIDPool's ctor), so the text and the invalid id are the same
    // value — which is why a map that DID write "None" would still read back as invalid, and why
    // no map can ever legitimately be named "None".
    CHECK_FALSE(OpaaxStringID("None").IsValid());
    CHECK(OpaaxStringID("None") == OpaaxStringID());
    CHECK_FALSE(OpaaxStringID("").IsValid());
}

TEST_CASE("MapJson: an invalid OwnerMap is written as \"\", not as \"None\"")
{
    // WM2's default (invalid OwnerMap) means RUNTIME-SPAWNED. ToString() answers "None" for an
    // invalid id, and writing that unguarded would still round-trip correctly — see the test
    // above — but the file would claim a bullet belongs to a map called "None". A map file is
    // read by humans and diffed in git, so "" is the encoding that does not lie.
    MapData lData;
    lData.Entities.push_back(MakeEntity(Guid::New(), "Bullet", MapId{}));

    const nlohmann::json lJson = MapJson::ToJson(lData);
    CHECK(lJson.at(MapJson::KEY_ENTITIES).at(0).at(MapJson::KEY_OWNER_MAP).get<std::string>().empty());

    MapData lParsed;
    REQUIRE(MapJson::FromJson(lJson, lParsed));
    REQUIRE(lParsed.EntityCount() == 1);

    // Still runtime-spawned on the way back — which is what keeps filtered capture (the "save
    // this map" path) from ever picking a bullet up.
    CHECK_FALSE(lParsed.Entities[0].OwnerMap.IsValid());
}

TEST_CASE("MapJson: entities are written sorted by Guid, whatever order they were captured in")
{
    // Not cosmetic. A map file lives in git and the editor's dirty check compares serialized
    // text, so an order-dependent write would reshuffle the file on every destroy and report a
    // world nobody edited as dirty.
    const Guid lLow { 0x0000000000000001ull, 0x0000000000000000ull };
    const Guid lMid { 0x0000000000000001ull, 0x0000000000000002ull };
    const Guid lHigh{ 0x0000000000000002ull, 0x0000000000000000ull };

    MapData lData;
    lData.Entities.push_back(MakeEntity(lHigh, "Third",  MapId("M")));
    lData.Entities.push_back(MakeEntity(lLow,  "First",  MapId("M")));
    lData.Entities.push_back(MakeEntity(lMid,  "Second", MapId("M")));

    const nlohmann::json lEntities = MapJson::ToJson(lData).at(MapJson::KEY_ENTITIES);
    REQUIRE(lEntities.size() == 3);
    CHECK(lEntities.at(0).at(MapJson::KEY_NAME).get<std::string>() == "First");
    CHECK(lEntities.at(1).at(MapJson::KEY_NAME).get<std::string>() == "Second");
    CHECK(lEntities.at(2).at(MapJson::KEY_NAME).get<std::string>() == "Third");

    // The same MapData in a different capture order must produce byte-identical text — that is
    // the property the dirty check actually depends on.
    MapData lShuffled;
    lShuffled.Entities.push_back(MakeEntity(lMid,  "Second", MapId("M")));
    lShuffled.Entities.push_back(MakeEntity(lHigh, "Third",  MapId("M")));
    lShuffled.Entities.push_back(MakeEntity(lLow,  "First",  MapId("M")));

    CHECK(MapJson::Serialize(lData) == MapJson::Serialize(lShuffled));
}

TEST_CASE("MapJson: ToJson does not reorder the MapData it was given")
{
    // Capture is read-only everywhere else in the snapshot core; a serializer that sorted its
    // input in place would be a surprise to the next caller (the editor captures, serializes for
    // the dirty check, and may then reuse the same MapData).
    MapData lData;
    lData.Entities.push_back(MakeEntity(Guid{ 0x9ull, 0x0ull }, "Nine", MapId("M")));
    lData.Entities.push_back(MakeEntity(Guid{ 0x1ull, 0x0ull }, "One",  MapId("M")));

    (void)MapJson::ToJson(lData);

    CHECK(lData.Entities[0].Name == OpaaxString("Nine"));
    CHECK(lData.Entities[1].Name == OpaaxString("One"));
}

// =============================================================================
// Tolerance — a map file is text, and a human will edit it
// =============================================================================
TEST_CASE("MapJson: a version NEWER than this build is refused, not half-read")
{
    nlohmann::json lFuture{
        { MapJson::KEY_VERSION,  MapJson::MAP_FORMAT_VERSION + 1 },
        { MapJson::KEY_ENTITIES, nlohmann::json::array() }
    };

    MapData lParsed;
    lParsed.Entities.push_back(MakeEntity(Guid::New(), "Existing", MapId("M")));

    CHECK_FALSE(MapJson::FromJson(lFuture, lParsed));

    // Refusing must not clobber what the caller already held — the next Save would otherwise
    // write an emptied world over the file it failed to read.
    CHECK(lParsed.EntityCount() == 1);
}

TEST_CASE("MapJson: the CURRENT version is accepted")
{
    const nlohmann::json lCurrent{
        { MapJson::KEY_VERSION,  MapJson::MAP_FORMAT_VERSION },
        { MapJson::KEY_ENTITIES, nlohmann::json::array() }
    };

    MapData lParsed;
    CHECK(MapJson::FromJson(lCurrent, lParsed));
    CHECK(lParsed.IsEmpty());
}

TEST_CASE("MapJson: malformed text returns false and never throws")
{
    MapData lParsed;

    CHECK_FALSE(MapJson::Deserialize(OpaaxString("{ this is not json"), lParsed));
    CHECK_FALSE(MapJson::Deserialize(OpaaxString(""), lParsed));

    // Valid json, wrong shape.
    CHECK_FALSE(MapJson::Deserialize(OpaaxString("[1, 2, 3]"), lParsed));
    CHECK_FALSE(MapJson::Deserialize(OpaaxString("\"a string\""), lParsed));
}

TEST_CASE("MapJson: a wrongly-typed field is tolerated rather than fatal")
{
    // Every read is guarded because nlohmann throws on a type mismatch, and one hand-edited
    // field must not take down the whole load.
    const Guid lId = Guid::New();

    nlohmann::json lJson{
        { MapJson::KEY_VERSION, MapJson::MAP_FORMAT_VERSION },
        { MapJson::KEY_ENTITIES, nlohmann::json::array({
            nlohmann::json{
                { MapJson::KEY_GUID,       lId.ToString().CStr() },
                { MapJson::KEY_NAME,       42 },                        // number, not a string
                { MapJson::KEY_OWNER_MAP,  nlohmann::json::array() },   // array, not a string
                { MapJson::KEY_COMPONENTS, "not an object" }
            }
        })}
    };

    MapData lParsed;
    REQUIRE(MapJson::FromJson(lJson, lParsed));
    REQUIRE(lParsed.EntityCount() == 1);

    CHECK(lParsed.Entities[0].Id == lId);          // the one field that was well-formed survived
    CHECK(lParsed.Entities[0].Name.IsEmpty());     // the rest fell back to their defaults
    CHECK_FALSE(lParsed.Entities[0].OwnerMap.IsValid());
    CHECK(lParsed.Entities[0].Components.empty());
}

TEST_CASE("MapJson: an entity with no usable guid is SKIPPED, never given a fresh identity")
{
    // Minting one would silently retarget every reference that pointed at this entity (WM3).
    const Guid lGood = Guid::New();

    const nlohmann::json lJson{
        { MapJson::KEY_VERSION, MapJson::MAP_FORMAT_VERSION },
        { MapJson::KEY_ENTITIES, nlohmann::json::array({
            nlohmann::json{ { MapJson::KEY_NAME, "NoGuid" } },
            nlohmann::json{ { MapJson::KEY_GUID, "nonsense" }, { MapJson::KEY_NAME, "BadGuid" } },
            nlohmann::json{ { MapJson::KEY_GUID, lGood.ToString().CStr() }, { MapJson::KEY_NAME, "Good" } }
        })}
    };

    MapData lParsed;
    REQUIRE(MapJson::FromJson(lJson, lParsed));

    REQUIRE(lParsed.EntityCount() == 1);
    CHECK(lParsed.Entities[0].Id == lGood);
    CHECK(lParsed.Entities[0].Name == OpaaxString("Good"));
}

TEST_CASE("MapJson: unknown fields are ignored, so a newer build's map still opens")
{
    const Guid lId = Guid::New();

    const nlohmann::json lJson{
        { MapJson::KEY_VERSION, MapJson::MAP_FORMAT_VERSION },
        { "somethingAddedLater", true },
        { MapJson::KEY_ENTITIES, nlohmann::json::array({
            nlohmann::json{
                { MapJson::KEY_GUID, lId.ToString().CStr() },
                { MapJson::KEY_NAME, "Forward" },
                { "perEntityFutureField", 7 }
            }
        })}
    };

    MapData lParsed;
    REQUIRE(MapJson::FromJson(lJson, lParsed));
    REQUIRE(lParsed.EntityCount() == 1);
    CHECK(lParsed.Entities[0].Name == OpaaxString("Forward"));
}

TEST_CASE("MapJson: a map with no entities array parses to an EMPTY map, not a failure")
{
    // Saving a cleared world must produce a file that opens back to a cleared world.
    const nlohmann::json lJson{ { MapJson::KEY_VERSION, MapJson::MAP_FORMAT_VERSION } };

    MapData lParsed;
    lParsed.Entities.push_back(MakeEntity(Guid::New(), "Stale", MapId("M")));

    REQUIRE(MapJson::FromJson(lJson, lParsed));
    CHECK(lParsed.IsEmpty());
}

TEST_CASE("MapJson: an empty map round-trips")
{
    const MapData lEmpty;

    MapData lParsed;
    REQUIRE(MapJson::Deserialize(MapJson::Serialize(lEmpty), lParsed));
    CHECK(lParsed.IsEmpty());
}
