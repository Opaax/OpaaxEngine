#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    inline constexpr LogCategory LogPrefabOverrides{"PrefabOverrides"};

    // =============================================================================
    // PrefabOverrides — what one instance entity changed from its template, per property
    //   (so a later prefab change still reaches the untouched properties).
    //   Uses JSON merge patch (RFC 7386). Limits: an array is replaced whole, and null means remove.
    //   Pure data: no World, no registry.
    // =============================================================================
    namespace PrefabOverrides
    {
        // ---- keys -----------------------------------------------------------
        // The patch is an object with two optional parts (components, entity fields).
        inline constexpr const char* KEY_COMPONENTS = "components";
        inline constexpr const char* KEY_NAME       = "name";
        inline constexpr const char* KEY_PARENT     = "parent";   // "" = detached

        /**
         * The patch that turns InTemplate into InInstance:
         *   - component on both, identical   -> absent
         *   - component on both, different   -> the changed properties
         *   - component only on the instance -> its full payload
         *   - component only on the template -> null
         * Name and Parent are included when changed.
         * InTemplate is the entity as built for this placement (derived guids).
         * @param InIgnore Component to skip (PrefabInstanceComponent's name)
         * @return An empty object when nothing differs
         */
        OPAAX_API nlohmann::json Diff(const EntityData& InTemplate, const EntityData& InInstance,
                                      OpaaxStringID InIgnore);

        /**
         * Applies InPatch to InOutEntity (a copy of the template). Inverse of Diff.
         * An unknown component is added; null removes; a malformed patch is ignored with a warning.
         */
        OPAAX_API void Apply(const nlohmann::json& InPatch, EntityData& InOutEntity);

        /** @return True if InPatch changes nothing */
        OPAAX_API bool IsEmpty(const nlohmann::json& InPatch);
    }
}
