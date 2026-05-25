#pragma once
#if OPAAX_WITH_EDITOR

#include "Editor/Camera/EditorCamera.h"
#include "Editor/EditorEventBus.h"
#include "Editor/EditorState.h"
#include "Editor/IEditorPanel.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#include "Toolbar/MainMenuBar.h"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/OpaaxMathTypes.h"
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

        FORCEINLINE Editor::EditorEventBus& GetEventBus() noexcept { return *m_EventBus; }

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
        bool Startup()              override;
        void Shutdown()             override;
        void Render(double Alpha)   override;

        Uint32 GetEventCategoryFilter() const noexcept override { return EEventCategory_None; }
        //~End EngineSubsystemBase interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        void SetEditorState(Editor::EEditorState NewState);
        void RestoreEditorCamera();
        void TickEditorCameraInput();

    private:
        // Bus is held by UniquePtr so the EditorSubsystem stays movable (default move
        // ctor) — EditorEventBus itself is non-movable because SubscriptionTokens hold
        // a raw EditorEventBus* and a relocation would silently dangle them.
        UniquePtr<Editor::EditorEventBus> m_EventBus = MakeUnique<Editor::EditorEventBus>();

        // Editor camera lives across PIE cycles so pan/zoom state persists. Installed as
        // the non-owning active camera on CameraSubsystem in Editing; swapped out for a
        // fresh runtime OrthographicCamera at PIE Start; restored at PIE Stop. Shutdown
        // MUST clear CameraSubsystem's non-owning pointer before this UniquePtr resets,
        // because subsystem teardown is reverse-of-registration and EditorSubsystem
        // destroys before CameraSubsystem.
        UniquePtr<Editor::EditorCamera> m_EditorCamera;

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

        // Middle-mouse drag latch + last cursor position for the editor-camera pan.
        // Snapshot at MMB-press, advanced each Update frame while held.
        bool     m_bMMBDragging = false;
        Vector2F m_LastDragPos  = { 0.f, 0.f };

        OpaaxString m_LastDialogDir;
    };

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR
