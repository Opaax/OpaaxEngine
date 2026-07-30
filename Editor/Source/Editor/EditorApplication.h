#pragma once

#include "Application/OpaaxApplication.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    class EditorExtensionRegistrar;

    // =============================================================================
    // EditorApplication — the editor host (Editor.md D1/D8). Composes the editor onto OpaaxApplication:
    //   provides IEditorService, initializes it once the engine is up, and (S10) wraps TickFrame with
    //   the UI. A game's editor executable is a ~10-line subclass of THIS. There is no "editor for the
    //   game" — the editor is generic; the game registers into it.
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
         * TODO: Get it from .OpaaxProj or somewhere strategic
         */
        virtual OpaaxString GetEditedProjectName() const { return OpaaxString(); }
        
        // =============================================================================
        // Override
        // =============================================================================
    protected:
        //~Begin OpaaxApplication Interface
        void OnProvideServices(AppServiceLocator& InServices) override;

        /**
         * The editor boots into an EDIT world: authoring, not simulating. Play gets its own
         * world by cloning this one (M4 S4/S5), which is why the mode is fixed at creation and
         * the edit world is never touched by playing.
         *
         * TODO (M5): open the last-opened map rather than the project's startup level.
         */
        WorldSpec GetStartupWorldSpec() const override;

        void PostEngineStartup() override;
        void TickFrame() override;
        void OnEvent(Event& InEvent) override;
        void OnModulesRegistered() override;
        UniquePtr<IPaths> CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv) override;
        //~End OpaaxApplication Interface
    };
}
