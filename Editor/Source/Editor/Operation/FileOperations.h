#pragma once

#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // FileOps — actions on content files as files (the Resource Browser's). Not undone: a file written is
    //   kept, as Create Prefab's is.
    // =============================================================================
    namespace FileOps
    {
        /**
         * Copies the file at InAbsPath, as saved, beside it under the next free name ("Grunt 1.opaaxasset").
         * A map gets its own map id and new entity guids (prefab placements included, references among its
         * entities re-pointed): a copy keeping them would have its entities dropped, or collide, in a level
         * holding both.
         * @param OutError Why nothing was written
         * @return The copy's absolute path, or empty
         */
        OpaaxString Duplicate(EditorContext& InContext, const OpaaxString& InAbsPath, OpaaxString* OutError = nullptr);
    }
}
