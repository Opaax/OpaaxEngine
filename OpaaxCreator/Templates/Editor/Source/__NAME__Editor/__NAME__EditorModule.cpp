#include "__NAME__EditorModule.h"

#include "Editor/Extensions/EditorExtensionRegistrar.h"

void __NAME__EditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // Register the game's editor extensions here (see SandboxEditorModule for live examples):
    //
    //   InRegistrar.Drawers().Register<MyComponent, MyComponentDrawer>();
    //   InRegistrar.Panels().Register("My Panel",
    //       [](Opaax::Editor::EditorContext& InContext) -> Opaax::UniquePtr<Opaax::Editor::IEditorPanel>
    //       { return Opaax::MakeUnique<MyPanel>(InContext); });
    //   InRegistrar.ResourceTypes().Register<MyResource>().SetIcon("[R]").SetActivate(...);
    //       (the EXTENSIONS live on MyResource itself, via OPAAX_RESOURCE_FORMAT, and are
    //        registered by the runtime module: InRegistrar.Resources().Register<MyResource>())
    //   InRegistrar.Menus().Register("Tools/My Tool",
    //       [](Opaax::Editor::EditorContext& InContext) { /* act on InContext.Worlds, .Selection, ... */ });
    (void)InRegistrar;
}
