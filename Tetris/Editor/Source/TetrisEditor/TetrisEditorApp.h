#pragma once

#include "Editor/Application/EditorApplication.h"

// =============================================================================
// TetrisEditorApp — the game's editor executable: a thin layer over the generic OpaaxEditorLib.
// The game module's types register themselves; this adds the game's editor extensions. The
// editor is generic; the game registers into it, never the reverse.
// =============================================================================
class TetrisEditorApp final : public Opaax::Editor::EditorApplication
{
public:
    TetrisEditorApp(int InArgc, char** InArgv);

protected:
    void OnRegisterEditorModules(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override;

    Opaax::OpaaxString GetEditedProjectName() const override { return Opaax::OpaaxString("Tetris"); }
};
