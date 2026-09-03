#pragma once

#include "Core/Tag/OpaaxTag.h"

namespace Opaax::Editor::Tags
{
    // The editor's own command tags. `inline` on purpose: a namespace-scope `const` has INTERNAL
    // linkage, so every including TU would build — and intern — its own copy (I14's ctor interns).

    //Miscs
    inline const OpaaxTag EDITOR_COMMAND_QUIT = OpaaxTag("Editor.Command.Quit");

    //Window — the title bar's buttons. Commands rather than direct Window calls so the caption and
    //a later key binding reach ONE verb, the rule every other front-end here already follows.
    //Close is EDITOR_COMMAND_QUIT: the X and File/Exit are the same verb, not two.
    inline const OpaaxTag EDITOR_COMMAND_MINIMIZE_WINDOW        = OpaaxTag("Editor.Command.MinimizeWindow");
    inline const OpaaxTag EDITOR_COMMAND_TOGGLE_MAXIMIZE_WINDOW = OpaaxTag("Editor.Command.ToggleMaximizeWindow");

    //Panels — ONE tag for every panel; which one is the PanelIdParams payload.
    inline const OpaaxTag EDITOR_COMMAND_TOGGLE_PANEL = OpaaxTag("Editor.Command.TogglePanel");

    //Play in editor
    inline const OpaaxTag EDITOR_COMMAND_PLAY         = OpaaxTag("Editor.Command.Play");
    inline const OpaaxTag EDITOR_COMMAND_TOGGLE_PAUSE = OpaaxTag("Editor.Command.TogglePause");
    inline const OpaaxTag EDITOR_COMMAND_STEP         = OpaaxTag("Editor.Command.Step");
    inline const OpaaxTag EDITOR_COMMAND_STOP         = OpaaxTag("Editor.Command.Stop");

    //Undo (⑤) — the two commands that are never themselves recorded.
    inline const OpaaxTag EDITOR_COMMAND_UNDO = OpaaxTag("Editor.Command.Undo");
    inline const OpaaxTag EDITOR_COMMAND_REDO = OpaaxTag("Editor.Command.Redo");

    //Entity — the author loop's own verbs (②). Reached from the Edit menu, the Hierarchy's context
    //menus and the viewport's keys, so all three make one call rather than three copies of it.
    inline const OpaaxTag EDITOR_COMMAND_CREATE_ENTITY  = OpaaxTag("Editor.Command.CreateEntity");
    inline const OpaaxTag EDITOR_COMMAND_DELETE_ENTITY  = OpaaxTag("Editor.Command.DeleteEntity");
    inline const OpaaxTag EDITOR_COMMAND_FOCUS_SELECTED = OpaaxTag("Editor.Command.FocusSelected");

    //The Inspector's own three, so its edits route through EntityOps like every other one. A field
    //edit has no tag: a drawer writes straight through a T& (**I15**), so the panel records the
    //step itself rather than dispatching a verb with nothing to do (⑤).
    inline const OpaaxTag EDITOR_COMMAND_RENAME_SELECTED  = OpaaxTag("Editor.Command.RenameSelected");
    inline const OpaaxTag EDITOR_COMMAND_ADD_COMPONENT    = OpaaxTag("Editor.Command.AddComponent");
    inline const OpaaxTag EDITOR_COMMAND_REMOVE_COMPONENT = OpaaxTag("Editor.Command.RemoveComponent");

    //The gizmo's MUTATION, dispatched per frame of a drag. It records nothing — the viewport builds
    //ONE EntityTransform from the transforms it cached at either end of the drag (⑤).
    inline const OpaaxTag EDITOR_COMMAND_TRANSFORM_SELECTED = OpaaxTag("Editor.Command.TransformSelected");

    //Gizmo (③) — THREE tags rather than one with a mode payload, because a key binding carries a
    //tag and no payload (the reason QuitParams died). W/E/R have to reach these directly.
    inline const OpaaxTag EDITOR_COMMAND_GIZMO_TRANSLATE = OpaaxTag("Editor.Command.GizmoTranslate");
    inline const OpaaxTag EDITOR_COMMAND_GIZMO_ROTATE    = OpaaxTag("Editor.Command.GizmoRotate");
    inline const OpaaxTag EDITOR_COMMAND_GIZMO_SCALE     = OpaaxTag("Editor.Command.GizmoScale");

    //Level
    inline const OpaaxTag EDITOR_COMMAND_NEW_LEVEL      = OpaaxTag("Editor.Command.NewLevel");
    inline const OpaaxTag EDITOR_COMMAND_OPEN_LEVEL     = OpaaxTag("Editor.Command.OpenLevel");
    inline const OpaaxTag EDITOR_COMMAND_OPEN_LEVEL_AT  = OpaaxTag("Editor.Command.OpenLevelAt");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_LEVEL     = OpaaxTag("Editor.Command.SaveLevel");
    inline const OpaaxTag EDITOR_COMMAND_ADD_MAP_TO_LEVEL = OpaaxTag("Editor.Command.AddMapToLevel");

    //Map
    inline const OpaaxTag EDITOR_COMMAND_NEW_MAP     = OpaaxTag("Editor.Command.NewMap");
    inline const OpaaxTag EDITOR_COMMAND_OPEN_MAP    = OpaaxTag("Editor.Command.OpenMap");
    inline const OpaaxTag EDITOR_COMMAND_OPEN_MAP_AT = OpaaxTag("Editor.Command.OpenMapAt");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_MAP    = OpaaxTag("Editor.Command.SaveMap");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_MAP_AS = OpaaxTag("Editor.Command.SaveMapAs");

    //Sprite sheet
    inline const OpaaxTag EDITOR_COMMAND_SAVE_SHEET  = OpaaxTag("Editor.Command.SaveSheet");

    //Animation
    inline const OpaaxTag EDITOR_COMMAND_SAVE_CLIP    = OpaaxTag("Editor.Command.SaveClip");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_LIBRARY = OpaaxTag("Editor.Command.SaveLibrary");
}
