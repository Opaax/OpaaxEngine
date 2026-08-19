#pragma once

#include "Core/Tag/OpaaxTag.h"

namespace Opaax::Editor::Tags
{
    // The editor's own command tags. `inline` on purpose: a namespace-scope `const` has INTERNAL
    // linkage, so every including TU would build — and intern — its own copy (I14's ctor interns).

    //Miscs
    inline const OpaaxTag EDITOR_COMMAND_QUIT = OpaaxTag("Editor.Command.Quit");

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
}
