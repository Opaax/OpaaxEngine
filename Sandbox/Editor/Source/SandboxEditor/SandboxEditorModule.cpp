#include "SandboxEditorModule.h"

#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // Panels factory receives EditorContext& (D10)

void SandboxEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // M0 demonstrative — proves each of the 5 routes accepts & counts. `int` placeholders stand in for the
    // real drawers / asset-types / edit-world-systems the game defines from M2 on (the editor analogue of
    // SandboxModule::RegisterModule's DummyComponent). Replaced with real extensions in M2/M4/M5.
    InRegistrar.Drawers().Register<int, int>();            // <TComponent, TDrawer>
    InRegistrar.AssetTypes().Register<int, int>();         // <TAsset, TActions>
    InRegistrar.EditWorldSystems().Register<int>();        // <TSystem>
    InRegistrar.Panels().Register("Sandbox Panel",
        [](Opaax::Editor::EditorContext&) { return 0; });  // name + factory(EditorContext&)
    InRegistrar.Menus().Register("Tools/Validate Sandbox",
        [] {});                                            // path + command
}
