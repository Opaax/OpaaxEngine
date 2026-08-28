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
#include "Editor/UI/IEditorUIBackend.h"
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
        
        // =============================================================================
        // Native Editor
    private:

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
         * The Preview panel's id, stated ONCE.
         *
         * Two calls here need it — the panel's registration and the activate closure that opens it —
         * and a panel deliberately does NOT carry its own id (PanelDesc.h: it used to be stated
         * twice, with nothing making the two agree). So it lives with the two callers instead.
         */
        static OpaaxStringID PreviewPanelId() { return OPAAX_ID("Preview"); }

        /**
         * The engine's own configs into m_Extensions.ConfigDrawers(), so the Config panel draws
         * their fields instead of their json. Same registry template, same two forms and the same
         * ordering rule as the component drawers — only the resolver differs.
         */
        void RegisterNativeConfigDrawers();

        /**
         * One Window-menu entry per registered panel, from its PanelDesc.
         *
         * Runs AFTER the game module has registered (so its panels get a toggle too) and BEFORE the
         * seal. Not a privileged path: it is the same AddCommand a module calls, carrying the same
         * tag, and the entry's tick reads EditorPanels so it cannot drift from the window's own
         * close button.
         */
        void BindPanelToggles();

        // End Native Editor
        // =============================================================================
        
        /** The dockspace and the menu bar — everything drawn outside a panel. */
        void DrawDockspace();

        /** Adopt the level the engine opened at boot, and one of its maps for editing. */
        void AdoptStartupLevel();

        /**
         * Re-derive the per-map dirty answers, at most 4×/s (**MP5**).
         *
         * The throttle is here because the frame clock is; the answers live in EditorLevelDocument
         * beside the baselines they come from, so the Hierarchy can mark every map without a
         * capture per row. Runs at the top of EndFrame, before anything that reads it.
         */
        void RefreshDirtyCache();

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
        // Dock layout file. ImGui stores io.IniFilename as a BORROWED const char* — it never copies the
        // string — so this must stay alive, and unmodified, until ImGui::DestroyContext() (which saves
        // through that very pointer). Assigned once in Initialize(); never cleared in OnShutdown().
        OpaaxString                 m_LayoutIniPath;

        // M2d: the app's IPaths downcast once (CacheEditorPaths). NON-OWNING — IPaths is an app service
        // that outlives this one. Null when no edited project was declared.
        const EditorPaths*          m_EditorPaths = nullptr;

        TUniquePtr<ResourcePreview>  m_Preview;         // ④b: what a double-click asked to see; EditorContext.Preview refs it
        TUniquePtr<EditorSelection>  m_Selection;       // M2a: what is selected; EditorContext.Selection refs it
        TUniquePtr<EditorViewport>   m_Viewport;        // ②: the viewport's pixel size, written by its panel
        TUniquePtr<EditorCamera>     m_Camera;          // ①: the Edit viewpoint, held here so it outlives a PIE cycle
        TUniquePtr<EditorGizmo>      m_Gizmo;           // ③: the transform handles' grab state; EditorContext.Gizmo refs it
        TUniquePtr<PlayInEditor>     m_PIE;             // M4 S5: the PIE state machine; EditorContext.PIE refs it
        TUniquePtr<InputRoute>       m_InputRoute;      // M-Input S2: is the engine being fed; EditorContext.InputRoute refs it
        TUniquePtr<EditorMapDocument> m_MapDocument;    // M5 S5: the open .opaaxmap; EditorContext.MapDocument refs it

        // WM1a: the open `.opaaxlevel` — a session holds a LEVEL, and the map above is one of its
        // maps. The manifest itself is the world Level's, not this one's.
        TUniquePtr<EditorLevelDocument> m_LevelDocument;

        // M5 S5: when the derived dirty answers were last re-taken. Only the CLOCK is here — the
        // answers themselves live in EditorLevelDocument, beside the baselines they come from.
        double m_LastDirtyCheck = -1.0;
        TUniquePtr<EditorContext>    m_Context;
        TUniquePtr<IEditorUIBackend> m_UIBackend;

        // Every panel (native + game), built from m_Extensions.Panels() in registration order, and their
        // visibility. The Viewport is in here too, registered first — natives register before modules
        // (MR2), so its render-target handshake still cannot be reordered by what a game module adds.
        TUniquePtr<EditorPanels> m_PanelHost;

        // Every D10 route, including the menu bar itself — one owner, so what a module registers
        // into is the object that draws.
        EditorExtensionRegistrar    m_Extensions;

        // M4 S5: the WorldManager we subscribed to, so OnShutdown can unsubscribe. Non-owning, and
        // held separately from m_Context because the unsubscribe must happen BEFORE the context dies.
        WorldManager*               m_SubscribedWorlds = nullptr;
    };
}
