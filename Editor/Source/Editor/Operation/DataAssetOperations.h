#pragma once

#include <nlohmann/json.hpp>

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // DataAssetOps — the data asset editor's actions.
    // =============================================================================
    namespace DataAssetOps
    {
        /**
         * Records one undo step for an edit gesture that just ended (the value is already changed).
         * @param InBefore The value when the gesture began
         * @return False if nothing actually changed
         */
        bool CommitEdit(EditorContext& InContext, const nlohmann::json& InBefore);

        /** Writes the open asset and reloads it, so a running game reads the new values. */
        bool Save(EditorContext& InContext);
    }
}
