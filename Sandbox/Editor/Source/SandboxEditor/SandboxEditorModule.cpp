#include "SandboxEditorModule.h"

#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // the Panels factory receives EditorContext& (D10)
#include "Panels/SandboxPanel.h"
#include "Drawers/DummyComponentDrawer.h"

// OPAAX_LOG expands to an unqualified ToSpdLevel(...) — bring Opaax into scope, as SandboxPanel does.
using namespace Opaax;

namespace
{
    constexpr LogCategory LogSandboxEditorModule{"SandboxEditorModule"};
}

void SandboxEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // M0 demonstrative — proves each remaining counts-only route accepts & counts. `int` placeholders stand
    // in for the real edit-world-systems / menus the game defines from M4 on (the editor analogue of
    // SandboxModule::RegisterModule's DummyComponent). Each placeholder dies in the slice that makes its
    // route real (overview F1) — it cannot survive its registry going real.
    InRegistrar.EditWorldSystems().Register<int>();        // <TSystem>
    InRegistrar.Menus().Register("Tools/Validate Sandbox",
        [] {});                                            // path + command

    // REAL extension (M2b): the game's own component drawer. The editor never learns what a
    // DummyComponent is — it just invokes this closure, which self-checks whether the selected entity
    // carries one. Duck-typed, no base class (D7).
    InRegistrar.Drawers().Register<Opaax::DummyComponent, DummyComponentDrawer>();

    // REAL extension (M2d): the game's own file type. The editor never learns what a wave definition is —
    // it matches the extension, shows this icon/label, and hands the file back to this closure on a
    // double-click. Adding a type touches no editor file.
    InRegistrar.ResourceTypes().Register(Opaax::Editor::ResourceTypeDesc{
        .Extension  = OPAAX_ID(".wave"),
        .Label      = OPAAX_ID("Wave Definition"),
        .Icon       = OpaaxString("[W]"),
        .OnActivate = [](Opaax::Editor::EditorContext&, const Opaax::Editor::ResourceFile& InFile)
        {
            // A wave editor is a later milestone; today activation proves the route end-to-end.
            OPAAX_LOG(LogSandboxEditorModule, Info, "Wave definition activated: {}", InFile.RelPath.CStr())
        }
    });

    // REAL extension (M2a): the game's own panel, registered through the same route the editor's native
    // Hierarchy uses. Constructed later by EditorService, once an EditorContext exists to hand it.
    InRegistrar.Panels().Register("Sandbox Panel",
        [](Opaax::Editor::EditorContext& InContext) -> Opaax::UniquePtr<Opaax::Editor::IEditorPanel>
        {
            return Opaax::MakeUnique<SandboxPanel>(InContext);
        });
}
