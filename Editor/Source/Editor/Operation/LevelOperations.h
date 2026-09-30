#pragma once

#include "Core/OpaaxTypes.h"            // TFunction
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // LevelOps — actions on the open level as a whole (MapOps does the same for one map).
    //   Not user commands: AdoptOpen is boot plumbing, ConfirmDiscardingEdits asks a question.
    // =============================================================================
    namespace LevelOps
    {
        /**
         * Adopts the active world's Level as the open document, and one of its loaded maps for editing.
         * Shared by boot, Open Level and Open Map. The first non-persistent map is edited (the persistent
         * one only if it is alone). Only loaded maps count: a map that failed to load is not editable.
         * @param InLevelAbsPath The .opaaxlevel file, or empty for a standalone map (it still gets a
         *   record, so Save Map works without a level)
         */
        void AdoptOpen(EditorContext& InContext, const OpaaxString& InLevelAbsPath);

        /**
         * Asks before an action that destroys the world, when the level has unsaved work (Save Level
         * writes all of it). Modal, because the action cannot be undone. Not needed to change the focused
         * map: baselines are per map.
         * @param InOnConfirmed Runs when it is safe to go on (the user said yes, or nothing was at risk).
         *   Not run on a refusal. A callback rather than a bool because the dialog answer may arrive later.
         */
        void ConfirmDiscardingEdits(EditorContext& InContext, TFunction<void()> InOnConfirmed);
    }
}
