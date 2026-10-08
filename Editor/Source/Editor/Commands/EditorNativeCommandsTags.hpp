#pragma once

#include "Core/Tag/OpaaxTag.h"

namespace Opaax::Editor::Tags
{
    // The editor's own command tags. inline: a namespace-scope const would give every TU its own copy.

    //Miscs
    inline const OpaaxTag EDITOR_COMMAND_QUIT = OpaaxTag("Editor.Command.Quit");

    // Export
    inline const OpaaxTag EDITOR_COMMAND_EXPORT_GAME = OpaaxTag("Editor.Command.ExportGame");

    //Window — the title bar buttons. Close is EDITOR_COMMAND_QUIT (the X and File/Exit are the same).
    inline const OpaaxTag EDITOR_COMMAND_MINIMIZE_WINDOW        = OpaaxTag("Editor.Command.MinimizeWindow");
    inline const OpaaxTag EDITOR_COMMAND_TOGGLE_MAXIMIZE_WINDOW = OpaaxTag("Editor.Command.ToggleMaximizeWindow");

    //Panels — one tag for every panel; the panel is the PanelIdParams payload.
    inline const OpaaxTag EDITOR_COMMAND_TOGGLE_PANEL = OpaaxTag("Editor.Command.TogglePanel");

    //Play in editor
    inline const OpaaxTag EDITOR_COMMAND_PLAY         = OpaaxTag("Editor.Command.Play");
    inline const OpaaxTag EDITOR_COMMAND_TOGGLE_PAUSE = OpaaxTag("Editor.Command.TogglePause");
    inline const OpaaxTag EDITOR_COMMAND_STEP         = OpaaxTag("Editor.Command.Step");
    inline const OpaaxTag EDITOR_COMMAND_STOP         = OpaaxTag("Editor.Command.Stop");

    //Undo
    inline const OpaaxTag EDITOR_COMMAND_UNDO = OpaaxTag("Editor.Command.Undo");
    inline const OpaaxTag EDITOR_COMMAND_REDO = OpaaxTag("Editor.Command.Redo");

    //Entity
    inline const OpaaxTag EDITOR_COMMAND_CREATE_ENTITY  = OpaaxTag("Editor.Command.CreateEntity");
    inline const OpaaxTag EDITOR_COMMAND_DELETE_ENTITY  = OpaaxTag("Editor.Command.DeleteEntity");
    inline const OpaaxTag EDITOR_COMMAND_FOCUS_SELECTED = OpaaxTag("Editor.Command.FocusSelected");

    //The Inspector's three. Field edits have no tag: the panel records those steps itself.
    inline const OpaaxTag EDITOR_COMMAND_RENAME_SELECTED  = OpaaxTag("Editor.Command.RenameSelected");
    inline const OpaaxTag EDITOR_COMMAND_ADD_COMPONENT    = OpaaxTag("Editor.Command.AddComponent");
    inline const OpaaxTag EDITOR_COMMAND_REMOVE_COMPONENT = OpaaxTag("Editor.Command.RemoveComponent");

    //The gizmo's per-frame drag. Records nothing (the viewport records one step per drag).
    inline const OpaaxTag EDITOR_COMMAND_TRANSFORM_SELECTED = OpaaxTag("Editor.Command.TransformSelected");

    //Gizmo modes, one tag each so W/E/R can reach them.
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

    //Prefab
    inline const OpaaxTag EDITOR_COMMAND_INSTANTIATE_PREFAB_AT =
        OpaaxTag("Editor.Command.InstantiatePrefabAt");
    inline const OpaaxTag EDITOR_COMMAND_CREATE_PREFAB_FROM_SELECTION =
        OpaaxTag("Editor.Command.CreatePrefabFromSelection");
    inline const OpaaxTag EDITOR_COMMAND_REVERT_TO_PREFAB =
        OpaaxTag("Editor.Command.RevertToPrefab");
    inline const OpaaxTag EDITOR_COMMAND_OPEN_PREFAB_AT =
        OpaaxTag("Editor.Command.OpenPrefabAt");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_PREFAB =
        OpaaxTag("Editor.Command.SavePrefab");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_PREFAB_AS_VARIANT =
        OpaaxTag("Editor.Command.SavePrefabAsVariant");
    inline const OpaaxTag EDITOR_COMMAND_UNDO_PREFAB =
        OpaaxTag("Editor.Command.UndoPrefab");
    inline const OpaaxTag EDITOR_COMMAND_REDO_PREFAB =
        OpaaxTag("Editor.Command.RedoPrefab");
    inline const OpaaxTag EDITOR_COMMAND_DELETE_PREFAB_SELECTION =
        OpaaxTag("Editor.Command.DeletePrefabSelection");

    //Sprite sheet
    inline const OpaaxTag EDITOR_COMMAND_SAVE_SHEET  = OpaaxTag("Editor.Command.SaveSheet");

    //Animation
    inline const OpaaxTag EDITOR_COMMAND_SAVE_CLIP    = OpaaxTag("Editor.Command.SaveClip");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_LIBRARY = OpaaxTag("Editor.Command.SaveLibrary");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_FAMILY  = OpaaxTag("Editor.Command.SaveFamily");

    //UI
    inline const OpaaxTag EDITOR_COMMAND_NEW_UI           = OpaaxTag("Editor.Command.NewUI");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_UI          = OpaaxTag("Editor.Command.SaveUI");
    inline const OpaaxTag EDITOR_COMMAND_DELETE_UI_WIDGET = OpaaxTag("Editor.Command.DeleteUIWidget");

    //Mover
    inline const OpaaxTag EDITOR_COMMAND_SAVE_MOVE_MODE = OpaaxTag("Editor.Command.SaveMoveMode");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_MOVER     = OpaaxTag("Editor.Command.SaveMover");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_DATA_ASSET = OpaaxTag("Editor.Command.SaveDataAsset");

    // Input: an action and its mapping context are separate assets, saved separately.
    inline const OpaaxTag EDITOR_COMMAND_SAVE_INPUT_ACTION = OpaaxTag("Editor.Command.SaveInputAction");
    inline const OpaaxTag EDITOR_COMMAND_SAVE_INPUT_MAP    = OpaaxTag("Editor.Command.SaveInputMap");
}
