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
        virtual void OnRegisterEditorModules(EditorExtensionRegistrar& InRegistrar) {}
    
    public:
        /**
         * The project this editor edits (its folder in the workspace). A game's editor app returns its
         * name, as the project template does; empty falls back to the executable's name, with a warning.
         */
        virtual OpaaxString GetEditedProjectName() const { return OpaaxString(); }
        
        // =============================================================================
        // Getter
    public:
        IEditorService& Editor();
        // End Getter
        // =============================================================================
        
        // =============================================================================
        // Override
        // =============================================================================
    protected:
        //~Begin OpaaxApplication Interface
        void OnProvideServices(AppServiceLocator& InServices) override;
        void TickFrame() override;
        void OnModulesRegistered() override;
        TUniquePtr<IPaths> CreatePaths(const IPlatform& InPlatform, const CommandLine& InCommandLine) override;

        /** --upgrade-assets converts old asset files to .opaaxasset; without it, finding some is logged. */
        void OnPathsResolved(const IPaths& InPaths, const CommandLine& InCommandLine) override;
        WorldSpec GetStartupWorldSpec() const override;
        void PreRegisterConfig(IConfigSystem& ConfigSystem) override;
    public:
        void PostEngineStartup() override;
        void OnEvent(Event& InEvent) override;

    protected:
        /** A game asking to quit stops Play; the editor stays open. */
        void OnQuitGameRequested() override;

        /** Driven from outside: the editor's commands join the engine's. */
        void OnAutomationStarted(AutomationRunner& InRunner) override;
        //~End OpaaxApplication Interface
    };
}
