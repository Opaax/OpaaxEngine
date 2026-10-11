#include "TetrisApp.h"

TetrisApp::TetrisApp(int InArgc, char** InArgv) : Opaax::OpaaxApplication(InArgc, InArgv)
{
}

void TetrisApp::OnInitializeApplication()
{
    // Intentionally empty: Tetris registers no app-level config. Overridden so the base's
    // "not overridden" trace does not fire.
}
