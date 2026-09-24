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
    //     OnSaving  — the file is ABOUT TO BE WRITTEN. The old one is still on disk and the old
    //                 payload still resident, so this is the only moment the previous state exists.
    //     OnSaved   — the write and the reload have happened; the NEW payload is live.
    //
    //   THE BOUNDARY IS THE WRITE, NOT THE RELOAD, and getting that wrong shipped a bug the user
    //   found in a minute: announced after the file was written, every resolve returned the NEW
    //   template, so a prefab's fold recorded the author's own edit as an override and the expand
    //   cancelled it straight back out — Save appeared to do nothing at all.
    //
    //   This is **LC**'s TearDown rule one layer over ("everything still alive" vs "it is being
    //   replaced"), and the same shape `WorldDestroying`/`WorldCreated` already uses.
    //
    //   The two come from `ResourceOps::AboutToSave` and `ResourceOps::SavedToDisk`, which a saver
    //   calls either side of its write. Both run synchronously within that one save, so a listener
    //   may hold state between them without a lifetime question.
    // =============================================================================
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnResourceSaving, const ResourceSavedEvent&)
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnResourceSaved, const ResourceSavedEvent&)

    class EditorResourceEvents
    {
    public:
        /** The file is ABOUT to be written — the previous state is still readable. */
        FOnResourceSaving OnResourceSaving;

        /** The resident payload is now the new one. */
        FOnResourceSaved  OnResourceSaved;
    };
}
