#include "TetrisEditorApp.h"

#include "TetrisEditorModule.h"

TetrisEditorApp::TetrisEditorApp(int InArgc, char** InArgv)
    : Opaax::Editor::EditorApplication(InArgc, InArgv)
{
}

void TetrisEditorApp::OnRegisterEditorModules(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // The game's editor extensions plug in here: after the game module, before the first world.
    TetrisEditorModule().OnRegister(InRegistrar);
}
