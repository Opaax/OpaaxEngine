#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxStringView.hpp"
#include "Core/Tag/OpaaxTag.h"
#include "Editor/EditorContext.h"

namespace Opaax::Editor::Tags
{
    //Miscs
    const OpaaxTag EDITOR_COMMAND_QUIT = OpaaxTag(OpaaxStringView("Editor.Command.Quit"));

    //Level
    const OpaaxTag EDITOR_COMMAND_NEW_LEVEL = OpaaxTag(OpaaxStringView("Editor.Command.NewLevel"));

    //Map
    const OpaaxTag EDITOR_COMMAND_NEW_MAP = OpaaxTag(OpaaxStringView("Editor.Command.NewMap"));
}

namespace Opaax::Editor
{
    struct FNewMapParams
    {
        OpaaxStringView Name;
    };

    struct FNewMapCommand
    {
        using Params = FNewMapParams;

        void Execute(
            EditorContext& InContext,
            const Params& InParams)
        {
            OPAAX_APP_LOG(Info, "Test New Map Command");
        }
    };
}
