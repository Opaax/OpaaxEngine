#pragma once

#include "Editor/EditorApplication.h"

// =============================================================================
// __NAME__EditorApp — the game's editor executable: a thin layer over the generic OpaaxEditorLib.
// It registers the same __NAME__Module as __NAME__.exe, then the game's editor extensions. The
// editor is generic; the game registers into it, never the reverse.
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
