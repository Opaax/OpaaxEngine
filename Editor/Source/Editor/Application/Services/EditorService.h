#pragma once

#include "Editor/Application/Services/IEditorService.h"
#include "Editor/EditorContext.h"
#include "Editor/Application/Services/EditorPaths.h"
#include "Editor/Camera/EditorCamera.h"
#include "Editor/Operation/EditorGizmo.hpp"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Prefab/EditorPrefabDocument.h"
#include "Editor/Prefab/PrefabReconciler.h"
#include "Editor/Resources/EditorResourceEvents.h"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Operation/EditorViewport.hpp"
#include "Editor/Resources/ResourcePreview.h"
#include "Editor/Input/InputRoute.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/EditorLevelDocument.h"
#include "Editor/EditorSpriteSheetDocument.h"
#include "Editor/Resources/Types/Animation/EditorAnimationClipDocument.h"
#include "Editor/Resources/Types/Animation/EditorAnimationLibraryDocument.h"
#include "Editor/EditorFontFamilyDocument.h"
#include "Editor/EditorUICanvasDocument.h"
#include "Editor/Resources/Types/Input/EditorInputActionDocument.h"
#include "Editor/Resources/Types/Input/EditorInputMappingContextDocument.h"
#include "Editor/Resources/Types/Mover/EditorMoveModeDocument.h"
#include "Editor/Resources/Types/DataAsset/EditorDataAssetDocument.h"
#include "Editor/Resources/Types/Mover/EditorMoverDocument.h"
#include "Editor/PIE/PlayInEditor.h"
#include "Editor/UI/IEditorGui.h"
#include "Editor/UI/IEditorDialogs.h"
#include "Editor/Panels/EditorPanels.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Core/OpaaxTypes.h"   // TUniquePtr

namespace Opaax::Editor
{
    // =============================================================================
    // EditorService — the IEditorService, and the editor's composition root. Initialize() resolves
    //   its engine dependencies once (the only editor code that uses the locator) and builds the
    //   EditorContext passed to every panel and drawer. Provided last, so it shuts down first.
    // =============================================================================
    class EditorService final : public IEditorService
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        /** Out-of-line: it picks the concrete IEditorGui, only named in the .cpp. */
        EditorService();
        ~EditorService() override = default;
        
        // =============================================================================
        // Copy Delete
        // =============================================================================

        EditorService(const EditorService&)            = delete;
        EditorService& operator=(const EditorService&) = delete;
        
        // =============================================================================
        // Functions
        // =============================================================================
        
        // =============================================================================
        // Editor Native
    private:
        /**
         * Resolves the app's IPaths as EditorPaths, once. May be null (no edited project).
         */
        void CacheEditorPaths();

        /**
         * Creates the editor systems.
         */
        void CreateEditorSystems(IEngine& InEngine);

        void ClearEditorSystems();
        
        /**
         * Creates the editor context.
         */
        void CreateEditorContext(Window* InWindow, IEngine& InEngine);
        
        void ClearEditorContext();

        /**
         * Called when init is done.
         */
        void PostInitialized();
        
        /** The editor shortcuts, then the gui's UI pass. */
        void DrawGUI();
        
        // End Editor Native
        // =============================================================================
        
        // =============================================================================
        // Editor Registers 
    private:

        /**
         * Registers the editor's own panels, before the game module's and before sealing. Built by the
         * same loop as game panels.
         */
        void RegisterNativePanels();

        /**
         * Registers the editor's own menu entries (same route and order as the panels).
         */
        void RegisterNativeMenus();
        
        /**
         * Registers the editor's own commands, keyed by the tags in EditorNativeCommandsTags.hpp.
         * The menu bar, the Resource Browser and Ctrl+S reach them by tag, like a game's commands.
         */
        void RegisterNativeEditorCommand();
        
        /**
         * Registers how the browser shows each resource type and what a double-click does.
         * File extensions come from the engine's ResourceFormatRegistry.
         */
        void RegisterNativeResourceTypes();

        /**
         * Registers drawers for the engine's own components (all reflected, so the generic drawer).
         */
        void RegisterNativeDrawers();

        /**
         * Registers drawers for the engine and editor configs (drawn from their properties, not JSON).
         */
        void RegisterNativeConfigDrawers();

        /**
         * Gives every reflected component that has no drawer (typically a game's) the generic one, drawn
         * through its registry entry. After the game's editor module, so a custom drawer still wins.
         */
        void RegisterGenericComponentDrawers();

        /**
         * Registers the editor's viewport tools (gizmo mode, snapping, grid, pivot, space): order and
         * grouping only; the widgets are in Editor/Toolbar/EditorNativeViewportTools.h.
         * Runs after RegisterNativeEditorCommand (the mode buttons dispatch by tag).
         */
        void RegisterNativeViewportTools();

        // End Editor Registers 
        // =============================================================================
        
        // =============================================================================
        // World
    private:
        /**
         * Caches the world manager from the engine.
         * @return True if it is not null
         */
        bool SetWorldManagerFromEngine(IEngine& InEngine);

        /**
         * Unbinds from the world manager and clears the cached pointer.
         */
        void ClearWorldManager();

        /**
         * Binds to the world manager delegates. Call only when the world is valid.
         */
        void BindToWorldManagerDelegates();
        
        /**
         * Unbinds from the world manager delegates. Call before clearing the cached pointer.
         */
        void UnbindFromWorldManagerDelegates();
        
        /**
         * The active world changed (Play/Stop, or the active world destroyed). Retargets the selection
         * first, then notifies the panels.
         */
        void HandleActiveWorldChanged(World* InOld, World* InNew);

        /**
         * A world is being destroyed: clears the selection if it belonged to it (also for non-active worlds).
         */
        void HandleWorldDestroyed(World* InWorld);
        // End World
        // =============================================================================
        
        // Level
        // =============================================================================
        
        /** Adopts the level the engine opened at startup, and one of its maps. */
        void AdoptStartupLevel();
        
        // End Level
        // =============================================================================
        
        // =============================================================================
        // GUI
    private:
        /**
         * Initializes the GUI.
         * @return False if it failed
         */
        bool InitGUI(Window* InWindow);

        /** Shuts the UI down (panels, then backend). */
        void ClearGUI();

        /**
         * Resolves <ProjectRoot>/Editor/Save/imgui.ini (the dock layout), creating the directory if needed
         * (ImGui does not, and fails silently).
         * @return The absolute path, or empty if the editor paths are unavailable (then layouts are not saved)
         */
        OpaaxString ResolveLayoutIniPath() const;

        /**
         * The editor's UI font from config, with absolute paths (the config uses mount paths).
         * @return An empty primary when none is set (keeps the backend's default font)
         */
        EditorUIFont ResolveUIFont() const;

        /**
         * Ctrl+S and the other chords, in the UI pass (not in HandleReservedKeys: with an Edit world open
         * the engine never receives Ctrl).
         */
        void HandleAuthoringShortcuts();
        
        /**
         * The reserved editor keys. Runs after ImGui's capture check (never while a text field has the keyboard).
         * @return True if the key was reserved and consumed
         */
        bool HandleReservedKeys(Event& InEvent);
        
        void BuildGUIs();
        
        
        // End GUI
        // =============================================================================
        
        // =============================================================================
        // Panels
    private:
        /**
         * Runs after the game module registered (its panels get a toggle too) and before sealing.
         */
        void BindPanelToggles();
        // End Panels
        // =============================================================================

        /**
         * Recomputes the per-map dirty state, at most 4 times per second. Runs at the start of EndFrame.
         */
        void RefreshDirtyCache();

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
        void RegisterExtensions(const TFunction<void(EditorExtensionRegistrar&)>& InCollect) override;
        void Initialize() override;
        void BeginFrame() override;
        void EndFrame()   override;
        bool RouteInput(Event& InEvent) override;
        //~End IEditorService interface

        //~Begin IAppService interface
        void OnShutdown() override;   // release the context while the engine is still alive
        //~End IAppService interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        const EditorPaths*              m_EditorPaths = nullptr;
        WorldManager*                   m_WorldMgr = nullptr;
        
        // Built in the ctor and kept until the end of OnShutdown (the context references it).
        TUniquePtr<IEditorGui>          m_Gui;

        // The dialog backend. Owns no OS resource, so it needs no Clear step.
        TUniquePtr<IEditorDialogs>      m_Dialogs;

        EditorExtensionRegistrar        m_Extensions;

        TUniquePtr<ResourcePreview>     m_Preview;     
        TUniquePtr<EditorSelection>     m_Selection;

        /** "A document was saved", two-phase. Referenced by EditorContext. */
        TUniquePtr<EditorResourceEvents> m_ResourceEvents;

        /** The open prefab and its world. */
        TUniquePtr<EditorPrefabDocument> m_PrefabDocument;

        /** Updates prefab instances when a prefab is saved (a Reload cannot: they are entities). */
        TUniquePtr<PrefabReconciler>     m_PrefabReconciler;   
        TUniquePtr<EditorViewport>      m_Viewport;    
        TUniquePtr<EditorCamera>        m_Camera;      
        TUniquePtr<EditorGizmo>         m_Gizmo;       
        TUniquePtr<EditorUndo>          m_Undo;
        TUniquePtr<PlayInEditor>        m_PIE;         
        TUniquePtr<InputRoute>          m_InputRoute;  
        TUniquePtr<EditorMapDocument>   m_MapDocument;
        TUniquePtr<EditorLevelDocument> m_LevelDocument;
        TUniquePtr<EditorSpriteSheetDocument> m_SheetDocument;

        /** The open .opaaxclip and its data. */
        TUniquePtr<EditorAnimationClipDocument> m_ClipDocument;

        /** The open .opaaxanim and its data. */
        TUniquePtr<EditorAnimationLibraryDocument> m_LibraryDocument;
        TUniquePtr<EditorFontFamilyDocument>       m_FamilyDocument;

        /** The open .opaaxui and its canvas. */
        TUniquePtr<EditorUICanvasDocument>         m_UICanvasDocument;

        /** The mover documents (tuning and mover). */
        TUniquePtr<EditorMoveModeDocument>         m_MoveModeDocument;
        TUniquePtr<EditorMoverDocument>            m_MoverDocument;
        TUniquePtr<EditorInputActionDocument>      m_InputActionDocument;
        TUniquePtr<EditorInputMappingContextDocument> m_InputMapDocument;
        TUniquePtr<EditorDataAssetDocument>           m_DataAssetDocument;
        
        TUniquePtr<EditorContext>       m_Context;
        
        double m_LastDirtyCheck = -1.0;
    };
}
