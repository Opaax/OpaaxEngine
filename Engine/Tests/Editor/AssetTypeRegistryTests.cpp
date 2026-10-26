// Suite: AssetTypeRegistry (Editor/Extensions/AssetTypeRegistry.h) — how an asset class looks in the
// editor and what edits it: defaults from its name, registered overrides, what a subclass inherits.
#include <doctest.h>

#include "Editor/Extensions/AssetTypeRegistry.h"
#include "Resources/Asset/Asset.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace AssetTypeRegistryTestTypes
{
    class AudioClipDefLike final : public Asset
    {
        OPAAX_OBJECT(AudioClipDefLike, Asset)
    };

    class UIHealthBarStyle final : public Asset
    {
        OPAAX_OBJECT(UIHealthBarStyle, Asset)
    };

    class Material2DLike : public Asset
    {
        OPAAX_OBJECT(Material2DLike, Asset)
    };

    class ToonMaterial final : public Material2DLike
    {
        OPAAX_OBJECT(ToonMaterial, Material2DLike)
    };

    class CustomEditor final : public IAssetEditor
    {
    public:
        explicit CustomEditor(EditorContext&) {}
        void DrawContents(AssetDocument&) override {}
    };
}

using namespace AssetTypeRegistryTestTypes;

TEST_CASE("AssetTypeRegistry: a class registered nowhere is named and drawn from its class name")
{
    CHECK(AssetTypeRegistry::DefaultLabel(AudioClipDefLike::StaticClass()) == "Audio Clip Def Like");
    CHECK(AssetTypeRegistry::DefaultLabel(UIHealthBarStyle::StaticClass()) == "UI Health Bar Style");
    CHECK(AssetTypeRegistry::DefaultLabel(Material2DLike::StaticClass()) == "Material2D Like");

    CHECK(AssetTypeRegistry::DefaultGlyph(AudioClipDefLike::StaticClass()) == "[ACDL]");
    CHECK(AssetTypeRegistry::DefaultGlyph(Material2DLike::StaticClass()) == "[M2DL]");
    CHECK(AssetTypeRegistry::DefaultGlyph(ToonMaterial::StaticClass()) == "[TM]");

    const AssetTypeRegistry lRegistry;
    CHECK(lRegistry.LabelOf(ToonMaterial::StaticClass()) == "Toon Material");
    CHECK(lRegistry.GlyphOf(ToonMaterial::StaticClass()) == "[TM]");
    CHECK(lRegistry.IconOf(ToonMaterial::StaticClass()).IsEmpty());
    CHECK(lRegistry.CategoryOf(ToonMaterial::StaticClass()).IsEmpty());
    CHECK(lRegistry.EditorOf(ToonMaterial::StaticClass()) == nullptr);   // the property editor
}

TEST_CASE("AssetTypeRegistry: a registered look wins; a subclass inherits icon, category and editor, not the name")
{
    AssetTypeRegistry lRegistry;
    lRegistry.Register<Material2DLike>()
             .SetLabel(OpaaxString("Material"))
             .SetGlyph(OpaaxString("[Mat]"))
             .SetIcon(OpaaxString("Icons/T_Material_Icon.png"))
             .SetCategory(OpaaxString("Rendering"))
             .SetEditor<CustomEditor>();

    CHECK(lRegistry.LabelOf(Material2DLike::StaticClass()) == "Material");
    CHECK(lRegistry.GlyphOf(Material2DLike::StaticClass()) == "[Mat]");

    // The subclass keeps its own name...
    CHECK(lRegistry.LabelOf(ToonMaterial::StaticClass()) == "Toon Material");
    CHECK(lRegistry.GlyphOf(ToonMaterial::StaticClass()) == "[TM]");

    // ...and sits with its parent, edited like it.
    CHECK(lRegistry.IconOf(ToonMaterial::StaticClass()) == "Icons/T_Material_Icon.png");
    CHECK(lRegistry.CategoryOf(ToonMaterial::StaticClass()) == "Rendering");
    CHECK(lRegistry.EditorOf(ToonMaterial::StaticClass()) == lRegistry.EditorOf(Material2DLike::StaticClass()));
    CHECK(lRegistry.EditorOf(ToonMaterial::StaticClass()) != nullptr);

    // Its own registration overrides what it inherited.
    lRegistry.Register<ToonMaterial>().SetCategory(OpaaxString("Stylized"));
    CHECK(lRegistry.CategoryOf(ToonMaterial::StaticClass()) == "Stylized");

    CHECK(lRegistry.FindExact(UIHealthBarStyle::StaticClass()) == nullptr);
}

TEST_CASE("AssetTypeRegistry: registering a class twice keeps one entry")
{
    AssetTypeRegistry lRegistry;
    lRegistry.Register<UIHealthBarStyle>().SetGlyph(OpaaxString("[HB]"));
    lRegistry.Register<UIHealthBarStyle>().SetCategory(OpaaxString("UI"));

    CHECK(lRegistry.Entries().size() == 1);
    CHECK(lRegistry.GlyphOf(UIHealthBarStyle::StaticClass()) == "[HB]");
    CHECK(lRegistry.CategoryOf(UIHealthBarStyle::StaticClass()) == "UI");
}
