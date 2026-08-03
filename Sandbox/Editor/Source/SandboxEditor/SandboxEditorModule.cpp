#include "SandboxEditorModule.h"

#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // the Panels factory receives EditorContext& (D10)
#include "World/WorldManager.h"     // the Validate command walks the active world
#include "World/World.h"
#include "World/Entity/EntityMeta.h"
#include "Components/HealthComponent.h"
#include "Panels/SandboxPanel.h"
#include "Drawers/DummyComponentDrawer.h"
#include "Systems/QuadBoundsSubsystem.h"

// OPAAX_LOG expands to an unqualified ToSpdLevel(...) — bring Opaax into scope, as SandboxPanel does.
using namespace Opaax;

namespace
{
    constexpr LogCategory LogSandboxEditorModule{"SandboxEditorModule"};
}

void SandboxEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // REAL extension (M4 S5): an Edit-only world subsystem, registered through a route that now
    // forwards into the ENGINE's WorldSubsystemRegistry — the same registry SandboxModule's
    // Play-only QuadOscillatorSubsystem lands in. Two modules, two routes, one candidate list, and
    // each World takes the subset its mode qualifies for. The M0 `Register<int>()` placeholder that
    // used to sit here could not survive the route going real, which is exactly why it was left.
    InRegistrar.EditWorldSystems().Register<QuadBoundsSubsystem>();

    // REAL extension (M5 S4): the last route to graduate, and the M0 placeholder that used to sit
    // here could not come with it — it was `[] {}`, a zero-argument lambda, and a command that
    // cannot reach the world could never do anything worth registering. It receives EditorContext&
    // now (D3), which is exactly what makes this one able to answer a question about the game.
    InRegistrar.Menus().Register("Tools/Validate Sandbox",
        [](Opaax::Editor::EditorContext& InContext)
        {
            Opaax::World* lWorld = InContext.Worlds.GetActiveWorld();
            if (lWorld == nullptr)
            {
                OPAAX_LOG(LogSandboxEditorModule, Warn, "Validate Sandbox: no active world")
                return;
            }

            // A real (if small) check the GAME defines and the editor knows nothing about: every
            // authored entity should carry the game's own HealthComponent. Counting the ones that
            // do not is the sort of thing a validate command exists for.
            Uint64 lTotal   = 0;
            Uint64 lMissing = 0;
            lWorld->Each<Opaax::EntityMeta>(
                [&](auto InEntity, const Opaax::EntityMeta&)
                {
                    ++lTotal;
                    if (!lWorld->GetRegistry().all_of<Sandbox::HealthComponent>(InEntity))
                    {
                        ++lMissing;
                    }
                });

            OPAAX_LOG(LogSandboxEditorModule, Info,
                "Validate Sandbox: world '{}' — {} entity(ies), {} without a HealthComponent",
                lWorld->GetName().CStr(), lTotal, lMissing)
        });

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
