#include "SandboxEditorModule.h"

#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // the Panels factory receives EditorContext& (D10)
#include "Panels/SandboxPanel.h"

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

    // REAL extension (M2a): the game's own panel, registered through the same route the editor's native
    // Hierarchy uses. Constructed later by EditorService, once an EditorContext exists to hand it.
    InRegistrar.Panels().Register("Sandbox Panel",
        [](Opaax::Editor::EditorContext& InContext) -> Opaax::UniquePtr<Opaax::Editor::IEditorPanel>
        {
            return Opaax::MakeUnique<SandboxPanel>(InContext);
        });
}
