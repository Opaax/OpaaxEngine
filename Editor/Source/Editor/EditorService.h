#pragma once

#include "Editor/IEditorService.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorPaths.h"
#include "Editor/EditorSelection.h"
#include "Editor/InputRoute.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/PlayInEditor.h"
#include "Editor/UI/IEditorUIBackend.h"
#include "Editor/Panels/ViewportPanel.h"
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
         * The editor's own menu commands, into m_Extensions.Menus() — same route, same ordering rule
         * and same lack of privilege as RegisterNativePanels (M5 S4).
         */
        void RegisterNativeMenus();

        /**
         * Render one level of the menu bar from the registry's flat entry list.
         *
         * Recursive: at InDepth, an entry whose path ends there is a MenuItem, and anything deeper
         * opens a submenu gathering every entry that shares the segment. Computing the tree here
         * rather than storing one is what keeps registration a single push_back.
         *
         * @param InIndices Indices into MenuRegistry::Entries() that belong at this level.
         * @param InDepth   Which path segment this level names.
         */
        void DrawMenuLevel(const TDynArray<Uint32>& InIndices, Uint32 InDepth);

        /** Every registered entry's index — the root call's argument for DrawMenuLevel. */
        TDynArray<Uint32> BuildAllIndices() const;

        /**
         * Point the document at the map the ENGINE already opened (M5 S5).
         *
         * Re-reads the project's startup level to find out which file that was, rather than the
         * engine growing a "what did I load" accessor for a single consumer. The LEVEL'S FIRST MAP
         * is the one edited — the editor opens one at a time, which is a real limitation of M5
         * rather than an oversight.
         */
        void AdoptStartupMap();

        /** The open map's name plus a `*` when the world no longer matches it. Once a frame, on the bar. */
        void DrawDocumentStatus();

        /**
         * Ctrl+S, in the UI pass.
         *
         * NOT in HandleReservedKeys, and not by choice of style: with an Edit world open the input
         * route is ClosedEditMode, so the engine's InputManager never receives Ctrl and could not
         * answer IsCtrlDown(). See the body for why the split (route-level F-keys vs UI-pass
         * chords) is the right shape rather than a workaround.
         */
        void HandleAuthoringShortcuts();

        // ---- menu commands (D3: a command's whole input is the context) ----------------------
        /** False while PIE runs — the active world is then a Play clone, not the authored map. */
        static bool CanEditMap(const EditorContext& InContext);
        static void SaveMapCommand(EditorContext& InContext);
        static void SaveMapAsCommand(EditorContext& InContext);
        static void OpenMapCommand(EditorContext& InContext);

        /**
         * Open InAbsPath into the ACTIVE world — the shared body behind both the File menu's
         * "Open Map..." and a double-click in the Resource Browser, so the two cannot diverge on
         * the parts that matter (the PIE guard, the unsaved-changes prompt, clearing the
         * selection before the entities it points at stop existing).
         */
        static void OpenMapAt(EditorContext& InContext, const OpaaxString& InAbsPath);

        /**
         * `.opaaxmap` / `.opaaxlevel` into m_Extensions.ResourceTypes() — the editor's own core
         * formats registered through the route a game's file type uses (M2d), with no privileged
         * path into the browser.
         */
        void RegisterNativeResourceTypes();

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
         * (Entity holds a raw World*, so a stale one dangles).
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

        TUniquePtr<EditorSelection>  m_Selection;       // M2a: the single selection; EditorContext.Selection refs it
        TUniquePtr<PlayInEditor>     m_PIE;             // M4 S5: the PIE state machine; EditorContext.PIE refs it
        TUniquePtr<InputRoute>       m_InputRoute;      // M-Input S2: is the engine being fed; EditorContext.InputRoute refs it
        TUniquePtr<EditorMapDocument> m_MapDocument;    // M5 S5: the open .opaaxmap; EditorContext.MapDocument refs it

        // M5 S5: the derived dirty answer, cached. EditorMapDocument::IsDirty is a full capture +
        // serialize and stays pure (so it is testable and cannot go stale on its own); the
        // THROTTLING lives here, where there is a frame clock to throttle against.
        double m_LastDirtyCheck = -1.0;
        bool   m_CachedDirty    = false;
        TUniquePtr<EditorContext>    m_Context;
        TUniquePtr<IEditorUIBackend> m_UIBackend;
        TUniquePtr<ViewportPanel>    m_ViewportPanel;   // M1: world-to-texture panel; owns the offscreen FBO

        // M2a: every registered panel (native + game), built from m_Extensions.Panels() in registration
        // order. The Viewport stays a NAMED member above, deliberately outside this collection — it drives
        // IEngine::SetPrimaryRenderTarget, so its construction/teardown order must not depend on what a
        // game module registers (overview §3.3).
        TDynArray<TUniquePtr<IEditorPanel>> m_Panels;

        EditorExtensionRegistrar    m_Extensions;

        // M4 S5: the WorldManager we subscribed to, so OnShutdown can unsubscribe. Non-owning, and
        // held separately from m_Context because the unsubscribe must happen BEFORE the context dies.
        WorldManager*               m_SubscribedWorlds = nullptr;
    };
}
