#pragma once

#include "Application/Services/IAppService.h"
#include "Core/OpaaxTypes.h"   // TFunction

namespace Opaax { class Event; }   // RouteInput takes it by reference only

namespace Opaax::Editor
{
    class EditorExtensionRegistrar;

    // =============================================================================
    // IEditorService — the editor, exposed as an application service (Editor.md D1). Provided ONLY by
    //   EditorApplication::OnProvideServices, so it exists only in editor executables; the engine never
    //   knows it (D4). It is the editor's composition root (D3): it resolves its dependencies once,
    //   builds the EditorContext, and owns the editor's lifecycle. Resolved via GetAppService by editor
    //   code only — never by the engine or a runtime target.
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
         * Build the EditorContext + editor state.
         * Called by EditorApplication AFTER engine startup, when the subsystems the context references exist.
         * Separate from construction because the service is provided during Bootstrap, before the engine runs.
         */
        virtual void Initialize() = 0;

        /**
         * BeginFrame opens the ImGui frame
         */
        virtual void BeginFrame() = 0;

        /**
         * EndFrame draws the dockspace and submits ImGui's draw data to the backbuffer.
         */
        virtual void EndFrame()   = 0;

        // Input SEAM (Editor.md D5, S11). The host calls this from EditorApplication::OnEvent, so the
        // editor sees every window/input event BEFORE the base app enqueues it to the engine bus.
        // Returns true when the editor CONSUMED the event (it must not reach the engine). S11 body is the
        // ImGui WantCapture* gate ONLY — the full route (viewport focus, reserved keys, world-mode
        // dispatch, InputManager feed + ResetState) is the separate M-Input milestone.

        /**
         * The host calls this from EditorApplication::OnEvent.
         * So the editor sees every window/input event BEFORE the base app enqueues it to the engine bus.
         * 
         * ImGui WantCapture* gate ONLY — the full route (viewport focus, reserved keys, world-mode dispatch, InputManager feed + ResetState) is the separate M-Input milestone.
         * 
         * @param InEvent 
         * @return true when the editor CONSUMED the event (it must not reach the engine).
         */
        virtual bool RouteInput(Event& InEvent) = 0;
        
        /**
         * Driven by EditorApplication::OnModulesRegistered.
         * AFTER the game module, BEFORE the first world.
         * 
         * @param InCollect runs each editor module's OnRegister(EditorExtensionRegistrar&);
         */
        virtual void RegisterExtensions(const TFunction<void(EditorExtensionRegistrar&)>& InCollect) = 0;

        //----- null object ----------------------------------------------------
        static IEditorService& Null();
    };
}
