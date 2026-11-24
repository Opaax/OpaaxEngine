// Suite: TagPicker — what the editor's tag fields offer: the registered tags under a field's root, the
// unregistered ones flagged, and Tags.config's list after "Register".
#include <doctest.h>

#include "Core/Reflection/OpaaxProperty.h"
#include "Editor/Properties/TagPicker.h"
#include "Engine/Tags/TagRegistry.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    struct TagRootSubject
    {
        OpaaxTag Layer;

        OPAAX_PROPERTIES(TagRootSubject, OPAAX_PROP(Layer).SetTagRoot("UI.Layer"))
    };
}

TEST_CASE("TagPicker: a field's rows are the tags under its root, indented by depth, with their comments")
{
    TagRegistry lTags;
    lTags.AddNative(OpaaxTag("UI.Layer.Menu"), OpaaxString("Menus"));
    lTags.AddNative(OpaaxTag("UI.Layer.Menu.Pause"), OpaaxString());
    lTags.AddNative(OpaaxTag("Audio.Bus.Music"), OpaaxString());

    const TDynArray<TagPickerRow> lRows = TagPickerRows(lTags, OpaaxTag("UI.Layer"));
    REQUIRE(lRows.size() == 2u);
    CHECK(lRows[0].Tag == OpaaxTag("UI.Layer.Menu"));
    CHECK(lRows[0].Label == "Menu");
    CHECK(lRows[0].Comment == "Menus");
    CHECK(lRows[1].Label == "  Pause");

    // No root: every tag, indented from the top.
    const TDynArray<TagPickerRow> lAll = TagPickerRows(lTags, OpaaxTag());
    CHECK(lAll.size() == lTags.GetAll().size());
    CHECK(lAll[0].Label == "Audio");
    CHECK(lAll[1].Label == "  Bus");
}

TEST_CASE("TagPicker: a set tag that is not registered is flagged; none is not")
{
    TagRegistry lTags;
    lTags.AddNative(OpaaxTag("UI.Layer.Menu"), OpaaxString());

    CHECK(IsTagUnregistered(lTags, OpaaxTag("UI.Layer.Popup")));
    CHECK_FALSE(IsTagUnregistered(lTags, OpaaxTag("UI.Layer.Menu")));
    CHECK_FALSE(IsTagUnregistered(lTags, OpaaxTag("UI.Layer")));   // a parent exists with its child
    CHECK_FALSE(IsTagUnregistered(lTags, OpaaxTag()));
}

TEST_CASE("TagPicker: Register adds the tag at the end of Tags.config's list, once")
{
    const TDynArray<TagDefinition> lExisting = { TagDefinition{ OpaaxTag("Game.Team"), OpaaxString("Teams") } };

    const TDynArray<TagDefinition> lAdded = WithTagListed(lExisting, OpaaxTag("UI.Layer.Popup"));
    REQUIRE(lAdded.size() == 2u);
    CHECK(lAdded[1].Tag == OpaaxTag("UI.Layer.Popup"));
    CHECK(lAdded[0].Comment == "Teams");

    CHECK(WithTagListed(lAdded, OpaaxTag("UI.Layer.Popup")).size() == 2u);
    CHECK(WithTagListed(lAdded, OpaaxTag()).size() == 2u);
}

TEST_CASE("TagPicker: a field's tag root is in its meta, and a group's reaches the fields inside")
{
    // GetProperties() returns its tuple by value: keep it, or the field is read from a destroyed temporary.
    const auto  lProperties = TagRootSubject::GetProperties();
    const auto& lProperty   = TupleGet<0>(lProperties);
    REQUIRE(lProperty.Meta.TagRoot != nullptr);
    CHECK(OpaaxString(lProperty.Meta.TagRoot) == "UI.Layer");

    PropertyMeta lGroup;
    lGroup.TagRoot = "Audio.Bus";
    CHECK(OpaaxString(InheritMeta(PropertyMeta{}, lGroup).TagRoot) == "Audio.Bus");
    CHECK(OpaaxString(InheritMeta(lProperty.Meta, lGroup).TagRoot) == "UI.Layer");   // its own wins
}
