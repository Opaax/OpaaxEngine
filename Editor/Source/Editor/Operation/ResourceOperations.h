#pragma once

#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxString.hpp"

#include "Editor/EditorContext.h"
#include "Editor/Resources/EditorResourceEvents.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogResourceOps{"ResourceOps"};

    // =============================================================================
    // ResourceOps — what happens AFTER a document writes its file.
    //
    //   EVERY DOCUMENT'S SAVE ENDED IN THE SAME HAND-WRITTEN LINE — `Resources.Reload<T>(path)` —
    //   eight times, in eight `*Operations.cpp` files, and not one of them told anything else. That
    //   duplication is the whole reason this exists: the reload is only HALF of what "saved" means,
    //   and the other half had nowhere to live.
    //
    //   For a texture, a sheet or a clip the reload IS enough, because `ResourceManager::Reload`
    //   swaps the payload in place and every live `ResourceRef` resolves to the new one. A PREFAB is
    //   the exception, and it is what forced the announce: its instances were MATERIALIZED into
    //   entities, so replacing the resource changes nothing already in the world.
    // =============================================================================
    namespace ResourceOps
    {
        /**
         * Announce, reload, announce — the whole of "this document was saved".
         *
         * Call it INSTEAD of `Resources.Reload<T>(...)`, after the file is written. The order is
         * the contract, not an implementation detail:
         *   1. `OnResourceSaving` — listeners that need the OLD payload read it here, while it is
         *      still resident (a prefab's instances compute their overrides against it).
         *   2. `Reload<T>` — the resident copy becomes the file's contents.
         *   3. `OnResourceSaved` — listeners rebuild against the new one.
         *
         * NOT RESIDENT IS NOT A FAILURE (`Reload`'s own rule): it means nobody was holding the
         * resource, so there is nothing to swap. The events fire ANYWAY — a listener may care about
         * the file having changed even when no `ResourceRef` existed, which is exactly a prefab
         * whose instances are entities rather than refs.
         *
         * @param InAbsPath The file just written, absolute — what the resource layer is keyed by.
         * @return what `Reload` answered: true when a resident copy was actually replaced.
         */
        template<CResource T>
        bool SavedToDisk(EditorContext& InContext, const OpaaxString& InAbsPath)
        {
            ResourceSavedEvent lEvent;
            lEvent.TypeId    = ResourceTypeID::Get<T>();
            lEvent.AbsPath   = InAbsPath;
            lEvent.AssetPath = InContext.Paths.AbsoluteToAsset(InAbsPath);

            InContext.ResourceEvents.OnResourceSaving.Broadcast(lEvent);

            const bool lSwapped = InContext.Resources.Reload<T>(InAbsPath.CStr());

            InContext.ResourceEvents.OnResourceSaved.Broadcast(lEvent);

            return lSwapped;
        }
    }
}
