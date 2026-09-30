#include "SandboxEditorModule.h"

#include "Core/Log/Logger.h"   // OPAAX_LOG + LogCategory
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/EditorContext.h"   // the Panels factory receives EditorContext&
#include "Commands/SandboxEditorCommandTags.h"
#include "Commands/ValidateSandboxCommand.h"
#include "Drawers/TagsComponentDrawer.h"
#include "Components/GunComponent.h"
#include "Components/HealthComponent.h"
#include "Resources/WaveResource.h"

using namespace Opaax;

namespace
{
    constexpr LogCategory LogSandboxEditorModule{"SandboxEditorModule"};
}

void SandboxEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // No Edit-only world subsystem is registered at the moment (QuadBoundsSubsystem was removed), so
    // EditWorldSystems() has no caller. Re-adding one is a single Register call.

    // The game's own action, registered as a command, then added to the menu by tag.
    InRegistrar.Commands().Register<SandboxEditor::ValidateSandboxCommand>(
        SandboxEditor::Tags::SANDBOX_COMMAND_VALIDATE);

    // Two levels deep; merged with an existing "Tools" menu by identity.
    InRegistrar.TitleBar().Category("Tools").SubCategory("Debug")
               .AddCommand("Validate Sandbox", SandboxEditor::Tags::SANDBOX_COMMAND_VALIDATE);

    // The game's own components only (engine components are registered by
    // EditorService::RegisterNativeDrawers). The default drawer, built from OPAAX_PROPERTIES.
    InRegistrar.Drawers().Register<Sandbox::HealthComponent>();

    // Same default drawer; the two prefab fields accept a .opaaxprefab dragged from the browser.
    InRegistrar.Drawers().Register<Sandbox::GunComponent>();

    // Hand-written: tags are added/removed from a validated vocabulary, not typed as a field.
    InRegistrar.Drawers().Register<Sandbox::TagsComponent, TagsComponentDrawer>();

    // The game's own file type. The runtime module registers WaveResource (and its .wave extension);
    // the editor only adds the glyph and the double-click action.
    InRegistrar.ResourceTypes().Register<Sandbox::WaveResource>()
        .SetGlyph(OpaaxString("[W]"))
        .SetActivate([](Opaax::Editor::EditorContext&, const Opaax::Editor::ResourceFile& InFile)
        {
            // No wave editor yet; activating only logs.
            OPAAX_LOG(LogSandboxEditorModule, Info, "Wave definition activated: {}", InFile.RelPath.CStr());
        });

    // No game panel is registered at the moment. Re-adding one is a single Register call.
}
