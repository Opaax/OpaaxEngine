#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/Commands/EditorCommandConcept.h"   // NoParams
#include "Editor/Operation/EntityOps.h"             // TransformDelta — a command's Params, verbatim

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // The editor's NATIVE commands — the author loop, one struct per verb.
    //
    //   Each satisfies EditorCommand<EditorContext>: a Params type and
    //   `void Execute(EditorContext&, const Params&)`. They are registered under the tags in
    //   EditorNativeCommandsTags.hpp and invoked BY TAG from the menu bar, the Resource Browser
    //   and Ctrl+S — the same route a game module's command travels, with no privileged path
    //   (D10). Nothing here knows EditorService exists.
    //
    //   A command's whole input is the context it is handed plus its params (D3): the locator is
    //   not available to it, and anything else it needs — the window QuitCommand closes — is
    //   resolved by the composition root and passed IN.
    //
    //   Stateless and default-constructible: the registry builds one per dispatch.
    // =============================================================================

    // =============================================================================
    // Params
    // =============================================================================

    /** An ABSOLUTE path to a `.opaaxmap`. Distinct from LevelPathParams so the dispatch check separates them. */
    struct MapPathParams
    {
        OpaaxString AbsPath;
    };

    /** An ABSOLUTE path to a `.opaaxlevel`. */
    struct LevelPathParams
    {
        OpaaxString AbsPath;
    };

    /** An ABSOLUTE path to a `.opaaxprefab`. Its own type for MapPathParams' reason. */
    struct PrefabPathParams
    {
        OpaaxString AbsPath;
    };

    /** Which panel a panel verb acts on — the id from its PanelDesc. */
    struct PanelIdParams
    {
        OpaaxStringID PanelId;
    };

    /**
     * WHICH MAP a verb authors into.
     *
     * An INVALID id means "the focused map" — the only thing a menu entry can name, since it has no
     * click target to take one from. That defaulting is safe here in a way **MP10** forbids for
     * capture: there, invalid meant "no filter, the whole world" and silently widened a query; here
     * it names the same map the document already points at, and the verb refuses when there is none.
     */
    struct MapIdParams
    {
        MapId Map;
    };

    /** A new name for the primary selection. */
    struct EntityNameParams
    {
        OpaaxString Name;
    };

    /**
     * Which component type a verb acts on — its AUTHORING name.
     *
     * The interned id rather than the registry entry it names, for PanelIdParams' reason: plain data
     * is what a key binding could also carry, and a pointer into a sealed registry is not.
     */
    struct ComponentTypeParams
    {
        OpaaxStringID TypeName;
    };

    // =============================================================================
    // App
    // =============================================================================

    /**
     * Close the editor, through Window::RequestClose — the same path as clicking the X, so there
     * is one close path rather than a second one to keep correct.
     *
     * Takes NO params: the window comes off the context. A command whose payload only the
     * composition root can supply is a command nothing but a menu can invoke, since a key binding
     * carries a tag and nothing else.
     */
    struct QuitCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Send the window to the taskbar — the title bar's first caption button.
     *
     * A command rather than a direct Window call for QuitCommand's reason one button over: the
     * caption and any later key binding then reach one verb. The window comes off the context.
     */
    struct MinimizeWindowCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Maximize the window, or restore it when it already is.
     *
     * ONE toggling verb rather than two, because the caption button is one button whose glyph
     * follows the state, and a double-click on the bar means exactly the same thing.
     */
    struct ToggleMaximizeWindowCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Show or hide one panel — the verb behind every entry in the Window menu, and behind the
     * window's own close button by way of the same bool.
     *
     * ONE command for every panel, because the panel is the PAYLOAD rather than the identity: an
     * interned id is plain data, so the entry that carries it is the entry a key binding could
     * carry too. A tag per panel would have needed a registration-time tag, and panel ids have
     * spaces, which OpaaxTag refuses (I14).
     */
    struct TogglePanelCommand
    {
        using Params = PanelIdParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    // =============================================================================
    // Undo (⑤)
    //
    //   These two record nothing, and nothing here has to say so: a command is not an undo step
    //   (**UN1**), and EditorUndo::Record is a no-op while a step is replaying anyway.
    //
    //   THE PIE GATE LIVES IN THESE BODIES, not in the stack. EditorUndo deliberately knows nothing
    //   about worlds, so MapOps::CanEdit is asked here, where every other editor policy is.
    // =============================================================================

    /** Put the last recorded step back. Refused while PIE runs. */
    struct UndoCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Re-assert the step an Undo took off. NEVER a second Execute — see **UN4**. */
    struct RedoCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Play in editor
    //
    //   PIE's four verbs as commands, so the THREE front-ends that drive them — the toolbar's
    //   buttons, the reserved F-keys and now the Play menu — all make the same call instead of
    //   three copies of it. They add no state: each forwards to the one PlayInEditor the context
    //   already carries, which is what a toolbar button has always done directly.
    //
    //   This is also what makes them bindable. A key binding carries a tag, so a verb that is not
    //   a command cannot be rebound; PlayInEditor::Play() was reachable only by hard-coding F5.
    // =============================================================================

    /** Clone the edit world and run it. Refused unless the state is Edit (PlayInEditor logs it). */
    struct PlayCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Pause if playing, resume if paused — the one verb a single key or menu entry can carry. */
    struct TogglePauseCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Tick exactly one frame, then stay paused. Refused unless Paused. */
    struct StepCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Discard the Play clone and put the edit world back. Refused from Edit. */
    struct StopCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Entity
    //
    //   The author loop's own three verbs (②). Bodies live in EntityOps — the ONE named mutation
    //   choke point ⑤ needs — so the Edit menu, the Hierarchy's context menus and the viewport's
    //   keys all reach one implementation.
    // =============================================================================

    /**
     * Create an empty entity in the FOCUSED map and select it.
     *
     * The focused map is the target because a menu entry has nothing else to name; the Hierarchy's
     * header menu is the route when you want to say which map, and it calls EntityOps directly for
     * the same reason MapOps' verbs take theirs.
     */
    struct CreateEntityCommand
    {
        using Params = MapIdParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Destroy everything selected. Refused while PIE runs. */
    struct DeleteSelectedCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Rename the PRIMARY selection — the Inspector's name field, committed on Enter or focus loss. */
    struct RenameSelectedCommand
    {
        using Params = EntityNameParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Put a component on the primary selection — the Inspector's Add popup. */
    struct AddComponentCommand
    {
        using Params = ComponentTypeParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Take one off it — the Remove popup. An essential type is refused by the registry entry. */
    struct RemoveComponentCommand
    {
        using Params = ComponentTypeParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /**
     * Apply one drag frame to the selection — the gizmo's mutation.
     *
     * THE PARAMS ARE EntityOps::TransformDelta ITSELF, not a wrapper around it: that struct was
     * already "one answer to what the gizmo just did", and its own header called the call
     * REPLAYABLE. Being a command is what makes that literal.
     *
     * Dispatched every frame a drag moves, and it records nothing: ⑤'s step for a drag is ONE
     * EntityTransform built by the viewport from the transforms it cached at either end.
     */
    struct TransformSelectedCommand
    {
        using Params = EntityOps::TransformDelta;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Frame the selection with the editor camera. Refused outside Edit — see EntityOps. */
    struct FocusSelectedCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Gizmo (③)
    //
    // Three commands rather than one taking a mode, because a KEY BINDING CARRIES A TAG AND NO
    // PAYLOAD — the same constraint that removed QuitParams. W/E/R is the binding every reference
    // editor uses, so each mode needs a tag a shortcut can name.
    //
    // Setting a mode is not an edit, so none of them gates on CanEdit: the gizmo simply does not
    // draw in a Play world, and switching modes while it is hidden is harmless.
    // =============================================================================

    /** Move the selection. The default mode. */
    struct GizmoTranslateCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Turn the selection about the gizmo's pivot. */
    struct GizmoRotateCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Scale the selection about the gizmo's pivot — writes TransformComponent::Scale. */
    struct GizmoScaleCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Map
    // =============================================================================

    /**
     * Create an EMPTY `.opaaxmap`, add it to the open level, and focus it.
     *
     * The file is written IMMEDIATELY rather than held as an unsaved cursor: everything below
     * assumes a map has a file, and a "not on disk yet" state would be the only one of its kind
     * in the editor. It carries its own `mapId` from the first byte (**MP10**), which is what
     * makes a map with no entities an ordinary map instead of an anonymous one.
     *
     * Refuses a path that already exists — Open Map and Add Map are the verbs for those.
     */
    struct NewMapCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Ask for a `.opaaxmap`, then OpenMapAtCommand. */
    struct OpenMapCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Edit the map at Params::AbsPath — the shared body behind both the File menu's "Open Map..."
     * and a double-click in the Resource Browser, so the two cannot diverge on the parts that
     * matter (the PIE guard, the unsaved-changes prompt).
     *
     * TWO OUTCOMES, decided by whether that map is already in the world. Every map of the open
     * level is mounted, so one of them LOADS NOTHING: it re-targets the document, and the
     * selection survives because its entities do. A map belonging to no open level gets its OWN
     * world with an empty Level instead of being merged into someone else's.
     */
    struct OpenMapAtCommand
    {
        using Params = MapPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Write the FOCUSED map. Save Level is what writes them all (**MP9**). */
    struct SaveMapCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Ask where, write the focused map there, and move the cursor onto the file it just wrote. */
    struct SaveMapAsCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    // =============================================================================
    // Prefab (⑦-C)
    // =============================================================================

    /**
     * Place one instance of the prefab at Params::AbsPath into the FOCUSED map.
     *
     * A thin dispatch onto `EntityOps::InstantiatePrefab`, which is where the policy lives — the
     * PIE guard, the map requirement, the selection and the undo step. The browser's double-click
     * is its only front-end today; a Hierarchy entry and a viewport drop are the two that come
     * next, and neither needs anything new here.
     */
    struct InstantiatePrefabAtCommand
    {
        using Params = PrefabPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /**
     * Ask where, write the SELECTION there as a prefab, and replace it with an instance.
     *
     * NoParams: the subject is the selection, which the context already holds — the same reason
     * `DeleteSelected` takes none. The dialog defaults to the primary entity's name, so the common
     * case is type-nothing-and-confirm.
     */
    struct CreatePrefabFromSelectionCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Which entities a revert covers — plain data, so the entry stays bindable (**MR2c**). */
    struct PrefabRevertParams
    {
        /** false: exactly what is selected. true: every entity of the instances it touches. */
        bool bWholeInstance = false;
    };

    /**
     * Put the selected instance entities back to their prefab's values.
     *
     * A thin dispatch onto `EntityOps::RevertToPrefab`. ONE command with a payload rather than two
     * tags, for the reason `TogglePanel` is one: the verb is the same and only its scope differs.
     */
    struct RevertToPrefabCommand
    {
        using Params = PrefabRevertParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    // =============================================================================
    // Level
    // =============================================================================

    /** Ask for a `.opaaxlevel`, then OpenLevelAtCommand. */
    struct OpenLevelCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Open a `.opaaxlevel` into a NEW world — the browser's double-click and File/Open Level.
     * A whole new world, because the level names which maps exist in it: that is not something
     * the current world can be edited into.
     */
    struct OpenLevelAtCommand
    {
        using Params = LevelPathParams;

        void Execute(EditorContext& InContext, const Params& InParams);
    };

    /** Write the manifest AND every map it mounts (**MP9**). */
    struct SaveLevelCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Write the open `.opaaxsheet` back to its file.
     *
     * A COMMAND rather than a call from the panel's button, because every other Save in the editor
     * is one: it is the route the panel's button AND Ctrl+S both take, which is what keeps them
     * from being two implementations of saving.
     *
     * Ctrl+S reaches it when the sheet panel has focus, and Save Map otherwise — the routing lives
     * in EditorService::HandleAuthoringShortcuts, since one chord cannot mean two things without
     * asking which document is in front.
     */
    struct SaveSheetCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * Write the open `.opaaxclip` and publish it, so an entity already playing it picks the change
     * up — SaveSheetCommand's shape, one asset over.
     *
     * Ctrl+S reaches it when the clip panel has focus, ahead of the sheet and the map, for the same
     * reason: one chord cannot mean three things without asking which document is in front.
     */
    struct SaveClipCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Write the open `.opaaxanim` and publish it — SaveClipCommand's shape, one asset over. */
    struct SaveLibraryCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Write the open `.opaaxfont` and publish it — SaveLibraryCommand's shape, one asset over. */
    struct SaveFamilyCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Write the open `.opaaxmovemode` and publish it. */
    struct SaveMoveModeCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Write the open `.opaaxmover` and publish it — the library's shape, one family over. */
    struct SaveMoverCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Write the open `.opaaxaction` and publish it. */
    struct SaveInputActionCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /** Write the open `.opaaxinputmap` and publish it — this is what a rebind saves. */
    struct SaveInputMapCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };

    /**
     * The only Level entry on the menu bar, because it is the only one that does not need a map
     * named first — it goes and picks one. Removing a map and choosing the persistent one live on
     * the Hierarchy's map headers (MapOps), where the target is what was clicked instead of
     * whatever happened to be focused.
     */
    struct AddMapToLevelCommand
    {
        using Params = NoParams;

        void Execute(EditorContext& InContext, const Params&);
    };
}
