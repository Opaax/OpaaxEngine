#pragma once

#include "Core/Log/Logger.h"
#include "Application/Services/IPaths.h"
#include "Core/String/OpaaxString.hpp"

#include "Editor/EditorContext.h"
#include "Editor/Resources/EditorResourceEvents.h"

#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"

namespace Opaax::Editor
{
    inline constexpr LogCategory LogResourceOps{"ResourceOps"};

    // =============================================================================
    // ResourceOps — what happens after a document writes its file: reload the resource and announce
    //   the save. A reload is enough for textures, sheets or clips (the data is swapped in place).
    //   Prefabs need the announce: their instances are entities, so a reload alone changes nothing.
    // =============================================================================
    namespace ResourceOps
    {
        /**
         * Call right before writing the file, while the old file and data still exist (a prefab's
         * instances need them to keep their overrides). Optional: most savers only call SavedToDisk.
         */
        template<CResource T>
        void AboutToSave(EditorContext& InContext, const OpaaxString& InAbsPath)
        {
            ResourceSavedEvent lEvent;
            lEvent.TypeId    = ResourceTypeID::Get<T>();
            lEvent.AbsPath   = InAbsPath;
            lEvent.AssetPath = InContext.Paths.AbsoluteToAsset(InAbsPath);

            InContext.ResourceEvents.OnResourceSaving.Broadcast(lEvent);
        }

        /**
         * Reloads the resource and announces the save. Call it after the file is written.
         * Not loaded is not a failure (nothing to swap); the event is sent anyway.
         * @param InAbsPath The file just written, absolute
         * @return True if a loaded copy was replaced
         */
        template<CResource T>
        bool SavedToDisk(EditorContext& InContext, const OpaaxString& InAbsPath)
        {
            ResourceSavedEvent lEvent;
            lEvent.TypeId    = ResourceTypeID::Get<T>();
            lEvent.AbsPath   = InAbsPath;
            lEvent.AssetPath = InContext.Paths.AbsoluteToAsset(InAbsPath);

            const bool lSwapped = InContext.Resources.Reload<T>(InAbsPath.CStr());

            InContext.ResourceEvents.OnResourceSaved.Broadcast(lEvent);

            return lSwapped;
        }
    }
}
