// Suite: ResourceFormatRegistry — "which resource type loads this file?", answered by extension.
//
// The point of the table is that the mapping is MANY-to-one: one loader claims .png AND .jpg AND
// .tga, so the editor keys its icon and its double-click by TYPE and never sees an extension. The
// cases below pin that, plus the two refusals that keep the answer unambiguous — a duplicate
// extension, and a registration after the seal.
//
// Probe types are defined HERE, in the test exe — the same position a game module is in.
#include <doctest.h>

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormatRegistry.h"

using namespace Opaax;

namespace
{
    // A resource type claiming SEVERAL extensions — the shape the whole design exists for, and
    // the one no live engine type exercises yet (textures land with the sprite slice).
    struct ProbeTextureResource
    {
        OPAAX_RESOURCE_FORMAT("Probe Texture", ".png", ".jpg", ".TGA")

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<ProbeTextureResource> Load(const char*, LoadContext&) { return ProbeTextureResource{}; }
        static ProbeTextureResource                Placeholder() { return ProbeTextureResource{}; }
    };

    // Single extension, written WITHOUT its dot — a registrant may be sloppy; the table may not.
    struct ProbeSoundResource
    {
        OPAAX_RESOURCE_FORMAT("Probe Sound", "wav")

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<ProbeSoundResource> Load(const char*, LoadContext&) { return ProbeSoundResource{}; }
        static ProbeSoundResource                Placeholder() { return ProbeSoundResource{}; }
    };

    // Structurally identical to ProbeSoundResource, and claims an extension it already owns.
    struct ProbeRivalResource
    {
        OPAAX_RESOURCE_FORMAT("Probe Rival", ".wav")

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<ProbeRivalResource> Load(const char*, LoadContext&) { return ProbeRivalResource{}; }
        static ProbeRivalResource                Placeholder() { return ProbeRivalResource{}; }
    };

    // Claims a FREE extension first and a taken one second — the all-or-nothing probe.
    struct ProbePartialResource
    {
        OPAAX_RESOURCE_FORMAT("Probe Partial", ".unique", ".wav")

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<ProbePartialResource> Load(const char*, LoadContext&) { return ProbePartialResource{}; }
        static ProbePartialResource                Placeholder() { return ProbePartialResource{}; }
    };
}

TEST_CASE("ResourceFormatRegistry: one type answers for EVERY extension it claims")
{
    ResourceFormatRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeTextureResource>(OPAAX_ID("ProbeTexture")));

    const ResourceFormatEntry* const lPng = lRegistry.FindByExtension(NormalizeExtension(".png"));
    const ResourceFormatEntry* const lJpg = lRegistry.FindByExtension(NormalizeExtension(".jpg"));
    const ResourceFormatEntry* const lTga = lRegistry.FindByExtension(NormalizeExtension(".tga"));

    REQUIRE(lPng != nullptr);
    CHECK(lJpg == lPng);   // same ENTRY, not merely an equal one — one loader, three spellings
    CHECK(lTga == lPng);

    CHECK(lPng->TypeId == ResourceTypeID::Get<ProbeTextureResource>());
    CHECK(lPng->Name == OPAAX_ID("ProbeTexture"));
    CHECK(OpaaxString(lPng->Format->Label) == OpaaxString("Probe Texture"));

    // Count is TYPES, not extensions: three spellings did not become three entries.
    CHECK(lRegistry.Count() == 1);
}

TEST_CASE("ResourceFormatRegistry: the type is also reachable by its id, which is what the editor keys on")
{
    ResourceFormatRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeTextureResource>(OPAAX_ID("ProbeTexture")));

    const ResourceFormatEntry* const lById = lRegistry.FindByTypeId(ResourceTypeID::Get<ProbeTextureResource>());
    REQUIRE(lById != nullptr);
    CHECK(lById == lRegistry.FindByExtension(NormalizeExtension(".png")));

    // A type that never registered has an id all the same — it simply is not in the table.
    CHECK(lRegistry.FindByTypeId(ResourceTypeID::Get<ProbeSoundResource>()) == nullptr);
}

TEST_CASE("ResourceFormatRegistry: case and the leading dot are normalized on BOTH sides")
{
    ResourceFormatRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeTextureResource>(OPAAX_ID("ProbeTexture")));  // declares ".TGA"
    REQUIRE(lRegistry.Register<ProbeSoundResource>(OPAAX_ID("ProbeSound")));      // declares "wav", no dot

    // Declared upper-case, asked lower-case.
    CHECK(lRegistry.FindByExtension(NormalizeExtension(".tga")) != nullptr);
    // Declared without a dot, asked with one — and the other way round.
    CHECK(lRegistry.FindByExtension(NormalizeExtension(".wav")) != nullptr);
    CHECK(lRegistry.FindByExtension(NormalizeExtension("wav")) != nullptr);
    // Asked in the case Windows would hand back from a directory listing.
    CHECK(lRegistry.FindByExtension(NormalizeExtension(".WAV")) != nullptr);
    CHECK(lRegistry.FindByExtension(NormalizeExtension(".PNG")) != nullptr);
}

TEST_CASE("ResourceFormatRegistry: an unregistered or invalid extension answers null, never a guess")
{
    ResourceFormatRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeSoundResource>(OPAAX_ID("ProbeSound")));

    CHECK(lRegistry.FindByExtension(NormalizeExtension(".mp3")) == nullptr);
    CHECK(lRegistry.FindByExtension(NormalizeExtension("")) == nullptr);
    CHECK(lRegistry.FindByExtension(OpaaxStringID()) == nullptr);
}

TEST_CASE("ResourceFormatRegistry: a duplicate extension is REFUSED and the first registration survives")
{
    ResourceFormatRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeSoundResource>(OPAAX_ID("ProbeSound")));

    // Two loaders for ".wav" would make "what opens this file?" depend on registration order.
    CHECK_FALSE(lRegistry.Register<ProbeRivalResource>(OPAAX_ID("ProbeRival")));

    CHECK(lRegistry.Count() == 1);
    CHECK(lRegistry.FindByTypeId(ResourceTypeID::Get<ProbeRivalResource>()) == nullptr);

    const ResourceFormatEntry* const lWav = lRegistry.FindByExtension(NormalizeExtension(".wav"));
    REQUIRE(lWav != nullptr);
    CHECK(lWav->TypeId == ResourceTypeID::Get<ProbeSoundResource>());   // the FIRST one still owns it
}

TEST_CASE("ResourceFormatRegistry: a refused registration claims NONE of its extensions")
{
    // The rival above claims only one extension. This is the case that matters: a type whose
    // SECOND extension collides must not have quietly claimed its first.
    ResourceFormatRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeSoundResource>(OPAAX_ID("ProbeSound")));

    CHECK_FALSE(lRegistry.Register<ProbePartialResource>(OPAAX_ID("ProbePartial")));
    CHECK(lRegistry.FindByExtension(NormalizeExtension(".unique")) == nullptr);
    CHECK(lRegistry.Count() == 1);
}

TEST_CASE("ResourceFormatRegistry: the same TYPE cannot register twice")
{
    ResourceFormatRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeSoundResource>(OPAAX_ID("ProbeSound")));
    CHECK_FALSE(lRegistry.Register<ProbeSoundResource>(OPAAX_ID("ProbeSoundAgain")));
    CHECK(lRegistry.Count() == 1);
}

TEST_CASE("ResourceFormatRegistry: an empty name is refused")
{
    ResourceFormatRegistry lRegistry;
    CHECK_FALSE(lRegistry.Register<ProbeSoundResource>(OpaaxStringID()));
    CHECK(lRegistry.Count() == 0);
}

TEST_CASE("ResourceFormatRegistry: registering after the seal is refused, not silently dropped")
{
    ResourceFormatRegistry lRegistry;
    REQUIRE(lRegistry.Register<ProbeSoundResource>(OPAAX_ID("ProbeSound")));

    lRegistry.Seal();
    CHECK(lRegistry.IsSealed());

    // A type accepted now would be missing from a session that already scanned its files.
    CHECK_FALSE(lRegistry.Register<ProbeTextureResource>(OPAAX_ID("ProbeTexture")));
    CHECK(lRegistry.Count() == 1);

    lRegistry.Seal();   // idempotent
    CHECK(lRegistry.Count() == 1);
}

TEST_CASE("ResourceFormatRegistry: structurally identical types get DISTINCT ids")
{
    // ProbeSoundResource and ProbeRivalResource have the same members and the same layout; if their
    // ids collided, one pool would reinterpret the other's payload and the table would answer for
    // the wrong loader.
    CHECK(ResourceTypeID::Get<ProbeSoundResource>() != ResourceTypeID::Get<ProbeRivalResource>());
}

TEST_CASE("NormalizeExtension: the ONE place an extension becomes comparable")
{
    // Same text, four spellings, one id — this is the whole reason the scanner and the registry
    // call the same function rather than each folding case its own way.
    const OpaaxStringID lExpected = NormalizeExtension(".png");

    CHECK(NormalizeExtension("png") == lExpected);
    CHECK(NormalizeExtension(".PNG") == lExpected);
    CHECK(NormalizeExtension("PNG") == lExpected);

    CHECK_FALSE(NormalizeExtension("").IsValid());
    CHECK(NormalizeExtension(".png") != NormalizeExtension(".jpg"));
}
