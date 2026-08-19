#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Editor/Commands/EditorCommandConcept.h"   // NoParams

namespace Opaax
{
    class Window;
}

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

    /** The window to close. Resolved by the composition root, because a command cannot ask for one. */
    struct QuitParams
    {
        Window* Target = nullptr;
    };

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

    // =============================================================================
    // App
    // =============================================================================

    /**
     * Close the editor, through Window::RequestClose — the same path as clicking the X, so there
     * is one close path rather than a second one to keep correct.
     */
    struct QuitCommand
    {
        using Params = QuitParams;

        void Execute(EditorContext& InContext, const Params& InParams);
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
