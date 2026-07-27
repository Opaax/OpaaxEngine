#pragma once

#include "Editor/IEditorService.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorSelection.h"
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

        UniquePtr<EditorSelection>  m_Selection;       // M2a: the single selection; EditorContext.Selection refs it
        UniquePtr<EditorContext>    m_Context;
        UniquePtr<IEditorUIBackend> m_UIBackend;
        UniquePtr<ViewportPanel>    m_ViewportPanel;   // M1: world-to-texture panel; owns the offscreen FBO

        // M2a: every registered panel (native + game), built from m_Extensions.Panels() in registration
        // order. The Viewport stays a NAMED member above, deliberately outside this collection — it drives
        // IEngine::SetPrimaryRenderTarget, so its construction/teardown order must not depend on what a
        // game module registers (overview §3.3).
        TDynArray<UniquePtr<IEditorPanel>> m_Panels;

        EditorExtensionRegistrar    m_Extensions;
    };
}
