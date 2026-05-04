#pragma once
#if OPAAX_WITH_EDITOR

#include "Editor/EditorState.h"
#include "Editor/IEditorPanel.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#include "Toolbar/MainMenuBar.h"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/OpaaxString.hpp"
#include "Core/Systems/EngineSubsystem.h"


struct GLFWwindow;


namespace Opaax
{
    /**
     * @class EditorSubsystem
     * Owns the ImGui context lifetime and drives the per-frame Begin/End cycle.
     * Registers all IAssetTypeActions and IComponentDrawers at Startup.
     */
    class OPAAX_API EditorSubsystem final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(EditorSubsystem)

        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        EditorSubsystem() = default;
        explicit EditorSubsystem(CoreEngineApp* InEngineApp)
            : EngineSubsystemBase(InEngineApp)
        {}
        ~EditorSubsystem() override = default;

        // =============================================================================
        // Copy - Delete
        // =============================================================================
        EditorSubsystem(const EditorSubsystem&)            = delete;
        EditorSubsystem& operator=(const EditorSubsystem&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
        EditorSubsystem(EditorSubsystem&&)                 = default;
        EditorSubsystem& operator=(EditorSubsystem&&)      = default;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void BeginFrame();
        void EndFrame();
        void DrawPanels();

        void RegisterViewportCallbacks();
        void RegisterAssetTypeActions();
        void RegisterComponentDrawers();

        //------------------------------------------------------------------------------
        // Get
    public:
        FORCEINLINE IRenderTarget& GetRenderTarget() noexcept { return m_ViewportPanel; }

        FORCEINLINE Editor::EEditorState GetEditorState() const noexcept { return m_EditorState; }

        // Panel visibility — references so the menu bar can toggle them in-place via ImGui::MenuItem.
        FORCEINLINE bool& GetShowHierarchyRef()    noexcept { return m_bShowHierarchy; }
        FORCEINLINE bool& GetShowInspectorRef()    noexcept { return m_bShowInspector; }
        FORCEINLINE bool& GetShowAssetBrowserRef() noexcept { return m_bShowAssetBrowser; }
        FORCEINLINE bool& GetShowViewportRef()     noexcept { return m_bShowViewport; }

        //------------------------------------------------------------------------------
        // PIE transitions

        void EnterPlayMode();   // Editing → Playing  (snapshots scene to temp file)
        void PauseToggle();     // Playing ↔ Paused
        void ExitPlayMode();    // Playing|Paused → Editing  (restores scene from temp)

        // Triggers an AssetBrowserPanel rescan — called by MainMenuBar after Save Scene As
        // so a freshly written .scene.json shows up without a manual Refresh click.
        void RefreshAssetBrowser();

        //------------------------------------------------------------------------------
        // Last-used dialog dir — volatile (lives for the editor session, no persistence).
        // Saved by MainMenuBar after a successful Open / SaveAs so the next dialog opens
        // where the user left off. Empty until the first successful dialog.
        const OpaaxString& GetLastDialogDir() const noexcept { return m_LastDialogDir; }
        void               SetLastDialogDir(const char* InAbsDir) { m_LastDialogDir = OpaaxString(InAbsDir); }

        // =============================================================================
        // Override EngineSubsystemBase
        // =============================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup()            override;
        void Shutdown()           override;
        void Render(double Alpha) override;

        Uint32 GetEventCategoryFilter() const noexcept override { return EEventCategory_None; }
        //~End EngineSubsystemBase interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        void SetEditorState(Editor::EEditorState NewState);

    private:
        Editor::MainMenuBar       m_MainMenuBar;
        Editor::HierarchyPanel    m_HierarchyPanel;
        Editor::InspectorPanel    m_InspectorPanel;
        Editor::ViewportPanel     m_ViewportPanel;
        Editor::AssetBrowserPanel m_AssetBrowserPanel;

        Editor::EEditorState m_EditorState = Editor::EEditorState::Editing;

        bool m_bShowHierarchy    = true;
        bool m_bShowInspector    = true;
        bool m_bShowAssetBrowser = true;
        bool m_bShowViewport     = true;

        OpaaxString m_LastDialogDir;
    };

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR
