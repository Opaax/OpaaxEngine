#pragma once

#include "Editor/IEditorService.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorPaths.h"
#include "Editor/EditorSelection.h"
#include "Editor/PlayInEditor.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Core/OpaaxTypes.h"   // UniquePtr

namespace Opaax::Editor
{
    // =============================================================================
    // EditorService — the concrete IEditorService and the editor's composition root (D3). At
    //   Initialize() it resolves its engine dependencies ONCE (the only place editor code touches the
    //   locator) and builds the EditorContext that every panel/drawer receives by ctor. Owns nothing
    //   the engine owns — the context is references. Provided last (OnProvideServices), so it tears
    //   down FIRST (reverse order), before the engine service it references.
    // =============================================================================
    class EditorService final : public IEditorService
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        EditorService()           = default;
        ~EditorService() override = default;
        
        // =============================================================================
        // Copy Delete
        // =============================================================================

        EditorService(const EditorService&)            = delete;
        EditorService& operator=(const EditorService&) = delete;
        
        // =============================================================================
        // Functions
        // =============================================================================
        
    private:
        /**
         *
         */
        void DrawDockspace();

        /**
         * Registers the editor's OWN panels into m_Extensions.Panels(), first — before the game module and
         * before Seal() (the D9/§2 "engine natives -> game module -> seal" order, one level down). Native
         * panels get no special route: they are built by the same factory loop as game panels (D10).
         */
        void RegisterNativePanels();

        /**
         * Resolves <ProjectRoot>/Editor/Save/imgui.ini — the dock layout ImGui loads on the first frame and
         * rewrites as it changes — CREATING the directory if absent (ImGui will not, and its save fails
         * silently on a missing dir).
         *
         * @return The absolute ini path, or an EMPTY string if the editor's path service is unavailable, in
         *   which case the caller must leave IniFilename null (ImGui's own "don't persist" contract).
         */
        OpaaxString ResolveLayoutIniPath() const;

        /**
         * Resolves the app's IPaths to EditorPaths ONCE, into m_EditorPaths. EditorApplication::CreatePaths
         * falls back to a plain Paths when no edited project is declared, so this genuinely can end up null —
         * every consumer treats that as "no editor space", never as an error.
         */
        void CacheEditorPaths();

        /**
         * The reserved editor keys — D5's step 3, and only that step. Runs AFTER ImGui's capture
         * check, so a shortcut can never fire while a text field has the keyboard.
         *
         * @return true when the key was a reserved one and the editor consumed it.
         */
        bool HandleReservedKeys(Event& InEvent);

        /**
         * The active world was replaced (PIE Play/Stop, or the active world being destroyed).
         * Retargets the selection FIRST, then notifies every panel — so no panel can observe a
         * selection pointing into the world that was just left.
         */
        void HandleActiveWorldChanged(World* InOld, World* InNew);

        /**
         * A world is going away. Clears the selection when it belonged to that world — the safety
         * net HandleActiveWorldChanged cannot provide, since a NON-active world can die too
         * (EditorSelection's M4 FIXME: Entity holds a raw World*, so a stale one dangles).
         */
        void HandleWorldDestroyed(World* InWorld);

        // =============================================================================
        // Get - Set
    public:
        /**
         * @return The injected context (valid after Initialize)
         */
        EditorContext& GetContext() noexcept { return *m_Context; }
        // End Get - Set
        // =============================================================================
        
        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorService interface
        void Initialize() override;
        void BeginFrame() override;
        void EndFrame()   override;
        bool RouteInput(Event& InEvent) override;
        void RegisterExtensions(const TFunction<void(EditorExtensionRegistrar&)>& InCollect) override;
        //~End IEditorService interface

        //~Begin IAppService interface
        void OnShutdown() override;   // release the context while the engine it references is still alive
        //~End IAppService interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Dock layout file. ImGui stores io.IniFilename as a BORROWED const char* — it never copies the
        // string — so this must stay alive, and unmodified, until ImGui::DestroyContext() (which saves
        // through that very pointer). Assigned once in Initialize(); never cleared in OnShutdown().
        OpaaxString                 m_LayoutIniPath;

        // M2d: the app's IPaths downcast once (CacheEditorPaths). NON-OWNING — IPaths is an app service
        // that outlives this one. Null when no edited project was declared.
        const EditorPaths*          m_EditorPaths = nullptr;

        UniquePtr<EditorSelection>  m_Selection;       // M2a: the single selection; EditorContext.Selection refs it
        UniquePtr<PlayInEditor>     m_PIE;             // M4 S5: the PIE state machine; EditorContext.PIE refs it
        UniquePtr<EditorContext>    m_Context;
        UniquePtr<IEditorUIBackend> m_UIBackend;
        UniquePtr<ViewportPanel>    m_ViewportPanel;   // M1: world-to-texture panel; owns the offscreen FBO

        // M2a: every registered panel (native + game), built from m_Extensions.Panels() in registration
        // order. The Viewport stays a NAMED member above, deliberately outside this collection — it drives
        // IEngine::SetPrimaryRenderTarget, so its construction/teardown order must not depend on what a
        // game module registers (overview §3.3).
        TDynArray<UniquePtr<IEditorPanel>> m_Panels;

        EditorExtensionRegistrar    m_Extensions;

        // M4 S5: the WorldManager we subscribed to, so OnShutdown can unsubscribe. Non-owning, and
        // held separately from m_Context because the unsubscribe must happen BEFORE the context dies.
        WorldManager*               m_SubscribedWorlds = nullptr;
    };
}
