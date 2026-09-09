#pragma once

#include "Core/Events/Delegate.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // ResourceSavedEvent — a document wrote its file and the resident copy is being replaced.
    //
    //   Carries BOTH forms of the path because its two audiences want different ones: the resource
    //   layer is keyed by ABSOLUTE path, and everything that stores a reference — a component, a
    //   map's instance record — stores the ASSET-relative one (**MP8**).
    // =============================================================================
    struct ResourceSavedEvent
    {
        /** `ResourceTypeID::Get<T>()` — which type was written. */
        Uint32      TypeId = 0;

        OpaaxString AbsPath;
        OpaaxString AssetPath;
    };

    // =============================================================================
    // EditorResourceEvents — "a document was saved", announced in TWO PHASES.
    //
    //   EDITOR-SIDE, and deliberately not on the engine bus. A shipped game never re-reads a
    //   resource it is already running, so nothing below the editor has a use for this — and
    //   `ResourceManager`'s surface is frozen against exactly this kind of growth, its own header
    //   saying every future capability is a separate system CONSUMING it. Tier-2 delegates rather
    //   than the bus because that is what the editor already binds (`OnActiveWorldChanged`).
    //
    //   WHY TWO PHASES, and it is not symmetry for its own sake. A prefab's instances keep their
    //   overrides across an edit, and an override is only computable against the template the
    //   instance was BUILT from — which stops existing the moment `Reload` swaps the payload. So a
    //   listener that needs the old data has to run before it goes:
    //
    //     OnSaving  — the file is written, the OLD payload is STILL RESIDENT. Read it now.
    //     OnSaved   — the reload has happened and the NEW payload is live.
    //
    //   This is **LC**'s TearDown rule one layer over ("everything still alive" vs "it is being
    //   replaced"), and the same shape `WorldDestroying`/`WorldCreated` already uses.
    //
    //   Both are broadcast SYNCHRONOUSLY inside one `ResourceOps::SavedToDisk` call, so a listener
    //   may hold state between them without a lifetime question.
    // =============================================================================
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnResourceSaving, const ResourceSavedEvent&)
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnResourceSaved, const ResourceSavedEvent&)

    class EditorResourceEvents
    {
    public:
        /** The file is written; the OLD resident payload is still there. */
        FOnResourceSaving OnResourceSaving;

        /** The resident payload is now the new one. */
        FOnResourceSaved  OnResourceSaved;
    };
}
