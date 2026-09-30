#pragma once

#include "Core/OpaaxTypes.h"
#include <type_traits>

#include "Core/String/OpaaxString.hpp"

// =============================================================================
// TResourcePath<T> — asset-relative path to a resource file, typed by the resource that loads it.
//   The type lets the editor refuse a wrong file in a drop target. T only needs a forward declaration.
//   Always asset-relative ("Textures/Hero.png") or a mount ("/Engine/Textures/T_Checker_64.png").
// =============================================================================
namespace Opaax
{
    // =============================================================================
    // EResourceLoad — when the referenced resource is loaded (like Unreal's TObjectPtr / TWeakObjectPtr).
    //   Part of the type, not saved in the file.
    // =============================================================================
    enum class EResourceLoad : Uint8
    {
        /**
         * Loaded when first resolved. The default. The only option for a prefab that references itself.
         */
        Soft,

        /**
         * Loaded with the object that holds it, and kept while it lives (e.g. a gun's bullet prefab,
         * so the first shot does not stall). Cycles are refused.
         */
        Hard,
    };

    template<typename TResource, EResourceLoad TLoad = EResourceLoad::Soft>
    struct TResourcePath
    {
        using ResourceType = TResource;

        /** When the resource is loaded. See EResourceLoad. */
        static constexpr EResourceLoad LoadPolicy = TLoad;

        OpaaxString Path;

        /** Empty means no resource set (not an error). */
        bool IsEmpty() const noexcept { return Path.IsEmpty(); }

        bool operator==(const TResourcePath& InOther) const noexcept { return Path == InOther.Path; }
        bool operator!=(const TResourcePath& InOther) const noexcept { return !(*this == InOther); }
    };

    /**
     * A reference loaded with its holder: TResourcePath<T, Hard>.
     *
     *     TResourcePath<TextureResource>     Texture;   // loaded when drawn
     *     THardResourcePath<PrefabResource>  Bullet;    // loaded before the gun fires
     */
    template<typename TResource>
    using THardResourcePath = TResourcePath<TResource, EResourceLoad::Hard>;

    /**
     * True if T is a hard resource reference.
     */
    template<typename T>
    struct TIsHardResourcePath : std::false_type {};

    template<typename TResource>
    struct TIsHardResourcePath<TResourcePath<TResource, EResourceLoad::Hard>> : std::true_type {};

    template<typename T>
    inline constexpr bool k_IsHardResourcePath = TIsHardResourcePath<T>::value;
}
