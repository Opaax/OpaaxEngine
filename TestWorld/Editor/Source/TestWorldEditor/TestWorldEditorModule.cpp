#include "TestWorldEditorModule.h"

#include "Editor/Extensions/EditorExtensionRegistrar.h"

void TestWorldEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // The game's editor extensions (see SandboxEditorModule and Docs/Customizing.md):
    //
    //   A command, and a menu entry that runs it:
    //     InRegistrar.Commands().Register<MyCommand>(MY_COMMAND_TAG);
    //     InRegistrar.TitleBar().Category("Tools").AddCommand("My Tool", MY_COMMAND_TAG);
    //   A panel (derives from IEditorPanel, built from an EditorContext&):
    //     InRegistrar.Panels().Register<MyPanel>(Opaax::Editor::PanelDesc{ OPAAX_ID("My Panel") });
    //   An Inspector drawer, replacing the one made from a component's OPAAX_PROPERTIES:
    //     InRegistrar.Drawers().Register<MyComponent, MyComponentDrawer>();
    //   How an asset class of the game looks in the Resource Browser (the runtime registers the class):
    //     InRegistrar.AssetTypes().Register<MyAsset>().SetGlyph(Opaax::OpaaxString("[M]"));
    (void)InRegistrar;
}
