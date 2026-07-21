#pragma once

#include "Editor/EditorPaths.h"

namespace Opaax::Editor
{
    void EditorPaths::LogPaths() const
    {
        Paths::LogPaths();
        
        //Todo: Log path to editor relatif, Projet/Editor/*.....
    }
}
