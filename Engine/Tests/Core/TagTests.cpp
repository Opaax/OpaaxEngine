// Suite: OpaaxTag (Core/Tag/OpaaxTag.h) — the hierarchical gameplay label.
//
// The whole hierarchy is derived from the dotted text (I14), so these cases are really testing ONE
// rule: a prefix names an ancestor only when it ends on a segment boundary. The case that
// discriminates a correct implementation from a naive StartsWith is "DamageOverTime" vs "Damage".
//
// The ctor's malformed-text branch ASSERTS (__debugbreak in a Debug build), so it is exercised
// through the constexpr predicate and the tolerant from_json path, never by constructing a bad tag.
#include <doctest.h>

#include "Core/Tag/OpaaxTag.h"
#include "Core/Tag/OpaaxTagJson.h"

using namespace Opaax;

// Constant-evaluated, so a regression here fails the BUILD rather than a run.
static_assert(OpaaxTag::IsValidTagText("Damage"));
static_assert(OpaaxTag::IsValidTagText("Damage.Fire.Burn"));
static_assert(!OpaaxTag::IsValidTagText(""));
static_assert(!OpaaxTag::IsValidTagText(".Damage"));
static_assert(!OpaaxTag::IsValidTagText("Damage."));
static_assert(!OpaaxTag::IsValidTagText("Damage..Fire"));
static_assert(!OpaaxTag::IsValidTagText("Damage Fire"));
static_assert(!OpaaxTag::IsValidTagText("."));

// UTF-8 written as ESCAPES, never as literal characters (L21, I7): a literal would be decoded by the
// build's own charset, which is part of what is under test. "D\u00E9g\u00E2ts" is "Degats" with
// accents; its continuation bytes are NEGATIVE as signed char, which is exactly what a naive
// `lChar <= ' '` control-byte scan refuses. \u65E5\u672C cannot survive a wrong encoding at all.
static_assert(OpaaxTag::IsValidTagText("D\u00E9g\u00E2ts.Feu"));
static_assert(OpaaxTag::IsValidTagText("\u65E5\u672C.Tag"));

TEST_CASE("OpaaxTag: a UTF-8 name is a tag like any other, and its hierarchy still works")
{
    const OpaaxTag lTag = OpaaxTag("D\u00E9g\u00E2ts.Feu");

    CHECK(lTag.IsValid());
    CHECK(lTag.MatchesTag(OpaaxTag("D\u00E9g\u00E2ts")));
    CHECK(lTag.GetLeafName() == "Feu");
    CHECK(lTag.GetView() == "D\u00E9g\u00E2ts.Feu");

    // 12 BYTES for 10 characters: the tag carries UTF-8 and never decodes it (I7).
    CHECK(lTag.GetView().GetLength() == 12u);
}

TEST_CASE("OpaaxTag: a default tag is invalid, and says so in every spelling")
{
    const OpaaxTag lTag;

    CHECK_FALSE(lTag.IsValid());
    CHECK(lTag.GetView().IsEmpty());            // NOT the pool's "None" — see the GetView note
    CHECK(lTag.ToString() == OpaaxString("None"));
    CHECK(lTag.GetParent() == OpaaxTag());
    CHECK(lTag.GetLeafName().IsEmpty());
}

TEST_CASE("OpaaxTag: empty text is the invalid tag, not an entry named ''")
{
    CHECK_FALSE(OpaaxTag("").IsValid());
    CHECK(OpaaxTag("") == OpaaxTag());
}

TEST_CASE("OpaaxTag: the same text is always the same tag, and comparison is the interned id")
{
    const OpaaxTag lFirst  = OpaaxTag("OpaaxTagTests.Identity");
    const OpaaxTag lSecond = OpaaxTag("OpaaxTagTests.Identity");

    CHECK(lFirst == lSecond);
    CHECK(lFirst.GetName() == lSecond.GetName());
    CHECK(lFirst != OpaaxTag("OpaaxTagTests.Other"));

    // Interning a repeat must not grow the pool — that is what makes a tag cheap to rebuild.
    const Uint32 lBefore = OpaaxStringID::PoolSize();
    const OpaaxTag lAgain = OpaaxTag("OpaaxTagTests.Identity");
    CHECK(OpaaxStringID::PoolSize() == lBefore);
    CHECK(lAgain == lFirst);
}

TEST_CASE("OpaaxTag: MatchesTag answers itself and every ancestor")
{
    const OpaaxTag lTag = OpaaxTag("Damage.Fire.Burn");

    CHECK(lTag.MatchesTag(OpaaxTag("Damage.Fire.Burn")));
    CHECK(lTag.MatchesTag(OpaaxTag("Damage.Fire")));
    CHECK(lTag.MatchesTag(OpaaxTag("Damage")));
}

TEST_CASE("OpaaxTag: MatchesTag is not a string prefix test")
{
    // THE case. Both of these start with the parent's bytes and neither is a descendant.
    CHECK_FALSE(OpaaxTag("DamageOverTime").MatchesTag(OpaaxTag("Damage")));
    CHECK_FALSE(OpaaxTag("Damage.Fireball").MatchesTag(OpaaxTag("Damage.Fire")));

    // Matching runs child -> ancestor only, and never on a middle segment.
    CHECK_FALSE(OpaaxTag("Damage").MatchesTag(OpaaxTag("Damage.Fire")));
    CHECK_FALSE(OpaaxTag("Damage.Fire.Burn").MatchesTag(OpaaxTag("Fire")));
    CHECK_FALSE(OpaaxTag("Damage.Fire.Burn").MatchesTag(OpaaxTag("Fire.Burn")));
}

TEST_CASE("OpaaxTag: the invalid tag is inert in BOTH directions")
{
    const OpaaxTag lNone;
    const OpaaxTag lTag = OpaaxTag("Damage.Fire");

    CHECK_FALSE(lTag.MatchesTag(lNone));
    CHECK_FALSE(lNone.MatchesTag(lTag));
    CHECK_FALSE(lNone.MatchesTag(lNone));    // even against itself: there is no tag to match

    // The trap this guards: an invalid id resolves to the pool text "None", so a text-only match
    // would report a tag literally named None.Thing as its descendant.
    CHECK_FALSE(OpaaxTag("None.Thing").MatchesTag(lNone));
    CHECK_FALSE(OpaaxTag("None").MatchesTag(lNone));
}

TEST_CASE("OpaaxTag: GetParent walks one segment at a time and stops at the root")
{
    const OpaaxTag lTag = OpaaxTag("Damage.Fire.Burn");

    CHECK(lTag.GetParent() == OpaaxTag("Damage.Fire"));
    CHECK(lTag.GetParent().GetParent() == OpaaxTag("Damage"));
    CHECK_FALSE(lTag.GetParent().GetParent().GetParent().IsValid());

    // Every parent it produces is one the child matches — the property the walk exists for.
    CHECK(lTag.MatchesTag(lTag.GetParent()));
    CHECK(lTag.MatchesTag(lTag.GetParent().GetParent()));
}

TEST_CASE("OpaaxTag: GetLeafName and GetView borrow the pool, they do not copy")
{
    const OpaaxTag lTag = OpaaxTag("Damage.Fire.Burn");

    CHECK(lTag.GetView() == "Damage.Fire.Burn");
    CHECK(lTag.GetLeafName() == "Burn");
    CHECK(OpaaxTag("Damage").GetLeafName() == "Damage");   // a root tag is its own leaf

    // The leaf views the tag's own bytes, and those bytes live as long as the process does.
    CHECK(lTag.GetLeafName().Data() == lTag.GetView().Data() + 12);
    CHECK(lTag.GetView().Data() == OpaaxTag("Damage.Fire.Burn").GetView().Data());
}

TEST_CASE("OpaaxTag: json is a plain string, and a malformed one reads back invalid")
{
    const OpaaxTag lTag = OpaaxTag("Damage.Fire.Burn");
    const nlohmann::json lJson = lTag;

    CHECK(lJson.is_string());
    CHECK(lJson.get<std::string>() == "Damage.Fire.Burn");
    CHECK(lJson.get<OpaaxTag>() == lTag);

    // An invalid tag writes "" and comes back invalid — never as a tag named "None".
    const nlohmann::json lNoneJson = OpaaxTag();
    CHECK(lNoneJson.get<std::string>().empty());
    CHECK_FALSE(lNoneJson.get<OpaaxTag>().IsValid());

    // Hand-edited text is untrusted: from_json refuses it quietly instead of asserting.
    CHECK_FALSE(nlohmann::json("Damage..Fire").get<OpaaxTag>().IsValid());
    CHECK_FALSE(nlohmann::json("Damage Fire").get<OpaaxTag>().IsValid());
}

TEST_CASE("OpaaxTag: it keys a map and formats in a log line")
{
    TUnorderedMap<OpaaxTag, int> lByTag;
    lByTag[OpaaxTag("OpaaxTagTests.Key")] = 7;

    CHECK(lByTag[OpaaxTag("OpaaxTagTests.Key")] == 7);
    CHECK(fmt::format("{}", OpaaxTag("Damage.Fire")) == "Damage.Fire");
    CHECK(fmt::format("{}", OpaaxTag()) == "None");
}
