#include "SandboxEditorModule.h"

#include "Editor/Extensions/EditorExtensionRegistrar.h"

void SandboxEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // M0 demonstrative — proves each remaining counts-only route accepts & counts. `int` placeholders stand
    // in for the real drawers / asset-types / edit-world-systems the game defines from M2b on (the editor
    // analogue of SandboxModule::RegisterModule's DummyComponent). Replaced with real extensions in
    // M2b/M2d/M4/M5; each placeholder dies in the slice that makes its route real (overview F1).
    InRegistrar.Drawers().Register<int, int>();            // <TComponent, TDrawer>
    InRegistrar.AssetTypes().Register<int, int>();         // <TAsset, TActions>
    InRegistrar.EditWorldSystems().Register<int>();        // <TSystem>
    InRegistrar.Menus().Register("Tools/Validate Sandbox",
        [] {});                                            // path + command

    // Panels() is REAL now (M2a) — the `return 0` placeholder cannot convert to UniquePtr<IEditorPanel>.
    // S3 registers a real SandboxPanel here, which is the milestone's dogfood proof.
}
