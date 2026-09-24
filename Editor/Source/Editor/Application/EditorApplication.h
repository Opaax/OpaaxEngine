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
         * TODO: source it from a CLI arg (InArgv) or a workspace scan for a single *.opaaxproj.
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
        TUniquePtr<IPaths> CreatePaths(const IPlatform& InPlatform, int InArgc, char** InArgv) override;
        WorldSpec GetStartupWorldSpec() const override;
        void PreRegisterConfig(IConfigSystem& ConfigSystem) override;
    public:
        void PostEngineStartup() override;
        void OnEvent(Event& InEvent) override;
        //~End OpaaxApplication Interface
    };
}
