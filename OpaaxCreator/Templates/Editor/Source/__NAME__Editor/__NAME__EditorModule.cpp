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
    //   InRegistrar.ResourceTypes().Register(Opaax::Editor::ResourceTypeDesc{ ... });
    //   InRegistrar.Menus().Register("Tools/My Tool", [] {});
    (void)InRegistrar;
}
