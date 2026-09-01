#pragma once

#include "Editor/Application/Services/IEditorService.h"
#include "Editor/EditorContext.h"
#include "Editor/Application/Services/EditorPaths.h"
#include "Editor/Camera/EditorCamera.h"
#include "Editor/Operation/EditorGizmo.hpp"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EditorViewport.hpp"
#include "Editor/Resources/ResourcePreview.h"
#include "Editor/Input/InputRoute.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/EditorLevelDocument.h"
#include "Editor/PIE/PlayInEditor.h"
#include "Editor/UI/IEditorGui.h"
#include "Editor/UI/IEditorDialogs.h"
#include "Editor/Panels/EditorPanels.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Core/OpaaxTypes.h"   // TUniquePtr

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
        /** Out-of-line: it picks the concrete IEditorGui, which only the .cpp needs to name. */
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
         * Resolves the app's IPaths to EditorPaths ONCE, into m_EditorPaths. EditorApplication::CreatePaths
         * falls back to a plain Paths when no edited project is declared, so this genuinely can end up null —
         * every consumer treats that as "no editor space", never as an error.
         */
        void CacheEditorPaths();

        /**
         * Create editor systems
         */
        void CreateEditorSystems(IEngine& InEngine);

        /**
         * 
         */
        void ClearEditorSystems();
        
        /**
         * Create the editor context
         */
        void CreateEditorContext(Window* InWindow, IEngine& InEngine);
        
        /***/
        void ClearEditorContext();

        /**
         * Called when the init is done
         */
        void PostInitialized();
        
        /** The authoring shortcuts, then the gui's one UI pass. */
        void DrawGUI();
        
        // End Editor Native
        // =============================================================================
        
        // =============================================================================
        // Editor Registers 
    private:

        /**
         * Registers the editor's OWN panels into m_Extensions.Panels(), first — before the game module and
         * before Seal() (the D9/§2 "engine natives -> game module -> seal" order, one level down). Native
         * panels get no special route: they are built by the same factory loop as game panels (D10).
         */
        void RegisterNativePanels();

        /**
         * The editor's own menu commands, into m_Extensions.TitleBar() — same route, same ordering rule
         * and same lack of privilege as RegisterNativePanels (M5 S4).
         */
        void RegisterNativeMenus();
        
        /**
         * The editor's own commands, into m_Extensions.Commands() — the author loop's verbs, keyed
         * by the tags in EditorNativeCommandsTags.hpp. Same route, same ordering rule and same lack
         * of privilege as RegisterNativePanels: the menu bar, the Resource Browser and Ctrl+S all
         * reach them BY TAG, exactly as a game module's command would be reached.
         */
        void RegisterNativeEditorCommand();
        
        /**
         * No extension here, and none anywhere else in the editor: the engine's ResourceFormatRegistry
         * owns which files a type claims (Engine::RegisterNativeResourceFormats), so this says only
         * what the browser shows and what a double-click does.
         */
        void RegisterNativeResourceTypes();

        /**
         * The engine's own configs into m_Extensions.ConfigDrawers(), so the Config panel draws
         * their fields instead of their json. Same registry template, same two forms and the same
         * ordering rule as the component drawers — only the resolver differs.
         */
        void RegisterNativeConfigDrawers();

        /**
         * The editor's own viewport tools, into m_Extensions.ViewportTools() (③b) — gizmo mode,
         * snapping, grid, pivot and space. Same route and same lack of privilege as the panels: a
         * game module adds a tool with the identical call.
         *
         * States the ORDER and the grouping only; each tool's widgets live in
         * Editor/Toolbar/EditorNativeViewportTools.h, the EditorNativeCommands shape.
         *
         * Runs AFTER RegisterNativeEditorCommand, because the mode buttons dispatch by tag.
         */
        void RegisterNativeViewportTools();

        // End Editor Registers 
        // =============================================================================
        
        // =============================================================================
        // World
    private:
        /**
         * Cache the world mgr from engine
         * @param InEngine 
         * @return true if world manager != nullptr
         */
        bool SetWorldManagerFromEngine(IEngine& InEngine);

        /**
         * Unbind delegate from World manager
         * Make sure to clear the cache ptr.
         */
        void ClearWorldManager();

        /**
         * Bind to World mgr delegates.
         * Should be call only if world is valid
         */
        void BindToWorldManagerDelegates();
        
        /**
         * Unbind from World mgr delegates.
         * Should be call only if world is valid and before clearing the world mgr cache
         */
        void UnbindFromWorldManagerDelegates();
        
        /**
         * The active world was replaced (PIE Play/Stop, or the active world being destroyed).
         * Retargets the selection FIRST, then notifies every panel — so no panel can observe a
         * selection pointing into the world that was just left.
         */
        void HandleActiveWorldChanged(World* InOld, World* InNew);

        /**
         * A world is going away. Clears the selection when it belonged to that world — the safety
         * net HandleActiveWorldChanged cannot provide, since a NON-active world can die too
         * (Entity holds a raw World*, so a stale one dangles).
         */
        void HandleWorldDestroyed(World* InWorld);
        // End World
        // =============================================================================
        
        // Level
        // =============================================================================
        
        /** Adopt the level the engine opened at boot, and one of its maps for editing. */
        void AdoptStartupLevel();
        
        // End Level
        // =============================================================================
        
        // =============================================================================
        // GUI
    private:
        /**
         * 
         * @param InWindow 
         * @return False if not initialized correctly
         */
        bool InitGUI(Window* InWindow);

        /** The whole UI stack down, panels then backend — IEditorGui::Teardown owns that order. */
        void ClearGUI();

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
         * Ctrl+S, in the UI pass.
         *
         * NOT in HandleReservedKeys, and not by choice of style: with an Edit world open the input
         * route is ClosedEditMode, so the engine's InputManager never receives Ctrl and could not
         * answer IsCtrlDown(). See the body for why the split (route-level F-keys vs UI-pass
         * chords) is the right shape rather than a workaround.
         */
        void HandleAuthoringShortcuts();
        
        /**
         * The reserved editor keys — D5's step 3, and only that step. Runs AFTER ImGui's capture
         * check, so a shortcut can never fire while a text field has the keyboard.
         *
         * @return true when the key was a reserved one and the editor consumed it.
         */
        bool HandleReservedKeys(Event& InEvent);
        
        // End GUI
        // =============================================================================
        
        // =============================================================================
        // Panels
    private:
        /**
         * 
         */
        void BuildPanels();
        
        /**
         * Runs AFTER the game module has registered (so its panels get a toggle too) and BEFORE the seal.
         */
        void BindPanelToggles();
        // End Panels
        // =============================================================================

        /**
         * Re-derive the per-map dirty answers, at most 4×/s (**MP5**).
         *
         * The throttle is here because the frame clock is; the answers live in EditorLevelDocument
         * beside the baselines they come from, so the Hierarchy can mark every map without a
         * capture per row. Runs at the top of EndFrame, before anything that reads it.
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
        void OnShutdown() override;   // release the context while the engine it references is still alive
        //~End IAppService interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        const EditorPaths*              m_EditorPaths = nullptr;
        WorldManager*                   m_WorldMgr = nullptr;
        
        // Built in the ctor and never reset — the context holds a reference to it until OnShutdown's
        // last step, well after ClearGUI().
        TUniquePtr<IEditorGui>          m_Gui;

        // Beside the gui, and built with it: the concrete modal backend. It owns no OS resource and
        // has nothing to shut down, so unlike the gui it needs no Clear step.
        TUniquePtr<IEditorDialogs>      m_Dialogs;

        EditorExtensionRegistrar        m_Extensions;

        TUniquePtr<ResourcePreview>     m_Preview;     
        TUniquePtr<EditorSelection>     m_Selection;   
        TUniquePtr<EditorViewport>      m_Viewport;    
        TUniquePtr<EditorCamera>        m_Camera;      
        TUniquePtr<EditorGizmo>         m_Gizmo;       
        TUniquePtr<PlayInEditor>        m_PIE;         
        TUniquePtr<InputRoute>          m_InputRoute;  
        TUniquePtr<EditorMapDocument>   m_MapDocument;
        TUniquePtr<EditorLevelDocument> m_LevelDocument;
        
        TUniquePtr<EditorContext>       m_Context;
        
        double m_LastDirtyCheck = -1.0;
    };
}
