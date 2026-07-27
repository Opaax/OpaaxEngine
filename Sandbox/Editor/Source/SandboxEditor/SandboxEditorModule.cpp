#include "SandboxEditorModule.h"

#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // the Panels factory receives EditorContext& (D10)
#include "Panels/SandboxPanel.h"

void SandboxEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // M0 demonstrative — proves each remaining counts-only route accepts & counts. `int` placeholders stand
    // in for the real asset-types / edit-world-systems the game defines from M2d on (the editor analogue of
    // SandboxModule::RegisterModule's DummyComponent). Each placeholder dies in the slice that makes its
    // route real (overview F1) — it cannot survive its registry going real.
    InRegistrar.AssetTypes().Register<int, int>();         // <TAsset, TActions>
    InRegistrar.EditWorldSystems().Register<int>();        // <TSystem>
    InRegistrar.Menus().Register("Tools/Validate Sandbox",
        [] {});                                            // path + command

    // Drawers() is REAL now (M2b) — the `<int, int>` placeholder cannot instantiate against it
    // (`int::TryGet`, `int::Draw`). S2 registers the real DummyComponentDrawer here.

    // REAL extension (M2a): the game's own panel, registered through the same route the editor's native
    // Hierarchy uses. Constructed later by EditorService, once an EditorContext exists to hand it.
    InRegistrar.Panels().Register("Sandbox Panel",
        [](Opaax::Editor::EditorContext& InContext) -> Opaax::UniquePtr<Opaax::Editor::IEditorPanel>
        {
            return Opaax::MakeUnique<SandboxPanel>(InContext);
        });
}
