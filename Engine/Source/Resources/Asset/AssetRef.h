#pragma once

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax
{
    // =============================================================================
    // TAssetRef<T> — a field that names an .opaaxasset holding a T (or a subclass of T). Saved as its
    //   path. The editor draws it as a drop target that refuses assets of other classes.
    //   Load it with LoadAsset (AssetHandle.h).
    // =============================================================================
    template<typename T>
    struct TAssetRef
    {
        using AssetType = T;

        /** Lets the property visitor recognise the field without depending on this header. */
        static constexpr bool k_IsAssetRef = true;

        /** Asset-relative ("Data/Grunt.opaaxasset") or a mount ("/Engine/..."). Empty: names nothing. */
        OpaaxString Path;

        bool IsEmpty() const noexcept { return Path.IsEmpty(); }

        bool operator==(const TAssetRef& InOther) const noexcept { return Path == InOther.Path; }
        bool operator!=(const TAssetRef& InOther) const noexcept { return !(*this == InOther); }
    };

    template<typename T>
    void to_json(nlohmann::json& InJson, const TAssetRef<T>& InValue)
    {
        InJson = InValue.Path;
    }

    template<typename T>
    void from_json(const nlohmann::json& InJson, TAssetRef<T>& InValue)
    {
        InJson.get_to(InValue.Path);
    }
}
