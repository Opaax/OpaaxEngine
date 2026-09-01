#include "SandboxEditorModule.h"

#include "Application/Services/ILogger.h"   // OPAAX_LOG + LogCategory
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // the Panels factory receives EditorContext& (D10)
#include "Commands/SandboxEditorCommandTags.h"
#include "Commands/ValidateSandboxCommand.h"
#include "Drawers/TagsComponentDrawer.h"
#include "Components/HealthComponent.h"
#include "Resources/WaveResource.h"
#include "World/Components/CameraComponent.h"
#include "World/Components/DummyComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Components/TransformComponent.h"

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

    // The DEFAULT drawer, folded from what each component declares with OPAAX_PROPERTIES. It
    // replaced a hand-written DummyComponentDrawer that was three ImGui calls in a file of its own —
    // and HealthComponent, which never had a drawer and was therefore invisible, becomes editable
    // for the price of this line.
    // Every entity has one, so this is the drawer that always shows.
    InRegistrar.Drawers().Register<Opaax::TransformComponent>();

    InRegistrar.Drawers().Register<Opaax::DummyComponent>();
    InRegistrar.Drawers().Register<Sandbox::HealthComponent>();

    // Seven fields, four widget kinds, zero drawer code — including the texture slot, which is a
    // drag target because the field's TYPE says which resource it names (TResourcePath).
    InRegistrar.Drawers().Register<Opaax::SpriteComponent>();

    // ① — two fields, both already covered by a built-in drawer, so the camera costs the editor
    // this line and nothing else. (Engine-native components getting their drawers from the GAME's
    // module is a gap: a new project's editor would have to remember this. Its answer is a
    // RegisterNativeDrawers() in EditorService, and that is not this milestone's job.)
    InRegistrar.Drawers().Register<Opaax::CameraComponent>();

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
