#pragma once

#include "Application/OpaaxApplication.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Application/Services/IEditorService.h"

namespace Opaax::Editor
{
    class EditorExtensionRegistrar;

    // =============================================================================
    // EditorApplication
    //      Composes the editor onto OpaaxApplication:
    //          Provides IEditorService, initializes it once the engine is up
    //          A game's editor executable is a ~10-line subclass of THIS. 
    //          There is no "editor for the game" — the editor is generic; the game registers into it.
    // =============================================================================
    class EditorApplication : public OpaaxApplication
    {
        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        EditorApplication(int InArgc, char** InArgv);
        
        // =============================================================================
        // Functions
        // =============================================================================
    protected:
        /**
         * 
         */
        virtual void OnRegisterEditorModules(EditorExtensionRegistrar& InRegistrar) {}
    
    public:
        /**
         * Which project this editor edits. CreatePaths builds <name>/<name>.opaaxproj from it, so this
         * CANNOT be read from the .opaaxproj — finding that file is what the name is for.
         * TODO: source it from a CLI arg (InArgv) or a workspace scan for a single *.opaaxproj.
         */
        virtual OpaaxString GetEditedProjectName() const { return OpaaxString(); }
        
        // =============================================================================
        // Getter
        IEditorService& Editor();
        // End Getter
        // =============================================================================
        
        // =============================================================================
        // Override
        // =============================================================================
    protected:
        //~Begin OpaaxApplication Interface
        void OnProvideServices(AppServiceLocator& InServices) override;

        /**
         * The editor boots into an EDIT world: authoring, not simulating. 
         * Play gets its own world by cloning this one, which is why the mode is fixed at creation and the edit world is never touched by playing.
         *
         * TODO: open the last-opened map rather than the project's startup level. Not map IO (M5
         * shipped that) — it needs persisted editor session state, which does not exist yet.
         */
        WorldSpec GetStartupWorldSpec() const override;

        void PostEngineStartup() override;
        void TickFrame() override;
        void OnEvent(Event& InEvent) override;
        void OnModulesRegistered() override;
        TUniquePtr<IPaths> CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv) override;
        //~End OpaaxApplication Interface
    };
}
