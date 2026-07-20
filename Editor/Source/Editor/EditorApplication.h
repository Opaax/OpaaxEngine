#pragma once

#include "Application/OpaaxApplication.h"
#include "Core/OpaaxString.hpp"

namespace Opaax::Editor
{
    class EditorExtensionRegistrar;   // OnRegisterEditorModules fills it (D10)

    // =============================================================================
    // EditorApplication — the editor host (Editor.md D1/D8). Composes the editor onto OpaaxApplication:
    //   provides IEditorService, initializes it once the engine is up, and (S10) wraps TickFrame with
    //   the UI. A game's editor executable is a ~10-line subclass of THIS. There is no "editor for the
    //   game" — the editor is generic; the game registers into it.
    // =============================================================================
    class EditorApplication : public OpaaxApplication
    {
    public:
        EditorApplication(int InArgc, char** InArgv);

    protected:
        //~Begin OpaaxApplication seams
        void OnProvideServices(AppServiceLocator& InServices) override;  // provide IEditorService (D1)
        void PostEngineStartup() override;                              // EditorService.Initialize()
        void TickFrame() override;                                      // UI begin -> Engine().Loop() -> UI end (S10)
        void OnEvent(Event& InEvent) override;                          // editor sees events first -> EditorService::RouteInput (S11)
        void OnModulesRegistered() override;                            // editor extension registration + seal, before first world (S12/D10)
        UniquePtr<IPaths> CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv) override;
        //~End OpaaxApplication seams

        /**
         * Route the game's editor module(s) into the extension registrar (Editor.md D10). Base no-op; a
         * game's editor exe overrides it — the analogue of OnRegisterModules for editor extensions. Called
         * by OnModulesRegistered (after the game module, before the first world). See IEditorModule.
         */
        virtual void OnRegisterEditorModules(EditorExtensionRegistrar& InRegistrar) {}

        /**
         * The game project this editor edits, e.g. "Sandbox" — resolves to <workspace>/<name>/
         * <name>.opaaxproj (the layout convention). A game's editor exe overrides this. Empty (base)
         * falls back to the exe-stem default (which, for an editor exe, is usually wrong — hence the
         * override contract). NOTE: TickFrame stays the base engine frame at S8 (UI wrapping = S10).
         */
        virtual OpaaxString GetEditedProjectName() const { return OpaaxString(); }
    };
}
