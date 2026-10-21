#pragma once

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Templates/TypeTraits.h"
#include "Resources/ResourcePath.h"   // EResourceLoad

namespace Opaax
{
    // =============================================================================
    // TAssetRef<T> — a field that names an asset file holding a T (or a subclass of T): an .opaaxasset,
    //   or a file a class reads (a .png for a Texture2D). Saved as its path. The editor draws it as a drop
    //   target that refuses assets of other classes. Load it with LoadAsset (AssetHandle.h).
    //   TLoad says when: Soft (the default) when first loaded by code, Hard with the map that holds it.
    // =============================================================================
    template<typename T, EResourceLoad TLoad = EResourceLoad::Soft>
    struct TAssetRef
    {
        using AssetType = T;

        /** When the asset is loaded. See EResourceLoad. */
        static constexpr EResourceLoad LoadPolicy = TLoad;

        /** Lets the property visitor recognise the field without depending on this header. */
        static constexpr bool k_IsAssetRef = true;

        /** Asset-relative ("Data/Grunt.opaaxasset") or a mount ("/Engine/..."). Empty: names nothing. */
        OpaaxString Path;

        bool IsEmpty() const noexcept { return Path.IsEmpty(); }

        bool operator==(const TAssetRef& InOther) const noexcept { return Path == InOther.Path; }
        bool operator!=(const TAssetRef& InOther) const noexcept { return !(*this == InOther); }
    };

    /**
     * An asset loaded with the map that holds it, and kept while the map is mounted (a gun's bullet
     * prefab, so the first shot does not stall). Cycles are refused.
     */
    template<typename T>
    using THardAssetRef = TAssetRef<T, EResourceLoad::Hard>;

    /** True if T is a hard asset reference. */
    template<typename T>
    struct TIsHardAssetRef : FalseType {};

    template<typename T>
    struct TIsHardAssetRef<TAssetRef<T, EResourceLoad::Hard>> : TrueType {};

    template<typename T>
    inline constexpr bool k_IsHardAssetRef = TIsHardAssetRef<T>::value;

    template<typename T, EResourceLoad TLoad>
    void to_json(nlohmann::json& InJson, const TAssetRef<T, TLoad>& InValue)
    {
        InJson = InValue.Path;
    }

    template<typename T, EResourceLoad TLoad>
    void from_json(const nlohmann::json& InJson, TAssetRef<T, TLoad>& InValue)
    {
        InJson.get_to(InValue.Path);
    }
}
