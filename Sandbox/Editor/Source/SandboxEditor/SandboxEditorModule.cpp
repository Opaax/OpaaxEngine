#include "SandboxEditorModule.h"

#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // the Panels factory receives EditorContext& (D10)
#include "Commands/SandboxEditorCommandTags.h"
#include "Commands/ValidateSandboxCommand.h"
#include "Drawers/TagsComponentDrawer.h"
#include "Components/GunComponent.h"
#include "Components/HealthComponent.h"
#include "Resources/WaveResource.h"

// OPAAX_LOG expands to an unqualified ToSpdLevel(...) — bring Opaax into scope, as SandboxPanel does.
using namespace Opaax;

namespace
{
    constexpr LogCategory LogSandboxEditorModule{"SandboxEditorModule"};
}

void SandboxEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // QuadBoundsSubsystem was DELETED (2026-08-31, user's call: the green quad outlines are not
    // wanted any more). It was the M4 S5 dogfood of EditWorldSystems() — a game module registering
    // an Edit-only world subsystem — so, stated rather than discovered later:
    //
    // KNOWN COST: that route now has NO caller, and the seal log reads editWorldSystems=0. The
    // mechanism is still exercised by Sandbox's Play-only QuadOscillatorSubsystem on the runtime
    // route into the same registry, so what is unproven is specifically the EDITOR-side entry
    // point, not world subsystems or mode filtering. Re-adding one is a single Register call.

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
    InRegistrar.TitleBar().Category("Tools").SubCategory("Debug")
               .AddCommand("Validate Sandbox", SandboxEditor::Tags::SANDBOX_COMMAND_VALIDATE);

    // THE GAME'S OWN COMPONENTS ONLY. Transform, Sprite, Camera and Dummy are engine types and are
    // registered by EditorService::RegisterNativeDrawers — this module used to register them, which
    // meant a new project had a blank Inspector for every engine component until it remembered four
    // types it does not own.
    //
    // The DEFAULT drawer, folded from what the component declares with OPAAX_PROPERTIES: Health
    // never had a drawer and was therefore invisible, and becomes editable for the price of this
    // line — no drawer code anywhere.
    InRegistrar.Drawers().Register<Sandbox::HealthComponent>();

    // Same default drawer; its two prefab fields get the typed drop target for free (I15), so a
    // bullet is assigned by dragging a .opaaxprefab from the browser onto the field.
    InRegistrar.Drawers().Register<Sandbox::GunComponent>();

    // Still HAND-WRITTEN, and the reason the override exists: a tag is not a field you type into, it
    // is add/remove against a validated vocabulary (I14). The authoring half of the tag dogfood — a
    // tag reaches a .opaaxmap without anyone editing json by hand.
    InRegistrar.Drawers().Register<Sandbox::TagsComponent, TagsComponentDrawer>();

    // REAL extension (M2d): the game's own file type. The editor never learns what a wave definition
    // is — the RUNTIME module already told the engine that WaveResource claims `.wave`, so this adds
    // only the glyph and what a double-click does. Adding a type touches no editor file; adding an
    // extension to an existing one touches nothing here at all.
    InRegistrar.ResourceTypes().Register<Sandbox::WaveResource>()
        .SetGlyph(OpaaxString("[W]"))
        .SetActivate([](Opaax::Editor::EditorContext&, const Opaax::Editor::ResourceFile& InFile)
        {
            // A wave editor is a later milestone; today activation proves the route end-to-end.
            OPAAX_LOG(LogSandboxEditorModule, Info, "Wave definition activated: {}", InFile.RelPath.CStr());
        });

    // SandboxPanel was DELETED in ② and this module no longer registers a panel. Its one feature —
    // Spawn Quad — is now Create Entity plus Add Component, through EntityOps, which also fixes what
    // it got wrong: it spawned with no OwnerMap, so its quads landed in "(runtime - not saved)" and
    // no Save could ever write them (**WM2**).
    //
    // KNOWN COST, stated rather than discovered later: this was the only GAME-module panel, so M2's
    // dogfood gate — "a Sandbox-module custom panel appears with zero changes to OpaaxEditorLib" —
    // is no longer live. The PanelRegistry route is still exercised by the editor's eight native
    // panels, and this module's other four extensions (a world subsystem, a command, a hand-written
    // drawer, a resource type) are untouched. Re-adding a game panel is one Register call.
}
