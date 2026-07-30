#pragma once

#include "Editor/EditorApplication.h"

// =============================================================================
// __NAME__EditorApp — the game's editor executable (Editor.md D8): a thin composition of the
// generic OpaaxEditorLib. It registers the SAME __NAME__Module as __NAME__.exe (D9), then the
// game's editor extensions (D10). The editor is generic; the game registers into it — never
// the reverse.
// =============================================================================
class __NAME__EditorApp final : public Opaax::Editor::EditorApplication
{
public:
    __NAME__EditorApp(int InArgc, char** InArgv);

protected:
    void RegisterModules(Opaax::ModuleRegistrar& InRegistrar) override;
    void OnRegisterEditorModules(Opaax::Editor::EditorExtensionRegistrar& InRegistrar) override;

    Opaax::OpaaxString GetEditedProjectName() const override { return Opaax::OpaaxString("__NAME__"); }
};
