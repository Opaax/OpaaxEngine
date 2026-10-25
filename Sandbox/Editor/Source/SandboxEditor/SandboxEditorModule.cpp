#include "SandboxEditorModule.h"

#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // the Panels factory receives EditorContext&
#include "Commands/SandboxEditorCommandTags.h"
#include "Commands/ValidateSandboxCommand.h"
#include "Drawers/TagsComponentDrawer.h"
#include "Data/EnemyWave.h"

using namespace Opaax;

void SandboxEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // The game's own action, registered as a command, then added to the menu by tag.
    InRegistrar.Commands().Register<SandboxEditor::ValidateSandboxCommand>(
        SandboxEditor::Tags::SANDBOX_COMMAND_VALIDATE);

    // Two levels deep; merged with an existing "Tools" menu by identity.
    InRegistrar.TitleBar().Category("Tools").SubCategory("Debug")
               .AddCommand("Validate Sandbox", SandboxEditor::Tags::SANDBOX_COMMAND_VALIDATE);

    // Components with OPAAX_PROPERTIES (Health, Gun) need no line here: the editor draws them from
    // their fields. Register a drawer only to replace that UI.
    // Hand-written: tags are added/removed from a validated vocabulary, not typed as a field.
    InRegistrar.Drawers().Register<Sandbox::TagsComponent, TagsComponentDrawer>();

    // The game's own file format: an asset class read from .wave files. The runtime module registers
    // the class (and its extension); the editor only adds its look. A double-click opens its window.
    InRegistrar.AssetTypes().Register<Sandbox::EnemyWave>()
        .SetLabel(OpaaxString("Enemy Wave"))
        .SetCategory(OpaaxString("Gameplay"))
        .SetGlyph(OpaaxString("[W]"));

    // No game panel is registered at the moment. Re-adding one is a single Register call.
}
