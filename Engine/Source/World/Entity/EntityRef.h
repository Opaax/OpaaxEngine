#pragma once

#include <nlohmann/json.hpp>

#include "Core/GUID/Guid.h"
#include "Core/GUID/GuidJson.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class Entity;
    class World;

    // =============================================================================
    // EntityRef — a field that names one entity of the same world by its guid, as Unity and Unreal
    //   reference objects: a rename, a copy or another entity of the same name never changes what it
    //   names. Saved as {"entityRef": "<guid>"}, a form every copy finds anywhere in a component (see
    //   EntityRefJson). The editor draws it as a slot an entity is dropped on.
    //   Behaviour::FindEntity (by name) stays for entities made at run time, which no file can name.
    // =============================================================================
    struct EntityRef
    {
        /** Lets the property visitor recognise the field without depending on this header. */
        static constexpr bool k_IsEntityRef = true;

        /** The entity named. Invalid: names nothing. */
        Guid Id;

        bool IsEmpty() const noexcept { return !Id.IsValid(); }

        /** The entity in InWorld, or an invalid Entity when it names nothing or that entity is gone. */
        Entity Resolve(World& InWorld) const;

        bool operator==(const EntityRef& InOther) const noexcept { return Id == InOther.Id; }
        bool operator!=(const EntityRef& InOther) const noexcept { return !(*this == InOther); }
    };

    // =============================================================================
    // EntityRefJson — the saved form, and the walk that finds every reference in saved data.
    // =============================================================================
    namespace EntityRefJson
    {
        inline constexpr const char* KEY = "entityRef";

        /** Whether InJson is a saved EntityRef. */
        inline bool IsRef(const nlohmann::json& InJson)
        {
            return InJson.is_object() && InJson.size() == 1 && InJson.contains(KEY) && InJson.at(KEY).is_string();
        }

        /**
         * Gives every EntityRef in InOutJson (any depth: nested structs, lists) the guid InMap returns for
         * its own; a ref InMap returns unchanged is left as it is.
         * @param InMap Guid(const Guid&)
         * @return How many references changed
         */
        template<typename TMap>
        Uint64 Remap(nlohmann::json& InOutJson, TMap&& InMap)
        {
            if (IsRef(InOutJson))
            {
                Guid lOld;
                from_json(InOutJson.at(KEY), lOld);
                if (!lOld.IsValid()) { return 0; }

                const Guid lNew = InMap(lOld);
                if (lNew == lOld) { return 0; }

                InOutJson[KEY] = lNew.ToString().CStr();
                return 1;
            }

            Uint64 lChanged = 0;
            if (InOutJson.is_object() || InOutJson.is_array())
            {
                for (nlohmann::json& lChild : InOutJson)
                {
                    lChanged += Remap(lChild, InMap);
                }
            }
            return lChanged;
        }
    }

    inline void to_json(nlohmann::json& InJson, const EntityRef& InValue)
    {
        InJson = nlohmann::json::object();
        InJson[EntityRefJson::KEY] = InValue.IsEmpty() ? "" : InValue.Id.ToString().CStr();
    }

    /** Reads the saved form, or a bare guid string written by hand. Anything else names nothing. */
    inline void from_json(const nlohmann::json& InJson, EntityRef& InValue)
    {
        InValue = EntityRef{};

        const nlohmann::json* lText = EntityRefJson::IsRef(InJson) ? &InJson.at(EntityRefJson::KEY)
                                    : InJson.is_string()           ? &InJson
                                                                    : nullptr;
        if (lText != nullptr)
        {
            from_json(*lText, InValue.Id);
        }
    }
}
