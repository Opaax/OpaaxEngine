#pragma once

#include "Application/Services/IAppService.h"
#include "Core/OpaaxTypes.h"   // TFunction

namespace Opaax
{
    class AutomationRunner;
    class Event;
}

namespace Opaax::Editor
{
    class EditorExtensionRegistrar;

    // =============================================================================
    // IEditorService — the editor as an application service. Only provided by EditorApplication, so it
    //   exists only in editor executables. The editor's composition root: resolves its dependencies,
    //   builds the EditorContext and runs the editor's lifecycle.
    // =============================================================================
    class IEditorService : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IEditorService)
        
        // =============================================================================
        // Functions
        // =============================================================================
        
        /**
         * Builds the EditorContext and editor state. Called after engine startup (the context
         * references subsystems created then).
         */
        virtual void Initialize() = 0;

        /**
         * Opens the ImGui frame.
         */
        virtual void BeginFrame() = 0;

        /**
         * Draws the dockspace and submits ImGui's draw data to the backbuffer.
         */
        virtual void EndFrame()   = 0;

        /**
         * Called from EditorApplication::OnEvent, so the editor sees every window/input event before it
         * reaches the engine.
         * @return True if the editor consumed the event (it must not reach the engine)
         */
        virtual bool RouteInput(Event& InEvent) = 0;
        
        /**
         * Called by EditorApplication::OnModulesRegistered, after the game module and before the first world.
         * @param InCollect Runs each editor module's OnRegister(EditorExtensionRegistrar&)
         */
        virtual void RegisterExtensions(const TFunction<void(EditorExtensionRegistrar&)>& InCollect) = 0;

        /** Stops Play In Editor if it is running or paused (a game asking to quit). */
        virtual void StopPlay() = 0;

        /**
         * The app is driven from outside: adds the editor's commands to InRunner. With bInOwnsInput (a
         * script), the editor feeds the game no input of its own.
         */
        virtual void RegisterAutomation(AutomationRunner& InRunner, bool bInOwnsInput) = 0;

        //----- null object ----------------------------------------------------
        static IEditorService& Null();
    };
}
