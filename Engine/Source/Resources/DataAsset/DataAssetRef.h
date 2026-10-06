#pragma once

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Resources/DataAsset/DataAsset.h"

namespace Opaax
{
    // =============================================================================
    // TDataAssetRef<T> — a field that points at a .opaaxdata holding a T. Saved as its path. The
    //   editor draws it as a drop target that accepts only data assets of type T.
    //   Load it with LoadDataAsset (DataAssetHandle.h).
    // =============================================================================
    template<CDataAsset T>
    struct TDataAssetRef
    {
        using DataType = T;

        /** Lets the property visitor recognise the field without depending on this header. */
        static constexpr bool k_IsDataAssetRef = true;

        /** Asset-relative ("Data/Grunt.opaaxdata"). Empty: points at nothing. */
        OpaaxString Path;

        bool IsEmpty() const noexcept { return Path.IsEmpty(); }
    };

    template<CDataAsset T>
    void to_json(nlohmann::json& InJson, const TDataAssetRef<T>& InValue)
    {
        InJson = InValue.Path;
    }

    template<CDataAsset T>
    void from_json(const nlohmann::json& InJson, TDataAssetRef<T>& InValue)
    {
        InJson.get_to(InValue.Path);
    }
}
