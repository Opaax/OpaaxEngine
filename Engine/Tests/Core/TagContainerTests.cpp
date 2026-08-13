// Suite: OpaaxTagContainer (Core/Tag/OpaaxTagContainer.h) — the set form.
//
// The container stores EXACTLY what was added and lets OpaaxTag::MatchesTag supply the hierarchy, so
// the cases worth pinning are the two that would break if it ever started expanding parents into
// storage: HasTag answers an ancestor that is NOT held, and Num/iteration still report only what the
// caller put in.
#include <doctest.h>

#include "Core/Tag/OpaaxTagContainer.h"
#include "Core/Tag/OpaaxTagJson.h"

using namespace Opaax;

TEST_CASE("OpaaxTagContainer: a fresh container is empty and answers nothing")
{
    const OpaaxTagContainer lTags;

    CHECK(lTags.IsEmpty());
    CHECK(lTags.Num() == 0u);
    CHECK_FALSE(lTags.HasTag(OpaaxTag("Damage")));
    CHECK_FALSE(lTags.HasTagExact(OpaaxTag("Damage")));
    CHECK(lTags.ToString().IsEmpty());
}

TEST_CASE("OpaaxTagContainer: Add refuses duplicates and the invalid tag")
{
    OpaaxTagContainer lTags;

    CHECK(lTags.AddTag(OpaaxTag("Damage.Fire")));
    CHECK_FALSE(lTags.AddTag(OpaaxTag("Damage.Fire")));   // already held
    CHECK_FALSE(lTags.AddTag(OpaaxTag()));                // nothing to add
    CHECK_FALSE(lTags.AddTag(OpaaxTag("")));

    CHECK(lTags.Num() == 1u);
}

TEST_CASE("OpaaxTagContainer: HasTag matches ancestors that are NOT stored")
{
    const OpaaxTagContainer lTags{"Damage.Fire.Burn", "Faction.Player"};

    // The point of deriving the hierarchy: neither of these was ever added.
    CHECK(lTags.HasTag(OpaaxTag("Damage")));
    CHECK(lTags.HasTag(OpaaxTag("Damage.Fire")));
    CHECK(lTags.HasTag(OpaaxTag("Faction")));

    // ...and the container still holds exactly the two the caller gave it.
    CHECK(lTags.Num() == 2u);
    CHECK_FALSE(lTags.HasTagExact(OpaaxTag("Damage")));
    CHECK(lTags.HasTagExact(OpaaxTag("Damage.Fire.Burn")));

    // Descendants are not implied — a Fire source is not a Burn source.
    CHECK_FALSE(lTags.HasTag(OpaaxTag("Damage.Fire.Burn.Stack")));
    CHECK_FALSE(lTags.HasTag(OpaaxTag("Faction.Player.Ally")));
}

TEST_CASE("OpaaxTagContainer: Remove is exact, so a parent never removes its children")
{
    OpaaxTagContainer lTags{"Damage.Fire", "Damage.Fire.Burn"};

    CHECK_FALSE(lTags.RemoveTag(OpaaxTag("Damage")));     // never held, never removed
    CHECK(lTags.RemoveTag(OpaaxTag("Damage.Fire")));
    CHECK(lTags.Num() == 1u);
    CHECK(lTags.HasTagExact(OpaaxTag("Damage.Fire.Burn")));

    // ...and the survivor still implies the parent that was just removed.
    CHECK(lTags.HasTag(OpaaxTag("Damage.Fire")));

    lTags.Clear();
    CHECK(lTags.IsEmpty());
}

TEST_CASE("OpaaxTagContainer: HasAny / HasAll, including the empty-query conventions")
{
    const OpaaxTagContainer lTags{"Damage.Fire.Burn", "Faction.Player"};

    CHECK(lTags.HasAny(OpaaxTagContainer{"Damage", "Nothing.Here"}));
    CHECK_FALSE(lTags.HasAny(OpaaxTagContainer{"Nothing.Here", "Faction.Enemy"}));

    CHECK(lTags.HasAll(OpaaxTagContainer{"Damage", "Faction"}));
    CHECK_FALSE(lTags.HasAll(OpaaxTagContainer{"Damage", "Faction.Enemy"}));

    // Asking for nothing: nothing satisfies HasAny, everything satisfies HasAll.
    const OpaaxTagContainer lEmpty;
    CHECK_FALSE(lTags.HasAny(lEmpty));
    CHECK(lTags.HasAll(lEmpty));
}

TEST_CASE("OpaaxTagContainer: Append merges without duplicating, and == ignores order")
{
    OpaaxTagContainer lTags{"Damage.Fire"};
    lTags.Append(OpaaxTagContainer{"Damage.Fire", "Faction.Player"});

    CHECK(lTags.Num() == 2u);
    CHECK(lTags == OpaaxTagContainer{"Faction.Player", "Damage.Fire"});
    CHECK(lTags != OpaaxTagContainer{"Damage.Fire"});
    CHECK(lTags != OpaaxTagContainer{"Faction.Player", "Damage"});
}

TEST_CASE("OpaaxTagContainer: iteration and ToString report registration order")
{
    const OpaaxTagContainer lTags{"Damage.Fire", "Faction.Player"};

    Uint32 lSeen = 0;
    for (const OpaaxTag lTag : lTags)
    {
        CHECK(lTag.IsValid());
        ++lSeen;
    }

    CHECK(lSeen == 2u);
    CHECK(lTags.ToString() == OpaaxString("Damage.Fire, Faction.Player"));
}

TEST_CASE("OpaaxTagContainer: json is an array of strings and round-trips")
{
    const OpaaxTagContainer lTags{"Damage.Fire.Burn", "Faction.Player"};
    const nlohmann::json    lJson = lTags;

    CHECK(lJson.is_array());
    CHECK(lJson.size() == 2u);
    CHECK(lJson[0].get<std::string>() == "Damage.Fire.Burn");
    CHECK(lJson.get<OpaaxTagContainer>() == lTags);

    // Malformed entries drop rather than poisoning the whole component.
    const nlohmann::json lDirty = nlohmann::json::array({"Damage.Fire", "Bad..Tag", ""});
    CHECK(lDirty.get<OpaaxTagContainer>() == OpaaxTagContainer{"Damage.Fire"});

    // A non-array (a hand-edit that wrote a bare string) leaves an empty container, not a crash.
    CHECK(nlohmann::json("Damage.Fire").get<OpaaxTagContainer>().IsEmpty());
}
