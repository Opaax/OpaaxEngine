#include "TestWorldEditorApp.h"

#include "TestWorldEditorModule.h"

TestWorldEditorApp::TestWorldEditorApp(int InArgc, char** InArgv)
    : Opaax::Editor::EditorApplication(InArgc, InArgv)
{
}

void TestWorldEditorApp::OnRegisterEditorModules(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // The game's editor extensions plug in here: after the game module, before the first world.
    TestWorldEditorModule().OnRegister(InRegistrar);
}
