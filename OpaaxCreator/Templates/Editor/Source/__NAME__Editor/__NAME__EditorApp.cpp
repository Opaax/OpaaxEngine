#include "__NAME__EditorApp.h"

#include "__NAME__EditorModule.h"
#include "__NAME__.h"
#include "Engine/Modules/ModuleRegistrar.h"

__NAME__EditorApp::__NAME__EditorApp(int InArgc, char** InArgv)
    : Opaax::Editor::EditorApplication(InArgc, InArgv)
{
}

void __NAME__EditorApp::RegisterModules(Opaax::ModuleRegistrar& InRegistrar)
{
    __NAME__Module().OnRegister(InRegistrar);
}

void __NAME__EditorApp::OnRegisterEditorModules(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // D10: the game's editor extensions plug in here — after the game module, before the first world.
    __NAME__EditorModule().OnRegister(InRegistrar);
}
