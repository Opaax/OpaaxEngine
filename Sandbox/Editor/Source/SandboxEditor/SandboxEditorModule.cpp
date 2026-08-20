#include "SandboxEditorModule.h"

#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // the Panels factory receives EditorContext& (D10)
#include "Commands/SandboxEditorCommandTags.h"
#include "Commands/ValidateSandboxCommand.h"
#include "Panels/SandboxPanel.h"
#include "Drawers/TagsComponentDrawer.h"
#include "Systems/QuadBoundsSubsystem.h"
#include "Components/HealthComponent.h"
#include "Resources/WaveResource.h"
#include "World/Components/DummyComponent.h"

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

    // REAL extension (M5 S4): the game's own verb, registered as a COMMAND and then bound in the
    // menu — the same two routes the editor's native File entries travel, in the same order, with
    // no privileged path (D10). The menu entry carries a TAG and nothing else, which is what lets
    // this verb be reached from anywhere later (a key binding, another panel) instead of only from
    // the one entry that used to hold its body.
    //
    // The M0 placeholder that used to sit here was `[] {}` and could not survive the route going
    // real; nor could the lambda that replaced it survive the bar becoming a tree of commands.
    InRegistrar.Commands().Register<SandboxEditor::ValidateSandboxCommand>(
        SandboxEditor::Tags::SANDBOX_COMMAND_VALIDATE);

    // Two levels deep, from a game module, with the editor's own "Tools" — if it had one — merged
    // in by identity rather than by string prefix.
    InRegistrar.Menus().Category("Tools").SubCategory("Debug")
               .AddCommand("Validate Sandbox", SandboxEditor::Tags::SANDBOX_COMMAND_VALIDATE);

    // The DEFAULT drawer, folded from what each component declares with OPAAX_PROPERTIES. It
    // replaced a hand-written DummyComponentDrawer that was three ImGui calls in a file of its own —
    // and HealthComponent, which never had a drawer and was therefore invisible, becomes editable
    // for the price of this line.
    InRegistrar.Drawers().Register<Opaax::DummyComponent>();
    InRegistrar.Drawers().Register<Sandbox::HealthComponent>();

    // Still HAND-WRITTEN, and the reason the override exists: a tag is not a field you type into, it
    // is add/remove against a validated vocabulary (I14). The authoring half of the tag dogfood — a
    // tag reaches a .opaaxmap without anyone editing json by hand.
    InRegistrar.Drawers().Register<Sandbox::TagsComponent, TagsComponentDrawer>();

    // REAL extension (M2d): the game's own file type. The editor never learns what a wave definition
    // is — the RUNTIME module already told the engine that WaveResource claims `.wave`, so this adds
    // only the glyph and what a double-click does. Adding a type touches no editor file; adding an
    // extension to an existing one touches nothing here at all.
    InRegistrar.ResourceTypes().Register<Sandbox::WaveResource>()
        .SetIcon(OpaaxString("[W]"))
        .SetActivate([](Opaax::Editor::EditorContext&, const Opaax::Editor::ResourceFile& InFile)
        {
            // A wave editor is a later milestone; today activation proves the route end-to-end.
            OPAAX_LOG(LogSandboxEditorModule, Info, "Wave definition activated: {}", InFile.RelPath.CStr());
        });

    // REAL extension (M2a): the game's own panel, registered through the same route the editor's native
    // Hierarchy uses. Constructed later by EditorService, once an EditorContext exists to hand it.
    // Under TOOLS, not Window, and hidden until asked for: a panel is not always a workspace pane,
    // and where its toggle lives is the panel's own statement rather than the editor's policy. The
    // category merges by identity with the "Tools > Debug" above — same Category(), one Tools menu.
    InRegistrar.Panels().Register<SandboxPanel>(Opaax::Editor::PanelDesc{
        .Id               = OPAAX_ID("Sandbox Panel"),
        .Menu             = OPAAX_ID("Tools"),
        .DefaultVisibility = Opaax::Editor::EPanelVisibility::Hidden
    });
}
