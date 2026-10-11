#include "__NAME__EditorApp.h"

#include "__NAME__EditorModule.h"

__NAME__EditorApp::__NAME__EditorApp(int InArgc, char** InArgv)
    : Opaax::Editor::EditorApplication(InArgc, InArgv)
{
}

void __NAME__EditorApp::OnRegisterEditorModules(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // The game's editor extensions plug in here: after the game module, before the first world.
    __NAME__EditorModule().OnRegister(InRegistrar);
}
