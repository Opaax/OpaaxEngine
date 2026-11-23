// Suite: TagRegistry — the tags a project uses: declared in code, listed in Tags.config, and their parents.
#include <doctest.h>

#include "Engine/Tags/Config_Tags.h"
#include "Engine/Tags/TagRegistry.h"

using namespace Opaax;

namespace
{
    OpaaxString Names(const TDynArray<TagRegistry::Entry>& InEntries)
    {
        OpaaxString lNames;
        for (const TagRegistry::Entry& lEntry : InEntries)
        {
            lNames += lNames.IsEmpty() ? lEntry.Tag.ToString() : "," + lEntry.Tag.ToString();
        }
        return lNames;
    }
}

TEST_CASE("TagRegistry: a registered tag brings its parents, every tag sorted by name")
{
    TagRegistry lRegistry;
    lRegistry.AddNative(OpaaxTag("UI.Layer.Menu"), OpaaxString("Menus"));
    lRegistry.AddNative(OpaaxTag("Audio.Bus.Music"), OpaaxString());

    CHECK(Names(lRegistry.GetAll()) == "Audio,Audio.Bus,Audio.Bus.Music,UI,UI.Layer,UI.Layer.Menu");
    CHECK(lRegistry.IsRegistered(OpaaxTag("UI.Layer")));
    CHECK(lRegistry.Find(OpaaxTag("UI.Layer"))->Source == ETagSource::Implied);
    CHECK(lRegistry.Find(OpaaxTag("UI.Layer.Menu"))->Comment == "Menus");
    CHECK_FALSE(lRegistry.IsRegistered(OpaaxTag("UI.Layer.Popup")));
    CHECK_FALSE(lRegistry.IsRegistered(OpaaxTag()));
}

TEST_CASE("TagRegistry: Under lists a root's descendants, not the root nor a name that only starts like it")
{
    TagRegistry lRegistry;
    lRegistry.AddNative(OpaaxTag("UI.Layer.Menu.Pause"), OpaaxString());
    lRegistry.AddNative(OpaaxTag("UI.Layer.Hud"), OpaaxString());
    lRegistry.AddNative(OpaaxTag("UI.LayerExtra"), OpaaxString());

    CHECK(Names(lRegistry.Under(OpaaxTag("UI.Layer"))) == "UI.Layer.Hud,UI.Layer.Menu,UI.Layer.Menu.Pause");
    CHECK(lRegistry.Under(OpaaxTag("Audio")).empty());
    CHECK(lRegistry.Under(OpaaxTag()).size() == lRegistry.GetAll().size());
}

TEST_CASE("TagRegistry: the config's tags are replaced as a whole; code's stay, and stay native")
{
    TagRegistry lRegistry;
    lRegistry.AddNative(OpaaxTag("UI.Layer.Menu"), OpaaxString());
    lRegistry.Seal();

    const Uint64 lBefore = lRegistry.GetRevision();
    lRegistry.SetConfigTags({ TagDefinition{ OpaaxTag("UI.Layer.Popup"), OpaaxString("Pop-ups") },
                              TagDefinition{ OpaaxTag("UI.Layer.Menu"), OpaaxString("Listed again") },
                              TagDefinition{ OpaaxTag(), OpaaxString("no tag: left out") } });
    CHECK(lRegistry.GetRevision() != lBefore);

    CHECK(lRegistry.Find(OpaaxTag("UI.Layer.Popup"))->Source == ETagSource::Config);
    CHECK(lRegistry.Find(OpaaxTag("UI.Layer.Menu"))->Source == ETagSource::Native);
    CHECK(lRegistry.Find(OpaaxTag("UI.Layer.Menu"))->Comment == "Listed again");   // code gave none

    lRegistry.SetConfigTags({});
    CHECK_FALSE(lRegistry.IsRegistered(OpaaxTag("UI.Layer.Popup")));
    CHECK(lRegistry.IsRegistered(OpaaxTag("UI.Layer.Menu")));

    // Sealed: code can declare no more.
    lRegistry.AddNative(OpaaxTag("Late.Tag"), OpaaxString());
    CHECK_FALSE(lRegistry.IsRegistered(OpaaxTag("Late.Tag")));
}

TEST_CASE("TagRegistry: Tags.config's data round-trips and fills the registry's config tags")
{
    TagsConfigData lData;
    lData.Tags.push_back(TagDefinition{ OpaaxTag("UI.Layer.Popup"), OpaaxString("Pop-ups") });

    const OpaaxString    lText = TConfigCodec<TagsConfigData>::ToText(lData);
    const nlohmann::json lJson = nlohmann::json::parse(lText.CStr());
    CHECK(lJson["Tags"][0]["Tag"] == "UI.Layer.Popup");
    CHECK(lJson["Tags"][0]["Comment"] == "Pop-ups");

    const TagsConfigData lRead = TConfigCodec<TagsConfigData>::FromText(OpaaxString(R"({"Tags":[{"Tag":"Game.Team.Red"},{"Tag":"Bad..Tag"}]})"));
    REQUIRE(lRead.Tags.size() == 2u);
    CHECK(lRead.Tags[0].Tag == OpaaxTag("Game.Team.Red"));
    CHECK_FALSE(lRead.Tags[1].Tag.IsValid());   // not a tag: read as none, left out of the registry

    TagRegistry lRegistry;
    lRegistry.SetConfigTags(lRead.Tags);
    CHECK(lRegistry.IsRegistered(OpaaxTag("Game.Team.Red")));
    CHECK(lRegistry.IsRegistered(OpaaxTag("Game.Team")));
    CHECK(lRegistry.GetAll().size() == 3u);   // Game, Game.Team, Game.Team.Red
}
