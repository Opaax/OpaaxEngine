#pragma once

#include "Core/Events/Delegate.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // ResourceSavedEvent — a document wrote its file. Carries the absolute path (how resources are
    //   keyed) and the asset-relative path (how components and maps store references).
    // =============================================================================
    struct ResourceSavedEvent
    {
        /** ResourceTypeID::Get<T>() of the type written. */
        Uint32      TypeId = 0;

        OpaaxString AbsPath;
        OpaaxString AssetPath;
    };

    // =============================================================================
    // EditorResourceEvents — "a document was saved", announced in two phases. Editor-only (a shipped
    //   game never reloads a resource it is running).
    //     OnSaving — the file is about to be written: the old file and the old data still exist.
    //     OnSaved  — the file is written and the resource reloaded: the new data is live.
    //   Two phases because a prefab's instances compute their overrides against the old template,
    //   which is gone once the resource reloads.
    //   Sent by ResourceOps::AboutToSave and ResourceOps::SavedToDisk, synchronously within one save.
    // =============================================================================
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnResourceSaving, const ResourceSavedEvent&)
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnResourceSaved, const ResourceSavedEvent&)

    class EditorResourceEvents
    {
    public:
        /** The file is about to be written; the previous state is still readable. */
        FOnResourceSaving OnResourceSaving;

        /** The loaded data is now the new one. */
        FOnResourceSaved  OnResourceSaved;
    };
}
